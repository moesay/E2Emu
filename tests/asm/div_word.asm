BITS 16
ORG 0x100
mov dx, 0
mov ax, 1000
mov bx, 30
div bx
hlt
