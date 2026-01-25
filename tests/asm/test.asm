BITS 16
ORG 0x100
mov ax, 0x00ff
mov bx, 0xff00
test ax, bx
hlt
