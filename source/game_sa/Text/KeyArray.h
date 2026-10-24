#pragma once

struct CKeyEntry {
    const GxtChar* string; // relative to TKEY block; need to call update after reading
    uint32      hash;      // name hash
#ifdef NOTSA_PORTABLE
    uint32      m_fileOffset; // portable: the 32 bit file offset the exe keeps in the 4 byte `string` slot until Update() (see CKeyEntry_File)
#endif
};

// A record of the TKEY chunk of a .gxt file: { offset into the TDAT block, name hash }. The exe reads the chunk straight into CKeyEntry[] (8 bytes on x86);
// CKeyEntry is 16 (24 with the offset) bytes on LP64, so a portable build reads these and converts. On-disk layout, identical on every platform.
struct CKeyEntry_File {
    uint32 offset;
    uint32 hash;
};
VALIDATE_SIZE_FILE(CKeyEntry_File, 0x8);

// TKEY block
class CKeyArray {
public:
    CKeyEntry* m_data;
    uint32     m_size;

public:
    CKeyArray();
    ~CKeyArray();

    bool Load(uint32 length, FILESTREAM file, uint32* offset, uint8 unknown);
    void Unload();

    void Update(char* offset);

    /*!
     * @brief Find text by it's key
     *
     * @param     key   The text's key
     * @param out found Whenever the text was found
     *
     * @return The text identified by the given key or null if not found
     */
    [[nodiscard]] const GxtChar* Search(const char* key, bool& found);

private:
    CKeyEntry* BinarySearch(uint32 key, CKeyEntry* entries, int16 firstIndex, int16 lastIndex);

private:
    friend class CText; // InjectHooks()
};
