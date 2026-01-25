BITS 16
ORG 0x100
mov ax, 0
mov ds, ax
mov word [0x2000], 30
mov word [0x2002], 10
mov word [0x2004], 20
mov cx, 2
outer:
push cx
mov si, 0x2000
mov cx, 2
inner:
mov ax, [si]
mov bx, [si+2]
cmp ax, bx
jle no_swap
mov [si], bx
mov [si+2], ax
no_swap:
add si, 2
loop inner
pop cx
loop outer
mov ax, [0x2000]
mov bx, [0x2002]
mov dx, [0x2004]
hlt
