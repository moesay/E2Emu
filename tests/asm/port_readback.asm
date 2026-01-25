BITS 16
ORG 0x100
; write to a port and read it back
mov al, 0xAB
out 0x80, al
in al, 0x80
mov bl, al
hlt
