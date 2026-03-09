.code

get_cs proc
mov ax, cs
ret
get_cs endp

get_ss proc
mov ax, ss
ret
get_ss endp

get_ds proc
mov ax, ds
ret
get_ds endp

get_es proc
mov ax, es
ret
get_es endp

get_fs proc
mov ax, fs
ret
get_fs endp

get_gs proc
mov ax, gs
ret
get_gs endp

get_tr proc
str ax
ret
get_tr endp

get_ldtr proc
sldt ax
ret
get_ldtr endp

end