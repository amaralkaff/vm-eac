;
; hv_cpuid.asm
; Usermode CPUID hypercall helper
; Sets full 64-bit registers before CPUID and reads 64-bit results
;
; void hv_cpuid_call(
;       uint32_t leaf,      ; RCX -> EAX
;       uint32_t command,   ; RDX -> ECX (sub-leaf)
;       uint64_t param1,    ; R8  -> RDX
;       uint64_t param2,    ; R9  -> RBX
;       uint64_t* out       ; [RSP+28h] -> pointer to 4x uint64_t [rax,rbx,rcx,rdx]
; );
;
; void hv_cpuid_call_ex(
;       uint32_t leaf,      ; RCX -> EAX
;       uint32_t command,   ; RDX -> ECX
;       uint64_t param1,    ; R8  -> RDX
;       uint64_t param2,    ; R9  -> RBX
;       uint64_t param3,    ; [RSP+28h] -> R8 (for write size etc.)
;       uint64_t* out       ; [RSP+30h] -> pointer to 4x uint64_t
; );
;

.code

PUBLIC hv_cpuid_call
PUBLIC hv_cpuid_call_ex

hv_cpuid_call proc
    push rbx
    push rsi

    ; out pointer at [rsp + 28h + 10h] = [rsp + 38h]
    mov rsi, [rsp + 38h]

    ; Set up registers
    mov eax, ecx            ; leaf
    mov r10, r8             ; save param1
    mov ecx, edx            ; command -> ECX
    mov rdx, r10            ; param1 -> RDX
    mov rbx, r9             ; param2 -> RBX

    ; Authentication: set R8 = HV_MAGIC so hypervisor knows we're legit
    mov r8, 0DEAD1337C0DE5AFEh

    cpuid

    ; Store 64-bit results
    mov [rsi],       rax
    mov [rsi + 8h],  rbx
    mov [rsi + 10h], rcx
    mov [rsi + 18h], rdx

    pop rsi
    pop rbx
    ret
hv_cpuid_call endp

hv_cpuid_call_ex proc
    push rbx
    push rsi

    ; param3 at [rsp + 28h + 10h] = [rsp + 38h]
    ; out    at [rsp + 30h + 10h] = [rsp + 40h]
    mov r10, [rsp + 38h]    ; param3
    mov rsi, [rsp + 40h]    ; out pointer

    ; Set up registers
    mov eax, ecx            ; leaf
    mov r11, r8             ; save param1
    mov ecx, edx            ; command -> ECX
    mov rdx, r11            ; param1 -> RDX
    mov rbx, r9             ; param2 -> RBX
    mov r8, r10             ; param3 -> R8 (visible in guest_regs_t)

    ; Authentication: set R9 = HV_MAGIC (R8 is used for param3)
    mov r9, 0DEAD1337C0DE5AFEh

    cpuid

    mov [rsi],       rax
    mov [rsi + 8h],  rbx
    mov [rsi + 10h], rcx
    mov [rsi + 18h], rdx

    pop rsi
    pop rbx
    ret
hv_cpuid_call_ex endp

END
