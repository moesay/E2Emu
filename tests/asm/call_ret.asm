BITS 16
ORG 0x100
mov ax, 0
call increment
call increment
call increment
hlt
increment:
inc ax
ret
