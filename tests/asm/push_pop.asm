BITS 16
ORG 0x100
mov ax, 0x1234
push ax
mov ax, 0x0000
pop ax
hlt
