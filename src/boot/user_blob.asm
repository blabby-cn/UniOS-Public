bits 64

section .rodata

global user_blob_start
global user_blob_end

user_blob_start:
    xor r12, r12
.loop:
    mov rax, 1
    lea rdi, [rel .msg]
    mov rsi, .msg_len
    int 0x80
    mov rax, 2
    int 0x80
    inc r12
    cmp r12, 12
    jl .loop
    mov rax, 4
    xor rdi, rdi
    int 0x80
.hang:
    jmp .hang
.msg:
    db "ring3 user process alive via syscall", 10
.msg_len equ $ - .msg
user_blob_end:
