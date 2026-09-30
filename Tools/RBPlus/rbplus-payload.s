// Experimental, exact Metal UUID F9C20525-B2C6-3CBA-93B1-AE82AFE1946D.
// Two independent, page-local replacements; no relocations or kernel pointers.
.text
.globl _rb_full, _rb_full_end, _rb_delta, _rb_delta_end

// Compute corrected color control in r9d from immutable state rsi.
// Scratch: edx, r10d, r11d, flags. Preserve rcx (delta arg4).


// Replace [image+0x26abc, image+0x26b8e), then fall through.
_rb_full:
    pushq %rbp
    movq %rsp, %rbp
    movq %rdx, %rax
    movl (%rsi), %edx
    movl 4(%rsi), %r8d
    testb $2, 0xb0(%rdi)
    jz 1f
    andl %r8d, %edx
1:  movabsq $0x8ec0026900, %r10
    movq %r10, (%rax)
    movl %edx, 8(%rax)
    movl %r8d, 12(%rax)
    movabsq $0x1e0c0086900, %r10
    movq %r10, 16(%rax)
    vmovdqu 8(%rsi), %ymm0
    vmovdqu %ymm0, 24(%rax)
    movabsq $0x3c0026900, %r10
    movq %r10, 56(%rax)
    movq 0x2c(%rsi), %rdx
    movq %rdx, 64(%rax)
        movl 0x28(%rsi), %r9d
    movl 8(%rsi), %r10d
    testl $0x40000000, %r10d
    jz 9f
    movl $0x2, %r11d
    testl $0x20000000, %r10d
    jz 1f
    movl $0x4, %r11d
1:  movl %r10d, %edx
    andl $0x1f, %edx
    subl $0xf, %edx
    cmpl $0x3, %edx
    jbe 2f
    shrl $0x8, %r10d
    decl %r11d
    jnz 1b
    jmp 9f
2:  orl $0x1, %r9d
9:

    movabsq $0x201c0036900, %r10
    movq %r10, 72(%rax)
    movl 0x34(%rsi), %edx
    movl %edx, 80(%rax)
    movl %r9d, 84(%rax)
    movl 0x38(%rsi), %edx
    movl %edx, 88(%rax)
    addq $0x144, %rax
    .space 0xd2-(.-_rb_full), 0x90
_rb_full_end:

// Replace [image+0x2bbb4, image+0x2bcd5), then fall through.
_rb_delta:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %rax
    movq %r8, %rbx
    movq %rdx, %rax
    movl (%rsi), %edx
    movl 4(%rsi), %r8d
    cmpl %r8d, %edx
    je 1f
    testb $2, 0xb0(%rdi)
    jz 1f
    andl %r8d, %edx
    jmp 2f
1:  cmpl %r8d, 4(%rax)
    jne 2f
    cmpl %edx, (%rax)
    je 3f
2:  movabsq $0x8ec0026900, %r10
    movq %r10, (%rbx)
    movl %edx, 8(%rbx)
    movl %r8d, 12(%rbx)
    addq $16, %rbx
3:  vmovdqu 8(%rax), %ymm0
    vpxor 8(%rsi), %ymm0, %ymm0
    vptest %ymm0, %ymm0
    jz 4f
    movabsq $0x1e0c0086900, %r10
    movq %r10, (%rbx)
    vmovdqu 8(%rsi), %ymm0
    vmovdqu %ymm0, 8(%rbx)
    addq $40, %rbx
4:  movq 0x2c(%rsi), %rdx
    cmpq %rdx, 0x2c(%rax)
    je 5f
    movabsq $0x3c0026900, %r10
    movq %r10, (%rbx)
    movq %rdx, 8(%rbx)
    addq $16, %rbx
5:  movl 8(%rsi), %edx
    cmpl %edx, 8(%rax)
    jne 6f
    movl 0x28(%rsi), %edx
    cmpl %edx, 0x28(%rax)
    jne 6f
    movq 0x34(%rsi), %rdx
    cmpq %rdx, 0x34(%rax)
    je 7f
6:      movl 0x28(%rsi), %r9d
    movl 8(%rsi), %r10d
    testl $0x40000000, %r10d
    jz 9f
    movl $0x2, %r11d
    testl $0x20000000, %r10d
    jz 1f
    movl $0x4, %r11d
1:  movl %r10d, %edx
    andl $0x1f, %edx
    subl $0xf, %edx
    cmpl $0x3, %edx
    jbe 2f
    shrl $0x8, %r10d
    decl %r11d
    jnz 1b
    jmp 9f
2:  orl $0x1, %r9d
9:

    movabsq $0x201c0036900, %r10
    movq %r10, (%rbx)
    movl 0x34(%rsi), %edx
    movl %edx, 8(%rbx)
    movl %r9d, 12(%rbx)
    movl 0x38(%rsi), %edx
    movl %edx, 16(%rbx)
    addq $20, %rbx
7:
    .space 0x121-(.-_rb_delta), 0x90
_rb_delta_end:
