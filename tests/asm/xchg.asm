BITS 16
ORG 0x100
mov ax, 0x1111
mov bx, 0x2222
xchg ax, bx
hlt
