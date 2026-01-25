BITS 16
ORG 0x100
mov ax, 0
mov ds, ax
mov si, 0x2000
mov byte [si], 0xcd
xor ax, ax
lodsb
hlt
