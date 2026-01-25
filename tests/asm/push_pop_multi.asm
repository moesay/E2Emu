BITS 16
ORG 0x100
mov ax, 0x1111
mov bx, 0x2222
mov cx, 0x3333
push ax
push bx
push cx
pop ax
pop bx
pop cx
hlt
