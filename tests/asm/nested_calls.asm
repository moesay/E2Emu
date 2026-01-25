BITS 16
ORG 0x100
call outer
hlt
outer:
mov ax, 1
call inner
add ax, 10
ret
inner:
add ax, 100
ret
