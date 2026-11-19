; NOTE: RFLAGS must be preserved: MSVC places the _penter call after the prologue, possibly between a cmp and the cmov/jcc that uses it.
; T3H GTASA_HITCOUNT: x64 _penter hook for MSVC /Gh. Saves everything a callee may expect live at its entry (arg regs, rax, r10/r11, xmm0-5),
; aligns the stack, calls notsa_hitcount_hit(return address). The return address lies inside the instrumented function: it identifies it.
EXTERN notsa_hitcount_hit:PROC
.CODE
_penter PROC
    push rbp
    mov rbp, rsp
    pushfq
    push rax
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    and rsp, -16
    sub rsp, 128
    movaps [rsp+32], xmm0
    movaps [rsp+48], xmm1
    movaps [rsp+64], xmm2
    movaps [rsp+80], xmm3
    movaps [rsp+96], xmm4
    movaps [rsp+112], xmm5
    mov rcx, [rbp+8]
    call notsa_hitcount_hit
    movaps xmm0, [rsp+32]
    movaps xmm1, [rsp+48]
    movaps xmm2, [rsp+64]
    movaps xmm3, [rsp+80]
    movaps xmm4, [rsp+96]
    movaps xmm5, [rsp+112]
    lea rsp, [rbp-64]
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
    pop rax
    popfq
    pop rbp
    ret
_penter ENDP
END
