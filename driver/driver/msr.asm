.code

memcpy_s proc
  mov r10, ehandler
  mov r11, rcx
  mov byte ptr [rcx], 0

  push rsi
  push rdi

  mov rsi, r8
  mov rdi, rdx
  mov rcx, r9

  rep movsb

ehandler:
  pop rdi
  pop rsi

  ret
memcpy_s endp

xsetbv_s proc
  mov r10, ehandler
  mov r11, rcx
  mov byte ptr [rcx], 0

  mov ecx, edx

  mov eax, r8d

  mov rdx, r8
  shr rdx, 32

  xsetbv

ehandler:
  ret
xsetbv_s endp

wrmsr_s proc
  mov r10, ehandler
  mov r11, rcx
  mov byte ptr [rcx], 0

  mov ecx, edx

  mov eax, r8d
  mov rdx, r8
  shr rdx, 32

  wrmsr

ehandler:
  ret
wrmsr_s endp

rdmsr_s proc
  mov r10, ehandler
  mov r11, rcx
  mov byte ptr [rcx], 0

  mov ecx, edx

  rdmsr

  shl rdx, 32
  and rax, rdx

ehandler:
  ret
rdmsr_s endp

end

