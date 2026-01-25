BITS 16
ORG 0x100
mov ax, 5
cmp ax, 5
je equal
mov bx, 1
jmp short done
equal:
mov bx, 2
done:
hlt
