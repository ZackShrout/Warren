//
// Created by Zack Shrout on 8/19/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

// Trusted QEMU-only result transport. This object is absent from ordinary
// Burrow products and is selected only by system-test child builds.

.equ PL011_FLAG,       0x18
.equ PL011_BUSY,       0x08
.equ PL011_TX_FULL,    0x20
.equ QEMU_PL011_BASE_HIGH, 0x0900
.equ FIRST_ENTRY_FAIL_MINIMUM, 65
.equ FIRST_ENTRY_FAIL_MAXIMUM, 83

#if defined(WARREN_QEMU_TEST_RESULT_PASS)
.equ WARREN_QEMU_RESULT_CODE, 0
#elif defined(WARREN_QEMU_TEST_RESULT_FAIL)
.equ WARREN_QEMU_RESULT_CODE, 64
#elif defined(WARREN_QEMU_TEST_RESULT_PANIC)
.equ WARREN_QEMU_RESULT_CODE, 2
#else
#error "A QEMU result mode is required"
#endif

.section .text.burrow.qemu_test_result, "ax", %progbits
.p2align 2
.global burrow_qemu_test_result
.hidden burrow_qemu_test_result
.type burrow_qemu_test_result, %function
burrow_qemu_test_result:
    mov x24, x0
    adrp x25, .Lterminal_marker
    add x25, x25, :lo12:.Lterminal_marker
    b burrow_qemu_emit_result
.size burrow_qemu_test_result, . - burrow_qemu_test_result

.p2align 2
.global burrow_qemu_test_exception
.hidden burrow_qemu_test_exception
.type burrow_qemu_test_exception, %function
burrow_qemu_test_exception:
    mov x24, x0
    adrp x25, .Lexception_marker
    add x25, x25, :lo12:.Lexception_marker

    adrp x6, burrow_qemu_exit_arguments
    add x6, x6, :lo12:burrow_qemu_exit_arguments
    mov x5, #4
    str x5, [x6, #8]
    b burrow_qemu_emit_result
.size burrow_qemu_test_exception, . - burrow_qemu_test_exception

.p2align 2
.global burrow_qemu_test_panic
.hidden burrow_qemu_test_panic
.type burrow_qemu_test_panic, %function
burrow_qemu_test_panic:
    mov x24, x1
    cmp w0, #2
    b.eq .Lselect_assertion_marker
    cmp w0, #3
    b.ne .Linvalid_failure_code_wait
    adrp x25, .Lpanic_marker
    add x25, x25, :lo12:.Lpanic_marker
    b .Lpublish_panic_code
.Lselect_assertion_marker:
    adrp x25, .Lassertion_marker
    add x25, x25, :lo12:.Lassertion_marker
.Lpublish_panic_code:
    adrp x6, burrow_qemu_exit_arguments
    add x6, x6, :lo12:burrow_qemu_exit_arguments
    uxtw x5, w0
    str x5, [x6, #8]
    b burrow_qemu_emit_result
.size burrow_qemu_test_panic, . - burrow_qemu_test_panic

.p2align 2
.global burrow_qemu_test_failure
.hidden burrow_qemu_test_failure
.type burrow_qemu_test_failure, %function
burrow_qemu_test_failure:
    movz x24, #QEMU_PL011_BASE_HIGH, lsl #16
    cmp w0, #74
    b.hs .Lselect_normalized_failure
    adrp x25, .Lfirst_entry_failure_marker
    add x25, x25, :lo12:.Lfirst_entry_failure_marker
    adrp x18, .Lfirst_entry_failure_code_digits
    add x18, x18, :lo12:.Lfirst_entry_failure_code_digits
    b .Lencode_failure

.p2align 2
.global burrow_qemu_test_failure_stable
.hidden burrow_qemu_test_failure_stable
.type burrow_qemu_test_failure_stable, %function
burrow_qemu_test_failure_stable:
    movz x24, #0x0000
    movk x24, #0x0900, lsl #16
    movk x24, #0xc000, lsl #32
    movk x24, #0xffff, lsl #48

.Lselect_normalized_failure:
    adrp x25, .Lnormalized_failure_marker
    add x25, x25, :lo12:.Lnormalized_failure_marker
    adrp x18, .Lnormalized_failure_code_digits
    add x18, x18, :lo12:.Lnormalized_failure_code_digits

.Lencode_failure:
    cmp w0, #FIRST_ENTRY_FAIL_MINIMUM
    b.lo .Linvalid_failure_code_wait
    cmp w0, #FIRST_ENTRY_FAIL_MAXIMUM
    b.hi .Linvalid_failure_code_wait

    mov w19, w0

    mov w6, #10
    udiv w7, w19, w6
    msub w8, w7, w6, w19
    add w7, w7, #'0'
    add w8, w8, #'0'
    strb w7, [x18]
    strb w8, [x18, #1]

    adrp x6, burrow_qemu_exit_arguments
    add x6, x6, :lo12:burrow_qemu_exit_arguments
    uxtw x5, w19
    str x5, [x6, #8]
    b burrow_qemu_emit_result

.Linvalid_failure_code_wait:
    msr DAIFSet, #0xf
.Linvalid_failure_code_wait_loop:
    wfe
    b .Linvalid_failure_code_wait_loop
.size burrow_qemu_test_failure, . - burrow_qemu_test_failure
.size burrow_qemu_test_failure_stable, . - burrow_qemu_test_failure_stable

.p2align 2
.global burrow_qemu_emit_result
.hidden burrow_qemu_emit_result
.type burrow_qemu_emit_result, %function
burrow_qemu_emit_result:
    bl .Lwrite_string

.Lwait_for_transmit_drain:
    ldr w5, [x24, #PL011_FLAG]
    tst w5, #PL011_BUSY
    b.ne .Lwait_for_transmit_drain

    mov w0, #0x20
    adrp x1, burrow_qemu_exit_arguments
    add x1, x1, :lo12:burrow_qemu_exit_arguments
    hlt #0xf000

    // A returned semihosting operation is a hang, never a second result.
    msr DAIFSet, #0xf
.Lunexpected_return_wait:
    wfe
    b .Lunexpected_return_wait
.size burrow_qemu_emit_result, . - burrow_qemu_emit_result

.Lwrite_string:
    mov x28, x30
.Lwrite_string_next:
    ldrb w4, [x25], #1
    cbz w4, .Lwrite_string_done
    bl .Lwrite_character
    b .Lwrite_string_next
.Lwrite_string_done:
    ret x28

.Lwrite_character:
    ldr w5, [x24, #PL011_FLAG]
    tst w5, #PL011_TX_FULL
    b.ne .Lwrite_character
    str w4, [x24]
    ret

.section .rodata.burrow.qemu_test_result, "a", %progbits
.p2align 3
.Lterminal_marker:
#if defined(WARREN_QEMU_TEST_RESULT_PASS)
    .asciz "WARREN_TEST:1:PASS:aarch64-normalized-entry\r\n"
#elif defined(WARREN_QEMU_TEST_RESULT_FAIL)
    .asciz "WARREN_TEST:1:FAIL:aarch64-normalized-entry:64\r\n"
#elif defined(WARREN_QEMU_TEST_RESULT_PANIC)
.Lassertion_marker:
    .asciz "WARREN_TEST:1:PANIC:aarch64-normalized-entry:2\r\n"
#endif
.Lexception_marker:
    .asciz "WARREN_TEST:1:PANIC:aarch64-normalized-entry:4\r\n"
#if !defined(WARREN_QEMU_TEST_RESULT_PANIC)
.Lassertion_marker:
    .asciz "WARREN_TEST:1:PANIC:aarch64-normalized-entry:2\r\n"
#endif
.Lpanic_marker:
    .asciz "WARREN_TEST:1:PANIC:aarch64-normalized-entry:3\r\n"

.section .data.burrow.qemu_test_result, "aw", %progbits
.p2align 3
.Lfirst_entry_failure_marker:
    .ascii "WARREN_TEST:1:FAIL:burrow-first-entry:"
.Lfirst_entry_failure_code_digits:
    .ascii "00\r\n"
    .byte 0
.Lnormalized_failure_marker:
    .ascii "WARREN_TEST:1:FAIL:aarch64-normalized-entry:"
.Lnormalized_failure_code_digits:
    .ascii "00\r\n"
    .byte 0
.p2align 3
.global burrow_qemu_exit_arguments
.hidden burrow_qemu_exit_arguments
burrow_qemu_exit_arguments:
    .quad 0x20026
    .quad WARREN_QEMU_RESULT_CODE

.section .note.GNU-stack, "", %progbits
