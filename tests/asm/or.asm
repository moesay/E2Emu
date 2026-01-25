BITS 16
ORG 0x100
mov ax, 0x00f0
mov bx, 0x0f00
or ax, bx
hlt
