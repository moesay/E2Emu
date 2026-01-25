BITS 16
ORG 0x100
mov cx, 5
mov ax, 0
loop_start:
add ax, 10
loop loop_start
hlt
