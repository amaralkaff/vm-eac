extern vmcs_setup_and_launch:proc
PUBLIC capture_ctx
PUBLIC restore_ctx
.code
capture_ctx proc
pushfq
push rax
push rcx
push rdx
push rbx
push rbp
push rsi
push rdi
push r8
push r9
push r10
push r11
push r12
push r13
push r14
push r15
sub rsp, 0100h
mov rdx, rsp
call vmcs_setup_and_launch
jmp restore_ctx
capture_ctx endp
restore_ctx proc
add rsp, 0100h
pop r15
pop r14
pop r13
pop r12
pop r11
pop r10
pop r9
pop r8
pop rdi
pop rsi
pop rbp
pop rbx
pop rdx
pop rcx
pop rax
popfq
ret
restore_ctx endp
END