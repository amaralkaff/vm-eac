.code
PUBLIC hv_cpuid_call
hv_cpuid_call proc
    push rbx
    push rsi
    mov rsi, [rsp + 38h]   ; out pointer (5th arg on stack)
    mov eax, ecx            ; leaf
    mov r10, r8             ; save param1
    mov ecx, edx            ; command -> ECX
    mov rdx, r10            ; param1 -> RDX
    mov rbx, r9             ; param2 -> RBX
    mov r8, 0DEAD1337C0DE5AFEh  ; magic auth
    cpuid
    mov [rsi],       rax
    mov [rsi + 8h],  rbx
    mov [rsi + 10h], rcx
    mov [rsi + 18h], rdx
    pop rsi
    pop rbx
    ret
hv_cpuid_call endp
END
