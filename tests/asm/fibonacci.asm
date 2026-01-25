BITS 16
ORG 0x100
mov cx, 8
mov ax, 0
mov bx, 1
loop_start:
mov dx, ax
add dx, bx
mov ax, bx
mov bx, dx
loop loop_start
hlt
