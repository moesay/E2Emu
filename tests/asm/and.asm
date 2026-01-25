BITS 16
ORG 0x100
mov ax, 0xff0f
mov bx, 0x0ff0
and ax, bx
hlt
