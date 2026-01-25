BITS 16
ORG 0x100
mov ax, 0x0001
mov cl, 4
shl ax, cl
hlt
