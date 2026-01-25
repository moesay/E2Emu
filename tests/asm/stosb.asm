BITS 16
ORG 0x100
mov ax, 0
mov es, ax
mov di, 0x3000
mov al, 0xab
stosb
hlt
