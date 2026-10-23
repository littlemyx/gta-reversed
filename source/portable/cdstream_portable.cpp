// Portable (non-Windows) CdStream, replacing game_sa/CdStreamInfo.cpp (MAC-W0). Built ONLY with GTASA_PORTABLE=ON.
//
// Same contract and same globals as the Win32 version (CdStreamInfo.h): a streaming thread reads the requested sectors of an .img file into the caller's
// buffer, CdStreamSync waits for it. Differences, all because Win32 overlapped I/O has no portable counterpart:
//  * files are FILE* (stored in the HANDLE slots of gStreamFileHandles / CdStream::hFile), reads are pread(2) on that file's descriptor;
//  * the "overlapped I/O" mode of the Win32 code (ReadFile + GetOverlappedResult without the thread) does not exist: gOverlappedIO stays 0 and the
//    threaded path (gStreamingInitialized = 1, the one the shipped game uses after CdStreamInit) is the only one. CdStreamInit does not probe it;
//  * a read that returns fewer bytes than asked (end of file) is a success, like ReadFile; only an I/O error is READING_FAILURE;
//  * a failed CdStreamOpen leaves the slot free (the Win32 code leaves INVALID_HANDLE_VALUE in it, i.e. leaks the slot);
//  * paths: '\\' is turned into '/' (the game builds "MODELS\\GTA3.IMG"); case folding / data root is stream W-7;
//  * CdStreamShutdown stops the thread (flag + semaphore + join) before freeing what it uses; the Win32 version closes the handles under a live thread.
#include "StdInc.h"

#include <mutex>
#include <pthread.h>
#include <unistd.h>

#include "oswrapper.h"
#include "AEBankLoader.h"

NOTSA_GLOBAL(gStreamFileHandles, 0x8E4010, (HANDLE[MAX_CD_STREAM_HANDLES]), {});
NOTSA_GLOBAL(gCdImageNames, 0x8E4098, (char[MAX_CD_STREAM_HANDLES][MAX_CD_STREAM_IMAGE_NAME_SIZE]), {});
NOTSA_GLOBAL(gStreamFileCreateFlags, 0x8E3FE0, (uint32), {});
NOTSA_GLOBAL(gCdStreams, 0x8E3FFC, (CdStream*), {});
NOTSA_GLOBAL(gStreamCount, 0x8E4090, (int32), {});
NOTSA_GLOBAL(gOpenStreamCount, 0x8E4094, (int32), {});
NOTSA_GLOBAL(gStreamingInitialized, 0x8E3FE4, (int32), {});
NOTSA_GLOBAL(gOverlappedIO, 0x8E3FE8, (int32), {});
NOTSA_GLOBAL(gStreamQueue, 0x8E3FEC, (Queue), {});
NOTSA_GLOBAL(gStreamSemaphore, 0x8E4004, (HANDLE), {});
NOTSA_GLOBAL(gStreamingThread, 0x8E4008, (HANDLE), {});
NOTSA_GLOBAL(gStreamingThreadId, 0x8E4000, (DWORD), {});
NOTSA_GLOBAL(gLastCdStreamPosn, 0x8E4898, (uint32), {});

namespace {
std::mutex        s_cdStreamThreadSync; // guards stream.nSectorsToRead / bLocked against CdStreamSync (the "deadlock fix" of the Win32 version)
std::atomic<bool> s_shutdown{false};

int FdOf(HANDLE h) { return fileno((FILE*)h); }
} // namespace

void InjectCdStreamHooks() {} // no hook registry in the portable build

// 0x4067B0
CdStreamHandle CdStreamOpen(const char* lpFileName) {
    NOTSA_LOG_DEBUG("CdStreamOpen: {}", lpFileName);
    int32 idx = 0;
    for (; idx < MAX_CD_STREAM_HANDLES; idx++) {
        if (!gStreamFileHandles[idx])
            break;
    }
    if (idx == MAX_CD_STREAM_HANDLES) {
        NOTSA_LOG_ERR("CdStreamOpen: no free stream slot for '{}'", lpFileName);
        return 0;
    }
    std::string path = lpFileName;
    std::replace(path.begin(), path.end(), '\\', '/');
    const auto file = std::fopen(path.c_str(), "rb");
    if (!file) {
        NOTSA_LOG_ERR("CdStreamOpen: cannot open '{}' (errno {})", path, errno);
        return 0;
    }
    gStreamFileHandles[idx] = file;
    strncpy_s(gCdImageNames[idx], lpFileName, MAX_CD_STREAM_IMAGE_NAME_SIZE);
    return (CdStreamHandle)(idx << CD_STREAM_HANDLE_BITS);
}

// 0x406460
eCdStreamStatus CdStreamSync(int32 streamId) {
    CdStream&                    stream = gCdStreams[streamId];
    std::unique_lock<std::mutex> lockGuard{s_cdStreamThreadSync};
    if (gStreamingInitialized) {
        if (stream.nSectorsToRead) {
            stream.bLocked = true;
            lockGuard.unlock();
            OS_SemaphoreWait(stream.sync.hSemaphore);
            lockGuard.lock();
        }
        stream.bInUse = false;
        return stream.status;
    }
    return eCdStreamStatus::READING_SUCCESS; // streaming not initialised: nothing is in flight (no overlapped mode here)
}

// 0x4063E0
eCdStreamStatus CdStreamGetStatus(int32 streamId) {
    CdStream& stream = gCdStreams[streamId];
    if (gStreamingInitialized) {
        if (stream.bInUse)
            return eCdStreamStatus::READING;

        if (stream.nSectorsToRead)
            return eCdStreamStatus::WAITING_TO_READ;

        if (stream.status != eCdStreamStatus::READING_SUCCESS) {
            const eCdStreamStatus status = stream.status;
            stream.status = eCdStreamStatus::READING_SUCCESS;
            return status;
        }
    }
    return eCdStreamStatus::READING_SUCCESS;
}

// 0x406A20
bool CdStreamRead(int32 streamId, void* lpBuffer, CdStreamPos pos, int32 sectorCount) {
    CdStream& stream = gCdStreams[streamId];
    gLastCdStreamPosn = sectorCount + (notsa::IsFixBugs() ? pos.Offset : pos.ToInt()); // CD_STREAM_READ_POS_FIX
    stream.hFile = gStreamFileHandles[pos.FileID];
    if (!gStreamingInitialized) {
        // The Win32 code reads synchronously here (CdStreamInit's probe read). Not reachable: CdStreamInit sets gStreamingInitialized before any read.
        NOTSA_LOG_ERR("CdStreamRead: streaming thread is not initialised");
        return false;
    }
    if (stream.nSectorsToRead || stream.bInUse)
        return false;
    stream.status = eCdStreamStatus::READING_SUCCESS;
    stream.nSectorOffset = pos.Offset;
    stream.nSectorsToRead = sectorCount;
    stream.lpBuffer = lpBuffer;
    stream.bLocked = false;
    AddToQueue(&gStreamQueue, streamId);
    if (!OS_SemaphorePost(gStreamSemaphore))
        NOTSA_LOG_DEBUG("Signal Sema Error");
    return true;
}

// 0x406560
[[noreturn]] void WINAPI CdStreamThread(LPVOID lpParam) {
#ifdef TRACY_ENABLE
    tracy::SetThreadName("CdStreamThread");
#endif

    while (true) {
        OS_SemaphoreWait(gStreamSemaphore);
        if (s_shutdown) {
            pthread_exit(nullptr);
        }

        ZoneScoped;

        const int32 streamId = GetFirstInQueue(&gStreamQueue);
        CdStream& stream = gCdStreams[streamId];
        stream.bInUse = true;
        if (stream.status == eCdStreamStatus::READING_SUCCESS) {
            auto           remaining = (size_t)stream.nSectorsToRead * STREAMING_SECTOR_SIZE;
            off_t          offset    = (off_t)stream.nSectorOffset * STREAMING_SECTOR_SIZE;
            auto*          dst       = (uint8*)stream.lpBuffer;
            const int      fd        = stream.hFile ? FdOf(stream.hFile) : -1;
            while (remaining) {
                const auto n = fd >= 0 ? ::pread(fd, dst, remaining, offset) : -1;
                if (n < 0) {
                    if (errno == EINTR)
                        continue;
                    stream.status = eCdStreamStatus::READING_FAILURE;
                    break;
                }
                if (n == 0) // end of file: ReadFile reports success with fewer bytes
                    break;
                dst += n;
                offset += n;
                remaining -= (size_t)n;
            }
        }

        RemoveFirstInQueue(&gStreamQueue);
        {
            std::lock_guard lockGuard{s_cdStreamThreadSync};
            // locking is necessary here, so the post is not done before the wait in CdStreamSync (deadlock otherwise)
            stream.nSectorsToRead = 0;
            if (stream.bLocked)
                OS_SemaphorePost(stream.sync.hSemaphore);
            stream.bInUse = false;
        }
    }
}

// 0x4068F0
void CdStreamInitThread() {
    s_shutdown = false;
    for (auto& stream : std::span{ gCdStreams, (size_t)gStreamCount }) {
        HANDLE hSemaphore = OS_SemaphoreCreate(2, nullptr);
        stream.sync.hSemaphore = hSemaphore;
        if (!hSemaphore) {
            NOTSA_LOG_DEBUG("cdvd_stream: failed to create sync semaphore");
            return;
        }
    }
    InitialiseQueue(&gStreamQueue, gStreamCount + 1);
    gStreamSemaphore = OS_SemaphoreCreate(5, "CdStream");
    if (gStreamSemaphore) {
        gStreamingThread = OS_ThreadLaunch([](void*) -> uint32 { CdStreamThread(nullptr); }, nullptr, 0, "CdStream");
        if (!gStreamingThread) {
            NOTSA_LOG_DEBUG("cdvd_stream: failed to create streaming thread");
        }
    } else {
        NOTSA_LOG_DEBUG("cdvd_stream: failed to create stream semaphore");
    }
}

// 0x406B70
void CdStreamInit(int32 streamCount) {
    for (int32 i = 0; i < MAX_CD_STREAM_HANDLES; i++) {
        gStreamFileHandles[i] = nullptr;
        gCdImageNames[i][0] = 0;
    }
    gStreamFileCreateFlags = 0; // FILE_FLAG_OVERLAPPED / NO_BUFFERING have no meaning here
    gOverlappedIO = 0;
    gOpenStreamCount = 0;
    gStreamCount = streamCount;
    gCdStreams = (CdStream*)LocalAlloc(LPTR, sizeof(CdStream) * streamCount);
    // The Win32 code opens MODELS\GTA3.IMG and probes an overlapped read to choose the I/O mode, then closes it again; there is only one mode here.
    gStreamingInitialized = 1;
    CdStreamInitThread();
}

// 0x406690
void CdStreamRemoveImages() {
    for (int32 i = 0; i < gStreamCount; ++i) {
        CdStreamSync(i);
    }
    for (int32 i = 0; i < gOpenStreamCount; i++) {
        if (gStreamFileHandles[i]) {
            std::fclose((FILE*)gStreamFileHandles[i]);
        }
        gStreamFileHandles[i] = nullptr;
        gCdImageNames[i][0] = 0;
    }
    gOpenStreamCount = 0;
}

// 0x406370
void CdStreamShutdown() {
    if (gStreamingInitialized) {
        if (gStreamingThread) {
            s_shutdown = true;
            OS_SemaphorePost(gStreamSemaphore);
            OS_ThreadWait(gStreamingThread);
            OS_ThreadClose(gStreamingThread);
            gStreamingThread = nullptr;
        }
        FinalizeQueue(&gStreamQueue);
        OS_SemaphoreDelete(gStreamSemaphore);
        for (auto& stream : std::span{ gCdStreams, (size_t)gStreamCount }) {
            OS_SemaphoreDelete(stream.sync.hSemaphore);
        }
    }
    LocalFree(gCdStreams);
}

uint32 CdStreamHandleToFileID(CdStreamHandle h) {
    return h >> CD_STREAM_HANDLE_BITS;
}
