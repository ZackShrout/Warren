//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

// Activate the audited EL1 translation regime, transfer through a register to
// the stable image alias, install the owned vectors and stack, and remove all
// lower-half translation. The register-write boundary contains no call, return,
// or stack access.

.equ ACTIVATION_FAILURE,           79
.equ IDENTITY_REMOVAL_FAILURE,     80

.section .text.burrow_aarch64_activation, "ax", %progbits
.p2align 2
.global burrow_aarch64_activate_translation
.hidden burrow_aarch64_activate_translation
.type burrow_aarch64_activate_translation, %function
burrow_aarch64_activate_translation:
    // Derive both stable image addresses from their physical aliases and the
    // validated loader bias. x25-x28 are callee-saved across the preflight.
    adrp x25, .Lhigher_half
    add x25, x25, :lo12:.Lhigher_half
    ldr x8, [x20, #0x30]
    sub x26, x25, x8
    movz x9, #0x0000
    movk x9, #0x8000, lsl #16
    movk x9, #0xffff, lsl #32
    movk x9, #0xffff, lsl #48
    add x26, x26, x9

    adrp x28, burrow_aarch64_stable_vectors
    add x28, x28, :lo12:burrow_aarch64_stable_vectors
    sub x27, x28, x8
    add x27, x27, x9

    adrp x0, .Lactivation_preflight_anchor
    add x0, x0, :lo12:.Lactivation_preflight_anchor
    mov x1, sp
    mov x2, x25
    mov x3, x26
    mov x4, x28
    mov x5, x27
    bl burrow_aarch64_preflight_activation
.Lactivation_preflight_anchor:
    cbnz w0, .Lactivation_failure

    // Capture every value needed on the far side before beginning the
    // no-call/no-stack activation boundary.
    adrp x8, burrow_aarch64_translation_configuration
    add x8, x8, :lo12:burrow_aarch64_translation_configuration
    ldp x11, x12, [x8, #0]
    ldr x9, [x8, #24]
    ldr x10, [x8, #32]
    ldr x13, [x8, #40]
    adrp x8, burrow_aarch64_empty_root_physical_address
    add x8, x8, :lo12:burrow_aarch64_empty_root_physical_address
    ldr x17, [x8]
    mov x19, x20

    dsb sy
    msr MAIR_EL1, x9
    msr TCR_EL1, x10
    msr TTBR0_EL1, x11
    msr TTBR1_EL1, x12
    isb
    tlbi VMALLE1
    dsb ish
    isb
    msr SCTLR_EL1, x13
    isb
    br x26

// From this label onward every instruction is fetched through TTBR1. The
// physical emergency table remains active only until the stable VBAR write.
.Lhigher_half:
    adrp x0, burrow_aarch64_transition_stage
    add x0, x0, :lo12:burrow_aarch64_transition_stage
    mov x1, #6
    str x1, [x0]

    msr VBAR_EL1, x27
    isb

    movz x16, #0x1000
    movk x16, #0x0001, lsl #16
    movk x16, #0xd000, lsl #32
    movk x16, #0xffff, lsl #48
    mov sp, x16

    movz x24, #0x0000
    movk x24, #0x0900, lsl #16
    movk x24, #0xc000, lsl #32
    movk x24, #0xffff, lsl #48

    movz x0, #0x0000
    movk x0, #0x8000, lsl #32
    movk x0, #0xffff, lsl #48
    add x20, x19, x0

    adrp x0, burrow_aarch64_emergency_console_physical
    add x0, x0, :lo12:burrow_aarch64_emergency_console_physical
    str x24, [x0]

    // Discard physical pointers which were needed only before the high branch.
    mov x8, xzr
    mov x19, xzr
    mov x22, xzr
    mov x25, xzr
    mov x28, xzr

    adrp x0, burrow_aarch64_transition_stage
    add x0, x0, :lo12:burrow_aarch64_transition_stage
    mov x1, #7
    str x1, [x0]

    dsb ishst
    msr TTBR0_EL1, x17
    isb
    tlbi VMALLE1IS
    dsb ish
    isb

    // Prove the complete surviving machine state using only upper-half code,
    // data, vectors, stack, and MMIO references.
    mrs x0, CurrentEL
    cmp x0, #4
    b.ne .Lidentity_removal_failure
    mrs x0, DAIF
    and x0, x0, #0x3c0
    cmp x0, #0x3c0
    b.ne .Lidentity_removal_failure
    mrs x0, TTBR0_EL1
    cmp x0, x17
    b.ne .Lidentity_removal_failure
    mrs x0, TTBR1_EL1
    cmp x0, x12
    b.ne .Lidentity_removal_failure
    mrs x0, VBAR_EL1
    cmp x0, x27
    b.ne .Lidentity_removal_failure
    mov x0, sp
    cmp x0, x16
    b.ne .Lidentity_removal_failure
    mrs x0, TCR_EL1
    cmp x0, x10
    b.ne .Lidentity_removal_failure
    mrs x0, MAIR_EL1
    cmp x0, x9
    b.ne .Lidentity_removal_failure
    mrs x0, SCTLR_EL1
    cmp x0, x13
    b.ne .Lidentity_removal_failure
.Lidentity_proof_pc:
    adr x0, .Lidentity_proof_pc
    tbz x0, #63, .Lidentity_removal_failure
    tbz x20, #63, .Lidentity_removal_failure
    tbz x24, #63, .Lidentity_removal_failure
    tbz x27, #63, .Lidentity_removal_failure

    adrp x0, burrow_aarch64_transition_stage
    add x0, x0, :lo12:burrow_aarch64_transition_stage
    mov x1, #8
    str x1, [x0]

    adrp x25, .Lactivation_diagnostic_prefix
    add x25, x25, :lo12:.Lactivation_diagnostic_prefix
    bl .Lactivation_write_string
    cmp x21, #8
    mov w4, #'1'
    mov w5, #'2'
    csel w4, w5, w4, eq
    bl .Lactivation_write_character
    adrp x25, .Lactivation_diagnostic_suffix
    add x25, x25, :lo12:.Lactivation_diagnostic_suffix
    bl .Lactivation_write_string

#if defined(WARREN_ENABLE_QEMU_TEST_RESULT)
    mov x0, x24
    b burrow_qemu_test_result
#else
    b .Lactivation_wait
#endif

.Lactivation_failure:
    // The preflight returns the stable failure allocation directly.
#if defined(WARREN_ENABLE_QEMU_TEST_RESULT)
    b burrow_qemu_test_failure
#else
    b .Lactivation_wait
#endif

.Lidentity_removal_failure:
    mov w0, #IDENTITY_REMOVAL_FAILURE
#if defined(WARREN_ENABLE_QEMU_TEST_RESULT)
    b burrow_qemu_test_failure_stable
#endif

.Lactivation_wait:
    msr DAIFSet, #0xf
.Lactivation_wait_loop:
    wfe
    b .Lactivation_wait_loop

.Lactivation_write_string:
    mov x28, x30
.Lactivation_write_string_next:
    ldrb w4, [x25], #1
    cbz w4, .Lactivation_write_string_done
    bl .Lactivation_write_character
    b .Lactivation_write_string_next
.Lactivation_write_string_done:
    ret x28

.Lactivation_write_character:
    ldr w5, [x24, #0x18]
    tst w5, #0x20
    b.ne .Lactivation_write_character
    str w4, [x24]
    ret
.size burrow_aarch64_activate_translation, . - burrow_aarch64_activate_translation

.section .rodata.burrow.activation, "a", %progbits
.p2align 3
.Lactivation_diagnostic_prefix:
    .asciz "BURROW_COMMON_EL1:initial=EL"
.Lactivation_diagnostic_suffix:
    .asciz ":normalized=EL1:tables=audited:identity=removed\r\n"

.section .note.GNU-stack, "", %progbits
