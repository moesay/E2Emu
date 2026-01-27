BITS 16
ORG 0x100
mov ax, 0x1234
mov bx, 0x1000
mov es, bx
mov di, 0x0020
mov [es:di], ax
