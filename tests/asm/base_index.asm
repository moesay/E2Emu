BITS 16
ORG 0x100
mov bx, 0x3000
mov si, 0x0010
mov word [bx+si], 0xfedc
mov ax, [bx+si]
hlt
