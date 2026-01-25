BITS 16
ORG 0x100
mov ax, 10
cmp ax, 20
jl less
mov bx, 1
jmp short done
less:
mov bx, 2
done:
hlt
