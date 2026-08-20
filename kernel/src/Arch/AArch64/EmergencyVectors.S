//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

// Terminal vectors for the inherited physical execution environment. They use
// no stack, allocation, firmware service, or recoverable return path.

.equ PL011_DATA,              0x00
.equ PL011_FLAG,              0x18
.equ PL011_TX_FULL,           0x20

.equ STATE_ACTIVE,            0x00
.equ STATE_STAGE,             0x08
.equ STATE_VECTOR,            0x10
.equ STATE_CURRENT_EL,        0x18
.equ STATE_ESR,               0x20
.equ STATE_ELR,               0x28
.equ STATE_FAR,               0x30
.equ STATE_SPSR,              0x38
.equ STATE_CONSOLE_READY,     0x40
.equ STATE_CONSOLE_PHYSICAL,  0x48
.equ STATE_SIZE,              0x50

.macro BURROW_EMERGENCY_VECTOR number
    mov x17, #\number
    b burrow_aarch64_emergency_exception
    .space 120
.endm

.section .text.burrow_aarch64_emergency_vectors, "ax", %progbits
.p2align 11
.global burrow_aarch64_emergency_vectors
.hidden burrow_aarch64_emergency_vectors
.type burrow_aarch64_emergency_vectors, %function
burrow_aarch64_emergency_vectors:
    BURROW_EMERGENCY_VECTOR 0
    BURROW_EMERGENCY_VECTOR 1
    BURROW_EMERGENCY_VECTOR 2
    BURROW_EMERGENCY_VECTOR 3
    BURROW_EMERGENCY_VECTOR 4
    BURROW_EMERGENCY_VECTOR 5
    BURROW_EMERGENCY_VECTOR 6
    BURROW_EMERGENCY_VECTOR 7
    BURROW_EMERGENCY_VECTOR 8
    BURROW_EMERGENCY_VECTOR 9
    BURROW_EMERGENCY_VECTOR 10
    BURROW_EMERGENCY_VECTOR 11
    BURROW_EMERGENCY_VECTOR 12
    BURROW_EMERGENCY_VECTOR 13
    BURROW_EMERGENCY_VECTOR 14
    BURROW_EMERGENCY_VECTOR 15
.size burrow_aarch64_emergency_vectors, . - burrow_aarch64_emergency_vectors

// The stable table is deliberately a distinct image-resident table. Before
// activation its physical alias is preflighted; after the high branch VBAR_EL1
// names this table through the owned stable image mapping.
.section .text.burrow_aarch64_stable_vectors, "ax", %progbits
.p2align 11
.global burrow_aarch64_stable_vectors
.hidden burrow_aarch64_stable_vectors
.type burrow_aarch64_stable_vectors, %function
burrow_aarch64_stable_vectors:
    BURROW_EMERGENCY_VECTOR 0
    BURROW_EMERGENCY_VECTOR 1
    BURROW_EMERGENCY_VECTOR 2
    BURROW_EMERGENCY_VECTOR 3
    BURROW_EMERGENCY_VECTOR 4
    BURROW_EMERGENCY_VECTOR 5
    BURROW_EMERGENCY_VECTOR 6
    BURROW_EMERGENCY_VECTOR 7
    BURROW_EMERGENCY_VECTOR 8
    BURROW_EMERGENCY_VECTOR 9
    BURROW_EMERGENCY_VECTOR 10
    BURROW_EMERGENCY_VECTOR 11
    BURROW_EMERGENCY_VECTOR 12
    BURROW_EMERGENCY_VECTOR 13
    BURROW_EMERGENCY_VECTOR 14
    BURROW_EMERGENCY_VECTOR 15
.size burrow_aarch64_stable_vectors, . - burrow_aarch64_stable_vectors

.section .text.burrow_aarch64_emergency_exception, "ax", %progbits
.p2align 2
.global burrow_aarch64_emergency_exception
.hidden burrow_aarch64_emergency_exception
.type burrow_aarch64_emergency_exception, %function
burrow_aarch64_emergency_exception:
    msr DAIFSet, #0xf

    adrp x16, burrow_aarch64_emergency_state
    add x16, x16, :lo12:burrow_aarch64_emergency_state
    ldr x15, [x16, #STATE_ACTIVE]
    cbnz x15, .Lnested_exception_wait
    mov x15, #1
    str x15, [x16, #STATE_ACTIVE]

    str x17, [x16, #STATE_VECTOR]
    mrs x15, CurrentEL
    str x15, [x16, #STATE_CURRENT_EL]
    cmp x15, #4
    b.eq .Lcapture_el1_exception
    cmp x15, #8
    b.ne .Lterminal_exception_wait

    mrs x12, ESR_EL2
    mrs x13, ELR_EL2
    mrs x14, FAR_EL2
    mrs x15, SPSR_EL2
    b .Lexception_state_captured

.Lcapture_el1_exception:
    mrs x12, ESR_EL1
    mrs x13, ELR_EL1
    mrs x14, FAR_EL1
    mrs x15, SPSR_EL1

.Lexception_state_captured:
    stp x12, x13, [x16, #STATE_ESR]
    stp x14, x15, [x16, #STATE_FAR]

    ldr x15, [x16, #STATE_CONSOLE_READY]
    cbz x15, .Lterminal_exception_wait
    ldr x24, [x16, #STATE_CONSOLE_PHYSICAL]
    cbz x24, .Lterminal_exception_wait

    adrp x25, .Lexception_stage
    add x25, x25, :lo12:.Lexception_stage
    bl .Lexception_write_string
    ldr x26, [x16, #STATE_STAGE]
    bl .Lexception_write_small_decimal

    adrp x25, .Lexception_vector
    add x25, x25, :lo12:.Lexception_vector
    bl .Lexception_write_string
    ldr x26, [x16, #STATE_VECTOR]
    bl .Lexception_write_small_decimal

    adrp x25, .Lexception_el
    add x25, x25, :lo12:.Lexception_el
    bl .Lexception_write_string
    ldr x26, [x16, #STATE_CURRENT_EL]
    lsr x26, x26, #2
    bl .Lexception_write_small_decimal

    adrp x25, .Lexception_esr
    add x25, x25, :lo12:.Lexception_esr
    bl .Lexception_write_string
    ldr x26, [x16, #STATE_ESR]
    bl .Lexception_write_hex

    adrp x25, .Lexception_elr
    add x25, x25, :lo12:.Lexception_elr
    bl .Lexception_write_string
    ldr x26, [x16, #STATE_ELR]
    bl .Lexception_write_hex

    adrp x25, .Lexception_far
    add x25, x25, :lo12:.Lexception_far
    bl .Lexception_write_string
    ldr x26, [x16, #STATE_FAR]
    bl .Lexception_write_hex

    adrp x25, .Lexception_spsr
    add x25, x25, :lo12:.Lexception_spsr
    bl .Lexception_write_string
    ldr x26, [x16, #STATE_SPSR]
    bl .Lexception_write_hex

    mov w4, #'\r'
    bl .Lexception_write_character
    mov w4, #'\n'
    bl .Lexception_write_character

#if defined(WARREN_ENABLE_QEMU_TEST_RESULT)
    mov x0, x24
    b burrow_qemu_test_exception
#endif

.Lterminal_exception_wait:
    msr DAIFSet, #0xf
.Lterminal_exception_wait_loop:
    wfe
    b .Lterminal_exception_wait_loop

.Lnested_exception_wait:
    msr DAIFSet, #0xf
.Lnested_exception_wait_loop:
    wfe
    b .Lnested_exception_wait_loop

.Lexception_write_string:
    mov x28, x30
.Lexception_write_string_next:
    ldrb w4, [x25], #1
    cbz w4, .Lexception_write_string_done
    bl .Lexception_write_character
    b .Lexception_write_string_next
.Lexception_write_string_done:
    ret x28

.Lexception_write_small_decimal:
    mov x29, x30
    cmp x26, #10
    b.lo .Lexception_write_decimal_units
    mov w4, #'1'
    bl .Lexception_write_character
    sub x26, x26, #10
.Lexception_write_decimal_units:
    add w4, w26, #'0'
    bl .Lexception_write_character
    ret x29

.Lexception_write_hex:
    mov x29, x30
    mov w27, #60
.Lexception_write_hex_next:
    lsr x4, x26, x27
    and x4, x4, #0xf
    cmp x4, #10
    add x5, x4, #'0'
    add x6, x4, #('A' - 10)
    csel x4, x5, x6, lo
    bl .Lexception_write_character
    subs w27, w27, #4
    b.pl .Lexception_write_hex_next
    ret x29

.Lexception_write_character:
    ldr w5, [x24, #PL011_FLAG]
    tst w5, #PL011_TX_FULL
    b.ne .Lexception_write_character
    str w4, [x24, #PL011_DATA]
    ret
.size burrow_aarch64_emergency_exception, . - burrow_aarch64_emergency_exception

.section .rodata.burrow.emergency_exception, "a", %progbits
.p2align 3
.Lexception_stage:
    .asciz "BURROW_EXCEPTION:stage="
.Lexception_vector:
    .asciz ":vector="
.Lexception_el:
    .asciz ":el="
.Lexception_esr:
    .asciz ":esr=0x"
.Lexception_elr:
    .asciz ":elr=0x"
.Lexception_far:
    .asciz ":far=0x"
.Lexception_spsr:
    .asciz ":spsr=0x"

.section .data.burrow.emergency_exception, "aw", %progbits
.p2align 3
.global burrow_aarch64_emergency_state
.hidden burrow_aarch64_emergency_state
burrow_aarch64_emergency_state:
    .quad 0
.global burrow_aarch64_transition_stage
.hidden burrow_aarch64_transition_stage
burrow_aarch64_transition_stage:
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
.global burrow_aarch64_emergency_console_ready
.hidden burrow_aarch64_emergency_console_ready
burrow_aarch64_emergency_console_ready:
    .quad 0
.global burrow_aarch64_emergency_console_physical
.hidden burrow_aarch64_emergency_console_physical
burrow_aarch64_emergency_console_physical:
    .quad 0
.size burrow_aarch64_emergency_state, . - burrow_aarch64_emergency_state

.if (. - burrow_aarch64_emergency_state) != STATE_SIZE
    .error "AArch64 emergency state layout is inconsistent"
.endif

.section .note.GNU-stack, "", %progbits
