BITS 16
ORG 0x100
; write to vga memory and read it back
mov ax, 0xB800
mov es, ax
mov byte [es:0x0000], 'X'
mov byte [es:0x0001], 0x1F
mov al, [es:0x0000]
mov ah, [es:0x0001]
hlt
