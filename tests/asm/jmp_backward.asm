BITS 16
ORG 0x100
mov cx, 3
mov ax, 0
loop_start:
inc ax
dec cx
jnz loop_start
hlt
