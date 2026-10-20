#include "StdInc.h"

#include "Messages.h"
#include "Hud.h"
#include <ControllerConfigManager.h>

constexpr auto MSG_BUF_SZ = 400u;

void CMessages::InjectHooks() {
    RH_ScopedClass(CMessages);
    RH_ScopedCategoryGlobal();

    RH_ScopedInstall(GetStringLength, 0x69DB50);
    RH_ScopedInstall(StringCopy, 0x69DB70);
    RH_ScopedInstall(StringCompare, 0x69DBD0);
    RH_ScopedInstall(CutString, 0x69DC50);
    RH_ScopedInstall(ClearMessages, 0x69DCD0);
    RH_ScopedInstall(ClearSmallMessagesOnly, 0x69DD30);
    RH_ScopedInstall(AddToPreviousBriefArray, 0x69DD50);
    RH_ScopedInstall(ClearPreviousBriefArray, 0x69DE70);
    RH_ScopedInstall(InsertNumberInString, 0x69DE90);
    RH_ScopedInstall(InsertStringInString, 0x69E040);
    RH_ScopedInstall(InsertPlayerControlKeysInString, 0x69E160);
    RH_ScopedInstall(AddMessageWithNumberQ, 0x69E360);
    RH_ScopedInstall(AddMessageJumpQWithNumber, 0x69E4E0);
    RH_ScopedInstall(AddBigMessageWithNumber, 0x69E5F0);
    RH_ScopedInstall(AddBigMessageWithNumberQ, 0x69E6E0);
    RH_ScopedInstall(AddMessageWithStringQ, 0x69E800);
    RH_ScopedInstall(AddMessageJumpQWithString, 0x69E950);
    RH_ScopedInstall(ClearThisPrint, 0x69EA30);
    RH_ScopedInstall(ClearThisBigPrint, 0x69EBE0);
    RH_ScopedInstall(ClearThisPrintBigNow, 0x69ED80);
    RH_ScopedInstall(Init, 0x69EE00);
    RH_ScopedInstall(ClearAllMessagesDisplayedByGame, 0x69EDC0);
    RH_ScopedInstall(Process, 0x69EE60);
    RH_ScopedInstall(Display, 0x69EFC0);
    RH_ScopedInstall(AddMessageQ, 0x69F0B0);
    RH_ScopedInstall(AddMessageJump, 0x69F1E0);
    RH_ScopedInstall(AddBigMessage, 0x69F2B0);
    RH_ScopedInstall(AddBigMessageQ, 0x69F370);
}

// Initialises messages
// 0x69EE00
void CMessages::Init() {
    ZoneScoped;

    ClearMessages(false); // the exe clears the text of EVERY big message stack (also the mission-title styles), not only the ignorable ones
    rng::fill(PreviousBriefs, tPreviousBrief{});
}

template<size_t N>
tMessage* FindUnusedMsgInArray(std::array<tMessage, N>& arr) {
    for (auto& msg : arr) {
        if (!msg.IsValid()) {
            return &msg;
        }
    }
    NOTSA_LOG_DEBUG("No free brief message found!");
    return nullptr;
}

/*********************
* BRIEF MESSAGES
**********************/

/*!
* Add a new brief message.
* @param text           The text (Possibly with format characters, eg.: ~~~~
* @param flag           Flags
* @param bPreviousBrief Whenever to call `AddToPreviousBriefArray`
* @param showInstantly  Whenever to show the message instantly
* @param str            String to insert [May be null]
* @param numbers        Numbers to insert (Use -1 as placeholder)
* @notsa
*/
void CMessages::AddMessage2(const GxtChar* text, uint32 time, uint16 flag, bool bPreviousBrief, bool showInstantly, GxtChar* str, std::optional<std::array<int32, 6>> numbers) {
    const auto CreateAt = [&](tMessage* p) {
        new (p) tMessage{
            text,
            str,
            flag,
            time,
            bPreviousBrief,
            numbers
        };
        if (bPreviousBrief && (showInstantly || p == &BriefMessages.front())) {
		    AddToPreviousBriefArray(
			    p->Text,
			    p->NumbersToInsert[0],
			    p->NumbersToInsert[1],
			    p->NumbersToInsert[2],
			    p->NumbersToInsert[3],
			    p->NumbersToInsert[4],
			    p->NumbersToInsert[5],
			    p->StringToInsert
            );
        }
    };
    if (showInstantly) {
        CreateAt(&BriefMessages[0]);
    } else if (const auto p = FindUnusedMsgInArray(BriefMessages)) {
        CreateAt(p);
    }
}

/*!
* Adds message to [Q]ueue
* @addr 0x69F0B0
*/
void CMessages::AddMessageQ(const GxtChar* text, uint32 time, uint16 flag, bool bPreviousBrief) { // Renamed from AddMessage
    /* Some string copy code here */
    AddMessage2(text, time, flag, bPreviousBrief, false);
}

/*!
* Adds message to [Q]ueue [With a string in `text` that will be replaced by `string`]
* @addr 0x69E800
*/
void CMessages::AddMessageWithStringQ(const GxtChar* text, uint32 time, uint16 flag, GxtChar* string, bool bPreviousBrief) {
    /* Some string copy code here */
    AddMessage2(text, time, flag, bPreviousBrief, false, string);
}

/*!
* Show a message instantly
* @addr 0x69F1E0
*/
void CMessages::AddMessageJump(const GxtChar* text, uint32 time, uint16 flag, bool bPreviousBrief) { // Renamed from AddMessageJumpQ
    /* unused string copy here */
    AddMessage2(text, time, flag, bPreviousBrief, true);
}

/*!
* Adds message to [Q]ueue [With numbers in `text`]
* @addr 0x69E360
*/
void CMessages::AddMessageWithNumberQ(const GxtChar* text, uint32 time, uint16 flag, int32 n1, int32 n2, int32 n3, int32 n4, int32 n5, int32 n6, bool bPreviousBrief) {
    /* Some string copy code here */
    AddMessage2(text, time, flag, bPreviousBrief, false, nullptr, { { n1, n2, n3, n4, n5, n6 } });
}

/*!
* Show a message instantly [With numbers in `text`]
* @addr 0x69E4E0
*/
void CMessages::AddMessageJumpQWithNumber(const GxtChar* text, uint32 time, uint16 flag, int32 n1, int32 n2, int32 n3, int32 n4, int32 n5, int32 n6, bool bPreviousBrief) {
    AddMessage2(text, time, flag, bPreviousBrief, true, nullptr, { { n1, n2, n3, n4, n5, n6 } });
}

/*!
* Show a message instantly [With a string in `text`]
* @addr 0x69E950
*/
void CMessages::AddMessageJumpQWithString(const GxtChar* text, uint32 time, uint16 flag, GxtChar* string, bool bPreviousBrief) {
    AddMessage2(text, time, flag, bPreviousBrief, true, string);
}

/*********************
* BIG MESSAGES
**********************/

/*!
* Add a new big message.
* @param text           The text (Possibly with format characters, eg.: ~~~~)
* @param bPreviousBrief Whenever to call `AddToPreviousBriefArray`
* @param showInstantly  Whenever to show the message instantly
* @param str            String to insert [May be null]
* @param numbers        Numbers to insert (Use -1 as placeholder)
* @notsa
*/
void CMessages::AddBigMessage2(const GxtChar* text, uint32 time, eMessageStyle style, bool showInstantly, GxtChar* str, std::optional<std::array<int32, 6>> numbers) {
    const auto CreateAt = [&](auto* ptr) {
        new (ptr) tMessage{
            text,
            str,
            0,
            time,
            false,
            numbers
        };
    };
    if (showInstantly) {
        CreateAt(&BIGMessages[style].Stack.front());
    } else if (const auto p = FindUnusedMsgInArray(BIGMessages[style].Stack)) {
        CreateAt(p);
    }
}

// Adds big message and shows it instantly
// 0x69F2B0
void CMessages::AddBigMessage(const GxtChar* text, uint32 time, eMessageStyle style) {
    AddBigMessage2(text, time, style, true);
}

// Adds big message to [Q]ueue
// 0x69F370
void CMessages::AddBigMessageQ(const GxtChar* text, uint32 time, eMessageStyle style) {
    AddBigMessage2(text, time, style, false);
}

// Adds big message with numbers and shows it instantly
// 0x69E5F0
void CMessages::AddBigMessageWithNumber(const GxtChar* text, uint32 time, eMessageStyle style, int32 n1, int32 n2, int32 n3, int32 n4, int32 n5, int32 n6) {
    AddBigMessage2(text, time, style, true, nullptr, {{n1, n2, n3, n4, n5, n6}});
}

// Adds big message with numbers to [Q]ueue
// 0x69E6E0
void CMessages::AddBigMessageWithNumberQ(const GxtChar* text, uint32 time, eMessageStyle style, int32 n1, int32 n2, int32 n3, int32 n4, int32 n5, int32 n6) {
    AddBigMessage2(text, time, style, false, nullptr, { {n1, n2, n3, n4, n5, n6} });
}

// Adds message to previous brief
// 0x69DD50
void CMessages::AddToPreviousBriefArray(const GxtChar* text, int32 n1, int32 n2, int32 n3, int32 n4, int32 n5, int32 n6, const GxtChar* string) {
    const auto numbers = std::array{ n1, n2, n3, n4, n5, n6 };

    // Find unused entry + duplicate check
    size_t i = 0;
    for (; i < PreviousBriefs.size(); i++) {
        const auto& prev = PreviousBriefs[i];
        if (!prev.Text) {
            break; // Found unused
        }
        // Check if it's a duplicate of the one we're about to insert
        if (prev.Text != text || prev.StringToInsert != string) { // I mean that's a pretty naive way, but okay
            continue;
        }
        if (numbers != prev.NumbersToInsert) {
            continue; 
        }
        return; // Duplicate messages after each other, so skip
    }

    // In case no free entries were found we overwrite the last one
    if (i == PreviousBriefs.size()) {
        i = PreviousBriefs.size() - 1;
    }

    // Move entries 0..i-1 to 1..i so that we overwrite the unused entry at `i` (or the last one) and thus free up the 0th entry
    // if `i == 0` this won't do anything. (The range is [0, i + 1): std::shift_right keeps the elements INSIDE the range, so the old `begin() + i` lost entry i - 1.)
    std::shift_right(
        PreviousBriefs.begin(),
        PreviousBriefs.begin() + i + 1,
        1
    );

    // Now construct at the 0th entry (as it's now freed up)
    new (&PreviousBriefs[0]) tPreviousBrief {
        .Text            = text,
        .NumbersToInsert = numbers,
        .StringToInsert  = string,
    };
}

// Removes registered messages
// 0x69DCD0
void CMessages::ClearMessages(bool bIgnoreMissionTitle) {
    // Clear all big messages except mission titles if bIgnoreMissionTitle is true
    for (eMessageStyle style = eMessageStyle::STYLE_MIDDLE; style < eMessageStyle::NUM_MESSAGE_STYLES; style = static_cast<eMessageStyle>(style + 1)) {
        // Skip STYLE_BOTTOM_RIGHT and STYLE_MIDDLE_SMALLER_HIGHER if ignoring mission titles
        if (bIgnoreMissionTitle && (style == eMessageStyle::STYLE_BOTTOM_RIGHT || style == eMessageStyle::STYLE_MIDDLE_SMALLER_HIGHER)) {
            continue;
        }
        rng::fill(BIGMessages[style].Stack, tMessage{});
    }

    // Clear brief messages
    ClearSmallMessagesOnly();
}

// Removes messages in brief
// 0x69DD30
void CMessages::ClearSmallMessagesOnly() {
    rng::fill(BriefMessages, tMessage{});
}

// Remove messages from previous brief
// 0x69DE70
void CMessages::ClearPreviousBriefArray() {
    rng::fill(PreviousBriefs, tPreviousBrief{});
}

template<size_t N>
void ClearThisPrint_Impl(std::array<tMessage, N>& messages, const GxtChar* text, auto&& OnFirstMessageCleared) {
    assert(text);
    for (;;) {
        size_t i = 0;
        for (;;) {
            const auto& msg = messages[i];
            if (!msg.IsValid()) {
                return; // Reached end of active texts
            }
            if (strcmp(AsciiFromGxtChar(msg.Text), AsciiFromGxtChar(text)) == 0) {
                break; // Texts match, so clear this
            }
            if (++i >= messages.size()) {
                return; // Reached end
            }
        }
        
        // Overwrite this element
        std::shift_left(
            messages.begin() + i,
            messages.end(),
            1
        );

        // Clear last (As it was left in an unspecified state)
        messages.back() = {};

        if (i == 0) {
            auto& msg = messages[i];
            msg.CreatedAtMS = CTimer::GetTimeInMS();
            if (OnFirstMessageCleared) {
                OnFirstMessageCleared(msg);
            }
        }
    }
}

// Removes small message with this text
// 0x69EA30
void CMessages::ClearThisPrint(const GxtChar* text) {
    ClearThisPrint_Impl(BriefMessages, text, [](tMessage& msg) {
        if (!msg.Text || !msg.PreviousBrief) { // the exe tests the new front message's PreviousBrief byte (+0x2C)
            return;
        }
        AddToPreviousBriefArray(
            msg.Text,
            msg.NumbersToInsert[0],
            msg.NumbersToInsert[1],
            msg.NumbersToInsert[2],
            msg.NumbersToInsert[3],
            msg.NumbersToInsert[4],
            msg.NumbersToInsert[5],
            msg.StringToInsert
        );
    });
}

// Removes big message with this text
// 0x69EBE0
void CMessages::ClearThisBigPrint(const GxtChar* text) {
    assert(text);
    for (size_t b = 0; b < NUM_MESSAGE_STYLES; b++) {
        ClearThisPrint_Impl(BIGMessages[b].Stack, text, [](tMessage&) { /*nop*/ });
    }
}

// Removes first big message in messages stack
// 0x69ED80
void CMessages::ClearThisPrintBigNow(eMessageStyle style) {
    if (auto& msg = BIGMessages[style].Stack[0]; msg.Text) {
        ClearThisBigPrint(msg.Text);
    }
    CHud::BigMessageX[style] = 0.f;
    CHud::m_BigMessage[style][0] = 0;
}

// Removes all displayed messages
// 0x69EDC0
void CMessages::ClearAllMessagesDisplayedByGame(bool unk) {
    ZoneScoped;

    ClearMessages(unk);
    ClearPreviousBriefArray();
    CHud::GetRidOfAllHudMessages(unk);
}

// Returns length of a string
// 0x69DB50
uint32 CMessages::GetStringLength(const GxtChar* string) {
    return std::strlen(AsciiFromGxtChar(string));
}

// Copies string src to dest
// 0x69DB70
void CMessages::StringCopy(GxtChar* dest, const GxtChar* src, uint16 len) {
    if (src && len) {
        const auto copyLen = std::min<size_t>(GetStringLength(src), len - 1u);

        std::memcpy(dest, AsciiFromGxtChar(src), copyLen);
        dest[copyLen] = '\0';
    } else {
        dest[0] = '\0';
    }
}

/*!
* Check if 2 string are equal
* @addr 0x69DBD0
*/
bool CMessages::StringCompare(const GxtChar* str1, const GxtChar* str2, uint16 len) {
    return std::strncmp(AsciiFromGxtChar(str1), AsciiFromGxtChar(str2), len) == 0;
}

// 0x69DC50
void CMessages::CutString(int32 count, const GxtChar* str, GxtChar** dest) {
    NOTSA_UNREACHABLE(); // unused
}

template<size_t N>
void StringReplace(const char* haystack, const char (&needle)[N], char* dst, auto&& Replace) {
    // Based on https://stackoverflow.com/a/32413923
    auto        pDst = dst;
    const char* pHS  = haystack;
    
    for(;;) {
        const char* pNeedle = strstr(pHS, needle);

        // walked past last occurrence of needle; copy remaining part
        if (!pNeedle) {
            pDst = std::copy(pHS, pHS + strlen(pHS), pDst);
            break;
        }

        // copy part before needle
        pDst = std::copy(pHS, pNeedle, pDst);

        // replace portion of string
        pDst = Replace(pDst);

        // adjust pointers, move on
        pHS = pNeedle + (N - 1);
    }
    *pDst = 0; // Null terminate
}

// Insert numbers into string
// 0x69DE90
void CMessages::InsertNumberInString(const GxtChar* str, int32 n1, int32 n2, int32 n3, int32 n4, int32 n5, int32 n6, GxtChar* dst) {
    if (!str) { // Appereantly that's valid?
        *dst = 0; // Null terminate result anyways
        return;
    }
    StringReplace(AsciiFromGxtChar(str), "~1~", (char*)dst, [n = 0, numbers = std::array{ n1, n2, n3, n4, n5, n6 }](char* where) mutable {
        // Originally they used sprintf + AsciiToGxtChar, but this should work. We don't care about any substring except '~1~'.
        return std::format_to(where, "{}", numbers[n++]);
    });
}

// Inserts string into src
// 0x69E040
// Replaces every "~a~" of `target` with `replacement` (the exe works through a 0x180 byte stack buffer, in place, stops once the output reached len(target) + len(replacement)
// characters and zero-fills `target` up to 400 bytes).
void CMessages::InsertStringInString(GxtChar* target, const GxtChar* replacement) {
    if (!target || !replacement) { // Appereantly that's valid?
        return;
    }

    const auto lenT = (uint32)std::strlen(AsciiFromGxtChar(target));
    const auto lenR = (uint16)std::strlen(AsciiFromGxtChar(replacement));
    const auto total = (uint16)(lenT + lenR);

    GxtChar buf[0x180]{};
    uint16 out = 0, in = 0;
    if (total != 0) {
        do {
            const GxtChar* p = target + in;
            if (p[0] == '~' && p[1] == 'a' && p[2] == '~') {
                in += 3;
                for (uint16 k = 0; k < lenR; k++) { // (the exe's replacement loop is a do-while: lenR == 0 copies nothing)
                    buf[out++] = replacement[k];
                }
            } else {
                buf[out++] = p[0];
                in++;
            }
        } while (out < total);
    }
    buf[out] = 0;
    if (total != 0) {
        std::memcpy(target, buf, total);
    }
    if (total < 400) {
        std::memset(target + total, 0, 400 - total);
    }
}

// Inserts key events into string
// 0x69E160: scans the string for `~k~~NAME~`; NAME is looked up in the controller action name table (59 entries, ControlsManager+0x224, the name must be followed by `~`), the key names of that
// action replace the whole tag. A `~k~~` whose name matches no entry is dropped (the 4 tag characters are skipped, the rest of the text is kept). The result is copied back over the string and
// the rest of the 400-byte buffer is zeroed.
void CMessages::InsertPlayerControlKeysInString(GxtChar* string) {
    static_assert(+eControllerAction::CONTROLLER_ACTION_COUNT == 0x3B);
    if (!string) {
        return;
    }
    const uint16 len = (uint16)strlen(AsciiFromGxtChar(string));
    GxtChar      out[MSG_BUF_SZ];
    GxtChar      aname[MSG_BUF_SZ]{};
    uint32       outLen = 0;
    uint32       i      = 0;
    while ((uint16)i < len) {
        const GxtChar* c = string + (uint16)i;
        if (c[0] == '~' && c[1] == 'k' && c[2] == '~') {
            i += 4;
            bool found = false;
            for (uint16 bx = 0; !found && bx < 0x3B; ++bx) {
                const GxtChar* name    = ControlsManager.m_ControllerActionName[bx];
                const uint16   nameLen = (uint16)strlen(AsciiFromGxtChar(name));
                if (nameLen == 0) {
                    continue;
                }
                if (StringCompare(string + (uint16)i, name, nameLen) && string[nameLen + (uint16)i] == '~') {
                    found = true;
                    ControlsManager.GetGxtStringOfCommandKeys((eControllerAction)bx, aname, MSG_BUF_SZ);
                    i = nameLen + i + 1;
                    const uint16 keysLen = (uint16)strlen(AsciiFromGxtChar(aname));
                    for (uint16 k = 0; k < keysLen; ++k) {
                        out[(uint16)outLen++] = aname[k];
                        aname[k]              = 0;
                    }
                }
            }
        } else {
            out[(uint16)outLen++] = *c;
            ++i;
        }
    }
    out[(uint16)outLen] = 0;
    const uint16 n      = (uint16)strlen(AsciiFromGxtChar(out));
    std::memcpy(string, out, n);
    if (n < MSG_BUF_SZ) {
        std::memset(string + n, 0, MSG_BUF_SZ - n);
    }
}

// Processing messages. This is called from CWorld::Process
// 0x69EE60
void CMessages::Process() {
    // Returns true when the front message expired and the queue advanced (the exe clears the front, moves the following live messages one slot down and stamps the new front)
    const auto ProcessMessagesArray = [](auto&& msgs) {
        auto& f = msgs.front();
        if (!f.IsValid()) { // Not even valid (Neither are the ones after it)
            return false;
        }
        if (CTimer::GetTimeInMS() <= f.GetTimeToDisappearAtMS()) { // Still visible
            return false;
        }
        f.Text = nullptr; // First one disappeared, so we can delete it
        size_t i = 0;
        while (msgs[i + 1].IsValid()) { // move the live messages after it one slot down
            msgs[i] = msgs[i + 1];
            if (++i >= msgs.size() - 1) {
                break;
            }
        }
        msgs[i].Text = nullptr; // the last moved-from slot is unused now
        f.CreatedAtMS = CTimer::GetTimeInMS(); // (also when the queue is empty now)
        return true;
    };

    // Process big messages
    for (auto& omgVeryBig : BIGMessages) {
        ProcessMessagesArray(omgVeryBig.Stack);
	}

    // Process briefs: when the queue advanced, the new front goes to the previous briefs if it asks for it
    if (ProcessMessagesArray(BriefMessages)) {
        if (const auto f = BriefMessages.front(); f.IsValid() && f.PreviousBrief) {
            AddToPreviousBriefArray(
                f.Text,
                f.NumbersToInsert[0],
                f.NumbersToInsert[1],
                f.NumbersToInsert[2],
                f.NumbersToInsert[3],
                f.NumbersToInsert[4],
                f.NumbersToInsert[5],
                f.StringToInsert
            );
        }
    }
}

// Displays messages
// 0x69EFC0
void CMessages::Display(bool bNotFading) {
    GxtChar buff[MSG_BUF_SZ];

    // Helper function to process text for display
    const auto ProcessMessageText = [&buff](const tMessage& msg) {
        InsertNumberInString(msg.Text, msg.NumbersToInsert[0], msg.NumbersToInsert[1], msg.NumbersToInsert[2], msg.NumbersToInsert[3], msg.NumbersToInsert[4], msg.NumbersToInsert[5], buff);
        InsertStringInString(buff, msg.StringToInsert);
        InsertPlayerControlKeysInString(buff);
    };

    // Display big messages if not fading
    if (bNotFading) {
        for (eMessageStyle style = STYLE_MIDDLE; style < NUM_MESSAGE_STYLES; style = static_cast<eMessageStyle>(style + 1)) {
            const auto& msg = BIGMessages[style].Stack[0];
            // Please dont check for IsValid here, that isnt correct, because if
            // the array is empty, the first element is still valid, but not initialized
            ProcessMessageText(msg);
            CHud::SetBigMessage(buff, style);
        }
    }

    // Display brief messages based on script fading condition
    if (bNotFading == CTheScripts::bDrawSubtitlesBeforeFade) {
        const auto& msg = BriefMessages[0];
        ProcessMessageText(msg);
        CHud::SetMessage(buff);
    }
}
