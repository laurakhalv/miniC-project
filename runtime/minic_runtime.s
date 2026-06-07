.intel_syntax noprefix
.text

.extern printf
.extern strlen
.extern snprintf
.extern fgets
.extern stdin
.extern malloc
.extern memcpy
.extern exit

.globl __minic_rt_print_i64
.globl __minic_rt_print_u64
.globl __minic_rt_print_string
.globl __minic_rt_print_bool
.globl __minic_rt_print_char
.globl __minic_rt_print_f32
.globl __minic_rt_print_f64
.globl __minic_rt_bool_to_string
.globl __minic_rt_char_to_string
.globl __minic_rt_i64_to_string
.globl __minic_rt_u64_to_string
.globl __minic_rt_f32_to_string
.globl __minic_rt_f64_to_string
.globl __minic_rt_f32_neg
.globl __minic_rt_f64_neg
.globl __minic_rt_f32_is_zero
.globl __minic_rt_f64_is_zero
.globl __minic_rt_f32_add
.globl __minic_rt_f32_sub
.globl __minic_rt_f32_mul
.globl __minic_rt_f32_div
.globl __minic_rt_f32_mod
.globl __minic_rt_f64_add
.globl __minic_rt_f64_sub
.globl __minic_rt_f64_mul
.globl __minic_rt_f64_div
.globl __minic_rt_f64_mod
.globl __minic_rt_f32_eq
.globl __minic_rt_f32_ne
.globl __minic_rt_f32_lt
.globl __minic_rt_f32_le
.globl __minic_rt_f32_gt
.globl __minic_rt_f32_ge
.globl __minic_rt_f64_eq
.globl __minic_rt_f64_ne
.globl __minic_rt_f64_lt
.globl __minic_rt_f64_le
.globl __minic_rt_f64_gt
.globl __minic_rt_f64_ge
.globl __minic_rt_u64_to_f32
.globl __minic_rt_u64_to_f64
.globl __minic_rt_i64_to_f32
.globl __minic_rt_i64_to_f64
.globl __minic_rt_f32_to_f64
.globl __minic_rt_f64_to_f32
.globl __minic_rt_f32_to_i64
.globl __minic_rt_f64_to_i64
.globl __minic_rt_f32_to_u64
.globl __minic_rt_f64_to_u64
.globl __minic_rt_panic
.globl __minic_rt_div_zero
.globl __minic_rt_index_out_of_bounds
.globl __minic_rt_input
.globl __minic_rt_concat

__minic_rt_print_i64:
    push rbp
    mov rbp, rsp
    mov rsi, rdi
    lea rdi, [rip + .Lfmt_i64]
    xor eax, eax
    call printf
    leave
    ret

__minic_rt_print_u64:
    push rbp
    mov rbp, rsp
    mov rsi, rdi
    lea rdi, [rip + .Lfmt_u64]
    xor eax, eax
    call printf
    leave
    ret

__minic_rt_print_string:
    push rbp
    mov rbp, rsp
    mov rsi, rdi
    lea rdi, [rip + .Lfmt_str]
    xor eax, eax
    call printf
    leave
    ret

__minic_rt_print_bool:
    push rbp
    mov rbp, rsp
    cmp rdi, 0
    jne .Lprint_true
    lea rdi, [rip + .Lfalse_str]
    call __minic_rt_print_string
    leave
    ret
.Lprint_true:
    lea rdi, [rip + .Ltrue_str]
    call __minic_rt_print_string
    leave
    ret

__minic_rt_print_char:
    push rbp
    mov rbp, rsp
    mov esi, edi
    lea rdi, [rip + .Lfmt_char]
    xor eax, eax
    call printf
    leave
    ret

__minic_rt_print_f32:
    push rbp
    mov rbp, rsp
    movd xmm0, edi
    cvtss2sd xmm0, xmm0
    lea rdi, [rip + .Lfmt_f32]
    mov eax, 1
    call printf
    leave
    ret

__minic_rt_print_f64:
    push rbp
    mov rbp, rsp
    movq xmm0, rdi
    lea rdi, [rip + .Lfmt_f64]
    mov eax, 1
    call printf
    leave
    ret

__minic_rt_bool_to_string:
    push rbp
    mov rbp, rsp
    cmp rdi, 0
    jne .Lbool_to_string_true
    lea rax, [rip + .Lfalse_str]
    leave
    ret
.Lbool_to_string_true:
    lea rax, [rip + .Ltrue_str]
    leave
    ret

__minic_rt_char_to_string:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov dword ptr [rbp - 4], edi
    mov edi, 2
    call malloc
    mov rcx, rax
    mov edx, dword ptr [rbp - 4]
    mov byte ptr [rcx], dl
    mov byte ptr [rcx + 1], 0
    mov rax, rcx
    leave
    ret

__minic_rt_i64_to_string:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov qword ptr [rbp - 8], rdi
    mov edi, 64
    call malloc
    mov qword ptr [rbp - 16], rax
    mov rdi, rax
    mov esi, 64
    lea rdx, [rip + .Lfmt_i64]
    mov rcx, qword ptr [rbp - 8]
    xor eax, eax
    call snprintf
    mov rax, qword ptr [rbp - 16]
    leave
    ret

__minic_rt_u64_to_string:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov qword ptr [rbp - 8], rdi
    mov edi, 64
    call malloc
    mov qword ptr [rbp - 16], rax
    mov rdi, rax
    mov esi, 64
    lea rdx, [rip + .Lfmt_u64]
    mov rcx, qword ptr [rbp - 8]
    xor eax, eax
    call snprintf
    mov rax, qword ptr [rbp - 16]
    leave
    ret

__minic_rt_f32_to_string:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov dword ptr [rbp - 4], edi
    mov edi, 64
    call malloc
    mov qword ptr [rbp - 16], rax
    mov rdi, rax
    mov esi, 64
    lea rdx, [rip + .Lfmt_f32]
    movd xmm0, dword ptr [rbp - 4]
    cvtss2sd xmm0, xmm0
    mov eax, 1
    call snprintf
    mov rax, qword ptr [rbp - 16]
    leave
    ret

__minic_rt_f64_to_string:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov qword ptr [rbp - 8], rdi
    mov edi, 64
    call malloc
    mov qword ptr [rbp - 16], rax
    mov rdi, rax
    mov esi, 64
    lea rdx, [rip + .Lfmt_f64]
    movq xmm0, qword ptr [rbp - 8]
    mov eax, 1
    call snprintf
    mov rax, qword ptr [rbp - 16]
    leave
    ret

__minic_rt_f32_neg:
    mov eax, edi
    xor eax, 0x80000000
    ret

__minic_rt_f64_neg:
    mov rax, rdi
    btc rax, 63
    ret

__minic_rt_f32_is_zero:
    mov eax, edi
    and eax, 0x7fffffff
    cmp eax, 0
    sete al
    movzx rax, al
    ret

__minic_rt_f64_is_zero:
    mov rax, rdi
    shl rax, 1
    cmp rax, 0
    sete al
    movzx rax, al
    ret

__minic_rt_f32_add:
    movd xmm0, edi
    movd xmm1, esi
    addss xmm0, xmm1
    movd eax, xmm0
    ret

__minic_rt_f32_sub:
    movd xmm0, edi
    movd xmm1, esi
    subss xmm0, xmm1
    movd eax, xmm0
    ret

__minic_rt_f32_mul:
    movd xmm0, edi
    movd xmm1, esi
    mulss xmm0, xmm1
    movd eax, xmm0
    ret

__minic_rt_f32_div:
    movd xmm0, edi
    movd xmm1, esi
    divss xmm0, xmm1
    movd eax, xmm0
    ret

__minic_rt_f32_mod:
    sub rsp, 16
    movd xmm0, edi
    movd xmm1, esi
    movss dword ptr [rsp], xmm0
    movss dword ptr [rsp + 4], xmm1
    fld dword ptr [rsp + 4]
    fld dword ptr [rsp]
.Lminic_f32_mod_loop:
    fprem
    fstsw ax
    sahf
    jp .Lminic_f32_mod_loop
    fstp st(1)
    fstp dword ptr [rsp]
    movss xmm0, dword ptr [rsp]
    movd eax, xmm0
    add rsp, 16
    ret

__minic_rt_f64_add:
    movq xmm0, rdi
    movq xmm1, rsi
    addsd xmm0, xmm1
    movq rax, xmm0
    ret

__minic_rt_f64_sub:
    movq xmm0, rdi
    movq xmm1, rsi
    subsd xmm0, xmm1
    movq rax, xmm0
    ret

__minic_rt_f64_mul:
    movq xmm0, rdi
    movq xmm1, rsi
    mulsd xmm0, xmm1
    movq rax, xmm0
    ret

__minic_rt_f64_div:
    movq xmm0, rdi
    movq xmm1, rsi
    divsd xmm0, xmm1
    movq rax, xmm0
    ret

__minic_rt_f64_mod:
    sub rsp, 16
    movq xmm0, rdi
    movq xmm1, rsi
    movsd qword ptr [rsp], xmm0
    movsd qword ptr [rsp + 8], xmm1
    fld qword ptr [rsp + 8]
    fld qword ptr [rsp]
.Lminic_f64_mod_loop:
    fprem
    fstsw ax
    sahf
    jp .Lminic_f64_mod_loop
    fstp st(1)
    fstp qword ptr [rsp]
    movsd xmm0, qword ptr [rsp]
    movq rax, xmm0
    add rsp, 16
    ret

__minic_rt_f32_eq:
    movd xmm0, edi
    movd xmm1, esi
    ucomiss xmm0, xmm1
    sete al
    setnp cl
    and al, cl
    movzx rax, al
    ret

__minic_rt_f32_ne:
    movd xmm0, edi
    movd xmm1, esi
    ucomiss xmm0, xmm1
    setne al
    setp cl
    or al, cl
    movzx rax, al
    ret

__minic_rt_f32_lt:
    movd xmm0, edi
    movd xmm1, esi
    ucomiss xmm0, xmm1
    setb al
    setnp cl
    and al, cl
    movzx rax, al
    ret

__minic_rt_f32_le:
    movd xmm0, edi
    movd xmm1, esi
    ucomiss xmm0, xmm1
    setbe al
    setnp cl
    and al, cl
    movzx rax, al
    ret

__minic_rt_f32_gt:
    movd xmm0, edi
    movd xmm1, esi
    ucomiss xmm0, xmm1
    seta al
    setnp cl
    and al, cl
    movzx rax, al
    ret

__minic_rt_f32_ge:
    movd xmm0, edi
    movd xmm1, esi
    ucomiss xmm0, xmm1
    setae al
    setnp cl
    and al, cl
    movzx rax, al
    ret

__minic_rt_f64_eq:
    movq xmm0, rdi
    movq xmm1, rsi
    ucomisd xmm0, xmm1
    sete al
    setnp cl
    and al, cl
    movzx rax, al
    ret

__minic_rt_f64_ne:
    movq xmm0, rdi
    movq xmm1, rsi
    ucomisd xmm0, xmm1
    setne al
    setp cl
    or al, cl
    movzx rax, al
    ret

__minic_rt_f64_lt:
    movq xmm0, rdi
    movq xmm1, rsi
    ucomisd xmm0, xmm1
    setb al
    setnp cl
    and al, cl
    movzx rax, al
    ret

__minic_rt_f64_le:
    movq xmm0, rdi
    movq xmm1, rsi
    ucomisd xmm0, xmm1
    setbe al
    setnp cl
    and al, cl
    movzx rax, al
    ret

__minic_rt_f64_gt:
    movq xmm0, rdi
    movq xmm1, rsi
    ucomisd xmm0, xmm1
    seta al
    setnp cl
    and al, cl
    movzx rax, al
    ret

__minic_rt_f64_ge:
    movq xmm0, rdi
    movq xmm1, rsi
    ucomisd xmm0, xmm1
    setae al
    setnp cl
    and al, cl
    movzx rax, al
    ret

__minic_rt_u64_to_f32:
    test rdi, rdi
    jns .Lu64_to_f32_signed
    mov rax, rdi
    shr rax, 1
    mov rcx, rdi
    and rcx, 1
    or rax, rcx
    cvtsi2ss xmm0, rax
    addss xmm0, xmm0
    movd eax, xmm0
    ret
.Lu64_to_f32_signed:
    cvtsi2ss xmm0, rdi
    movd eax, xmm0
    ret

__minic_rt_u64_to_f64:
    test rdi, rdi
    jns .Lu64_to_f64_signed
    mov rax, rdi
    shr rax, 1
    mov rcx, rdi
    and rcx, 1
    or rax, rcx
    cvtsi2sd xmm0, rax
    addsd xmm0, xmm0
    movq rax, xmm0
    ret
.Lu64_to_f64_signed:
    cvtsi2sd xmm0, rdi
    movq rax, xmm0
    ret

__minic_rt_i64_to_f32:
    cvtsi2ss xmm0, rdi
    movd eax, xmm0
    ret

__minic_rt_i64_to_f64:
    cvtsi2sd xmm0, rdi
    movq rax, xmm0
    ret

__minic_rt_f32_to_f64:
    movd xmm0, edi
    cvtss2sd xmm0, xmm0
    movq rax, xmm0
    ret

__minic_rt_f64_to_f32:
    movq xmm0, rdi
    cvtsd2ss xmm0, xmm0
    movd eax, xmm0
    ret

__minic_rt_f32_to_i64:
    movd xmm0, edi
    cvttss2si rax, xmm0
    ret

__minic_rt_f64_to_i64:
    movq xmm0, rdi
    cvttsd2si rax, xmm0
    ret

__minic_rt_f32_to_u64:
    movd xmm0, edi
    cvtss2sd xmm0, xmm0
    movsd xmm1, qword ptr [rip + .Lf64_twopow63]
    ucomisd xmm0, xmm1
    jb .Lf32_to_u64_small
    subsd xmm0, xmm1
    cvttsd2si rax, xmm0
    bts rax, 63
    ret
.Lf32_to_u64_small:
    cvttsd2si rax, xmm0
    ret

__minic_rt_f64_to_u64:
    movq xmm0, rdi
    movsd xmm1, qword ptr [rip + .Lf64_twopow63]
    ucomisd xmm0, xmm1
    jb .Lf64_to_u64_small
    subsd xmm0, xmm1
    cvttsd2si rax, xmm0
    bts rax, 63
    ret
.Lf64_to_u64_small:
    cvttsd2si rax, xmm0
    ret

__minic_rt_panic:
    push rbp
    mov rbp, rsp
    mov rsi, rdi
    lea rdi, [rip + .Lfmt_panic]
    xor eax, eax
    call printf
    mov edi, 1
    call exit

__minic_rt_div_zero:
    push rbp
    mov rbp, rsp
    mov rsi, rdi
    lea rdi, [rip + .Lfmt_div_zero]
    xor eax, eax
    call printf
    mov edi, 1
    call exit

__minic_rt_index_out_of_bounds:
    push rbp
    mov rbp, rsp
    mov rsi, rdi
    lea rdi, [rip + .Lfmt_bounds]
    xor eax, eax
    call printf
    mov edi, 1
    call exit

__minic_rt_input:
    push rbp
    mov rbp, rsp
    sub rsp, 16
    mov edi, 4096
    call malloc
    mov qword ptr [rbp - 8], rax
    mov rdi, rax
    mov esi, 4096
    mov rdx, qword ptr [rip + stdin]
    call fgets
    cmp rax, 0
    jne .Linput_ok
    mov rax, qword ptr [rbp - 8]
    mov byte ptr [rax], 0
    leave
    ret
.Linput_ok:
    mov rdi, qword ptr [rbp - 8]
    call strlen
    cmp rax, 0
    je .Linput_done
    mov rcx, qword ptr [rbp - 8]
    mov rdx, rax
    dec rdx
    cmp byte ptr [rcx + rdx], 10
    jne .Linput_done
    mov byte ptr [rcx + rdx], 0
.Linput_done:
    mov rax, qword ptr [rbp - 8]
    leave
    ret

__minic_rt_concat:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov qword ptr [rbp - 8], rdi
    mov qword ptr [rbp - 16], rsi
    mov rdi, qword ptr [rbp - 8]
    call strlen
    mov qword ptr [rbp - 24], rax
    mov rdi, qword ptr [rbp - 16]
    call strlen
    mov qword ptr [rbp - 32], rax
    mov rax, qword ptr [rbp - 24]
    add rax, qword ptr [rbp - 32]
    add rax, 1
    mov rdi, rax
    call malloc
    mov rcx, rax
    mov rdi, rcx
    mov rsi, qword ptr [rbp - 8]
    mov rdx, qword ptr [rbp - 24]
    call memcpy
    mov rdi, rcx
    add rdi, qword ptr [rbp - 24]
    mov rsi, qword ptr [rbp - 16]
    mov rdx, qword ptr [rbp - 32]
    call memcpy
    mov rax, rcx
    mov rdx, qword ptr [rbp - 24]
    add rdx, qword ptr [rbp - 32]
    mov byte ptr [rax + rdx], 0
    leave
    ret

.section .rodata
.Lfmt_i64:
    .asciz "%lld"
.Lfmt_u64:
    .asciz "%llu"
.Lfmt_str:
    .asciz "%s"
.Lfmt_char:
    .asciz "%c"
.Lfmt_f32:
    .asciz "%.9g"
.Lfmt_f64:
    .asciz "%.17g"
.Lfmt_panic:
    .asciz "panic: %s\n"
.Lfmt_div_zero:
    .asciz "runtime error: division by zero at line %lld\n"
.Lfmt_bounds:
    .asciz "runtime error: index out of bounds at line %lld\n"
.Ltrue_str:
    .asciz "true"
.Lfalse_str:
    .asciz "false"
.Lf64_twopow63:
    .quad 0x43e0000000000000
.section .note.GNU-stack,"",@progbits

