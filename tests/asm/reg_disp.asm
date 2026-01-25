BITS 16
ORG 0x100
mov bx, 0x3000
mov word [bx+10], 0xabcd
mov ax, [bx+10]
hlt
