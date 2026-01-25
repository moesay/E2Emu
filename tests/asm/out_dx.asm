BITS 16
ORG 0x100
mov dx, 0x3D4
mov al, 0x0E
out dx, al
hlt
