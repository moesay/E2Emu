BITS 16
ORG 0x100
; put cursor at column 10, row 5

; high byte
mov dx, 0x3D4
mov al, 0x0E
out dx, al
mov dx, 0x3D5
mov al, 0x01
out dx, al
; low byte
mov dx, 0x3D4
mov al, 0x0F
out dx, al
mov dx, 0x3D5
mov al, 0x9A
out dx, al
hlt
