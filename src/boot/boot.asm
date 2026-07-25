section .multiboot_header
align 8
header_start:
    dd 0xE85250D6
    dd 0
    dd header_end - header_start
    dd 0x100000000 - (0xE85250D6 + 0 + (header_end - header_start))

align 8
    dw 5
    dw 0
    dd 20
%ifdef BOOT_RES_W
    dd BOOT_RES_W
%else
    dd 1024
%endif
%ifdef BOOT_RES_H
    dd BOOT_RES_H
%else
    dd 768
%endif
%ifdef BOOT_RES_D
    dd BOOT_RES_D
%else
    dd 32
%endif

align 8
    dw 6
    dw 0
    dd 8

align 8
    dw 0
    dw 0
    dd 8
header_end:

section .bss
align 4096
pml4_table:
    resb 4096
pdpt_table:
    resb 4096
pd_table:
    resb 16384
align 16
stack_bottom:
    resb 16384
stack_top:

section .rodata
align 8
gdt64:
    dq 0
.code: equ $ - gdt64
    dq 0x00209A0000000000
.data: equ $ - gdt64
    dq 0x0000920000000000
.pointer:
    dw $ - gdt64 - 1
    dq gdt64

section .data
align 4
mb_magic_save:
    dd 0
mb_info_save:
    dd 0

section .text
default rel
bits 32
global _start
extern kmain
_start:
    mov esp, stack_top
    mov [mb_magic_save], eax
    mov [mb_info_save], ebx

    mov eax, pdpt_table
    or eax, 0x3
    mov [pml4_table], eax

    xor ecx, ecx
.set_pdpt:
    mov eax, ecx
    shl eax, 12
    add eax, pd_table
    or eax, 0x3
    mov [pdpt_table + ecx * 8], eax
    inc ecx
    cmp ecx, 4
    jne .set_pdpt

    xor ecx, ecx
.map_pd:
    mov eax, 0x200000
    mul ecx
    or eax, 0x83
    mov [pd_table + ecx * 8], eax
    inc ecx
    cmp ecx, 2048
    jne .map_pd

    mov eax, pml4_table
    mov cr3, eax

    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    lgdt [gdt64.pointer]
    jmp gdt64.code:long_mode_start

bits 64
long_mode_start:
    mov ax, gdt64.data
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    fninit

    mov rax, cr0
    and rax, ~(1 << 2)
    or rax, (1 << 1)
    mov cr0, rax

    mov rax, cr4
    or rax, (3 << 9)
    mov cr4, rax

    mov rsp, stack_top
    xor rdi, rdi
    xor rsi, rsi
    mov edi, [mb_magic_save]
    mov esi, [mb_info_save]
    call kmain
.halt:
    cli
    hlt
    jmp .halt
