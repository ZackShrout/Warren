//
// Created by Zack Shrout on 8/20/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

// Normalize inherited EL1 and EL2 execution into one physical, MMU-off EL1h
// state. Entry.S has already validated the physical image, stack, boot object,
// console, and immutable transition plan. The captured values remain in:
//   x20 boot-information physical address
//   x21 initial CurrentEL
//   x22 physical bootstrap-stack top
//   x23 initial DAIF
//   x24 validated physical PL011 address

.equ SCTLR_M_C_I,                    0x1005
.equ SCTLR_EL1_DISABLED_BASELINE,    0x30d00800
.equ HCR_EL2_AARCH64_EL1,            0x80000000
.equ CNTHCTL_EL2_EL1_PHYSICAL_TIMER, 0x3
.equ SPSR_EL2_EL1H_MASKED,           0x3c5
.equ NORMALIZATION_FAILURE,          76

.section .text.burrow_aarch64_normalization, "ax", %progbits
.p2align 2
.global burrow_aarch64_normalize
.hidden burrow_aarch64_normalize
.type burrow_aarch64_normalize, %function
burrow_aarch64_normalize:
    cmp x21, #4
    b.eq .Lnormalize_from_el1
    cmp x21, #8
    b.eq .Lnormalize_from_el2
    b .Lnormalization_failure

// This call completes before either route changes its current SCTLR. The
// routine uses only x0-x15 and x30, preserving the captured x20-x24 state.
.Lnormalize_from_el1:
    bl .Lclean_invalidate_data_caches

    mrs x0, SCTLR_EL1
    mov x1, #SCTLR_M_C_I
    bic x0, x0, x1
    msr SCTLR_EL1, x0
    isb
    ic IALLU
    dsb sy
    isb

    msr CPACR_EL1, xzr
    msr CNTKCTL_EL1, xzr
    isb
    b burrow_aarch64_common_el1

.Lnormalize_from_el2:
    bl .Lclean_invalidate_data_caches

    mrs x0, SCTLR_EL2
    mov x1, #SCTLR_M_C_I
    bic x0, x0, x1
    msr SCTLR_EL2, x0
    isb
    ic IALLU
    dsb sy
    isb

    mov x0, #HCR_EL2_AARCH64_EL1
    msr HCR_EL2, x0
    msr CPTR_EL2, xzr
    mov x0, #CNTHCTL_EL2_EL1_PHYSICAL_TIMER
    msr CNTHCTL_EL2, x0
    msr CNTVOFF_EL2, xzr

    movz x0, #(SCTLR_EL1_DISABLED_BASELINE & 0xffff)
    movk x0, #((SCTLR_EL1_DISABLED_BASELINE >> 16) & 0xffff), lsl #16
    msr SCTLR_EL1, x0
    msr CPACR_EL1, xzr
    msr CNTKCTL_EL1, xzr

    adrp x0, burrow_aarch64_emergency_vectors
    add x0, x0, :lo12:burrow_aarch64_emergency_vectors
    msr VBAR_EL1, x0
    msr SP_EL1, x22

    adrp x0, burrow_aarch64_common_el1
    add x0, x0, :lo12:burrow_aarch64_common_el1
    msr ELR_EL2, x0
    mov x0, #SPSR_EL2_EL1H_MASKED
    msr SPSR_EL2, x0
    dsb sy
    isb
    eret

.p2align 2
.global burrow_aarch64_common_el1
.hidden burrow_aarch64_common_el1
.type burrow_aarch64_common_el1, %function
burrow_aarch64_common_el1:
    mrs x0, CurrentEL
    cmp x0, #4
    b.ne .Lnormalization_failure

    mrs x0, DAIF
    and x0, x0, #0x3c0
    cmp x0, #0x3c0
    b.ne .Lnormalization_failure

    mov x0, sp
    cmp x0, x22
    b.ne .Lnormalization_failure

    mrs x0, SCTLR_EL1
    mov x1, #SCTLR_M_C_I
    tst x0, x1
    b.ne .Lnormalization_failure

    adrp x0, burrow_image_start
    add x0, x0, :lo12:burrow_image_start
    ldr x1, [x20, #0x30]
    cmp x0, x1
    b.ne .Lnormalization_failure

    mrs x0, VBAR_EL1
    adrp x1, burrow_aarch64_emergency_vectors
    add x1, x1, :lo12:burrow_aarch64_emergency_vectors
    cmp x0, x1
    b.ne .Lnormalization_failure

    adrp x0, burrow_aarch64_transition_stage
    add x0, x0, :lo12:burrow_aarch64_transition_stage
    mov x1, #3
    str x1, [x0]

    mrs x0, ID_AA64MMFR0_EL1
    bl burrow_aarch64_build_transition_tables
    cbnz w0, .Lreport_normalization_failure

    adrp x25, .Lcommon_el1_diagnostic_prefix
    add x25, x25, :lo12:.Lcommon_el1_diagnostic_prefix
    bl .Lnormalization_write_string
    cmp x21, #8
    mov w4, #'1'
    mov w5, #'2'
    csel w4, w5, w4, eq
    bl .Lnormalization_write_character
    adrp x25, .Lcommon_el1_diagnostic_suffix
    add x25, x25, :lo12:.Lcommon_el1_diagnostic_suffix
    bl .Lnormalization_write_string

#if defined(WARREN_ENABLE_QEMU_TEST_RESULT)
    mov x0, x24
    b burrow_qemu_test_result
#else
    b .Lnormalization_wait
#endif

.Lnormalization_failure:
    mov w0, #NORMALIZATION_FAILURE
.Lreport_normalization_failure:
#if defined(WARREN_ENABLE_QEMU_TEST_RESULT)
    b burrow_qemu_test_failure
#endif

.Lnormalization_wait:
    msr DAIFSet, #0xf
.Lnormalization_wait_loop:
    wfe
    b .Lnormalization_wait_loop
.size burrow_aarch64_common_el1, . - burrow_aarch64_common_el1

// Clean and invalidate every implemented data or unified cache by set/way.
// CLIDR.LoC bounds the walk; legacy ARMv8.0 CCSIDR fields provide line size,
// associativity, and set counts for the Cortex-A57 baseline.
.p2align 2
.type .Lclean_invalidate_data_caches, %function
.Lclean_invalidate_data_caches:
    mrs x0, CLIDR_EL1
    ubfx x1, x0, #24, #3
    cbz x1, .Lcache_walk_complete
    lsl x11, x1, #1
    mov x10, #0

.Lcache_level_loop:
    lsr x2, x0, x10
    and x2, x2, #7
    cmp x2, #2
    b.lo .Lcache_next_level

    msr CSSELR_EL1, x10
    isb
    mrs x2, CCSIDR_EL1
    and x3, x2, #7
    add x3, x3, #4
    ubfx x4, x2, #3, #10
    clz w5, w4
    ubfx x6, x2, #13, #15

.Lcache_way_loop:
    lsl x7, x4, x5
    orr x7, x7, x10
    mov x8, x6
.Lcache_set_loop:
    lsl x9, x8, x3
    orr x9, x9, x7
    dc CISW, x9
    subs x8, x8, #1
    b.ge .Lcache_set_loop
    subs x4, x4, #1
    b.ge .Lcache_way_loop

.Lcache_next_level:
    add x10, x10, #2
    cmp x10, x11
    b.lo .Lcache_level_loop

.Lcache_walk_complete:
    msr CSSELR_EL1, xzr
    dsb sy
    isb
    ret
.size .Lclean_invalidate_data_caches, . - .Lclean_invalidate_data_caches

.Lnormalization_write_string:
    mov x28, x30
.Lnormalization_write_string_next:
    ldrb w4, [x25], #1
    cbz w4, .Lnormalization_write_string_done
    bl .Lnormalization_write_character
    b .Lnormalization_write_string_next
.Lnormalization_write_string_done:
    ret x28

.Lnormalization_write_character:
    ldr w5, [x24, #0x18]
    tst w5, #0x20
    b.ne .Lnormalization_write_character
    str w4, [x24]
    ret
.size burrow_aarch64_normalize, . - burrow_aarch64_normalize

.section .rodata.burrow.normalization, "a", %progbits
.p2align 3
.Lcommon_el1_diagnostic_prefix:
    .asciz "BURROW_COMMON_EL1:initial=EL"
.Lcommon_el1_diagnostic_suffix:
    .asciz ":normalized=EL1:tables=audited\r\n"

.section .note.GNU-stack, "", %progbits
