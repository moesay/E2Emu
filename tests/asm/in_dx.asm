BITS 16
ORG 0x100
mov dx, 0x3DA
in al, dx
mov bl, al
hlt
