BITS 16
ORG 0x100
mov bx, 0x3000
mov word [bx], 0x5678
mov ax, [bx]
hlt
