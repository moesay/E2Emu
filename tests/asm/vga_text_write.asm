BITS 16
ORG 0x100
; write "Hi" to the screen
mov ax, 0xB800
mov es, ax
mov byte [es:0x0000], 'H'
mov byte [es:0x0001], 0x07
mov byte [es:0x0002], 'i'
mov byte [es:0x0003], 0x07
hlt
