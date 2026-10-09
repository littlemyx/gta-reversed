; Placeholder inside the standalone exe's own image (linked at /BASE:0x400000, merged into .text by /MERGE:_TEXT=.text).
; It makes the *main image* cover the original data VA range [0x858000, 0xCB0000): only the main image is mapped before the
; process heap / NLS sections, so (unlike VirtualAlloc at startup) this range can never be taken by someone else.
; DataImage.cpp makes the data part RW and copies original_data.bin into it. 0x8B0000 >= 0xCB1000 - 0x401000, so wherever in the first
; pages of .text the linker puts it, it covers the data range.
.686
.model flat
.code
public _notsa_orig_pad
public _notsa_orig_pad_end
_notsa_orig_pad label byte
    db 8B0000h dup(0)
_notsa_orig_pad_end label byte
end
