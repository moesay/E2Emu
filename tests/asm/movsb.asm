BITS 16
ORG 0x100
mov ax, 0
mov ds, ax
mov es, ax
mov si, 0x2000
mov di, 0x3000
mov byte [si], 0x42
movsb
hlt
