extern vmentry_handler_cpp:proc
PUBLIC vmexit_handler
extern RtlCaptureContext:proc
.code



vmexit_handler proc
pushfq
push r15
push r14
push r13
push r12
push r11
push r10
push r9
push r8
push rdi
push rsi
push rbp
push rbx
push rdx
push rcx
push rax
mov rcx,rsp
sub rsp,28h
call vmentry_handler_cpp
add rsp,28h
pop rax
pop rcx
pop rdx
pop rbx
pop rbp
pop rsi
pop rdi
pop r8
pop r9
pop r10
pop r11
pop r12
pop r13
pop r14
pop r15
popfq
vmresume
; If vmresume fails, we reach here - trigger BSOD so we know
int 3
jmp $
vmexit_handler endp

END
