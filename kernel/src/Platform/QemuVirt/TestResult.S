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
.equ FIRST_ENTRY_FAIL_MAXIMUM, 81

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
.global burrow_qemu_test_failure
.hidden burrow_qemu_test_failure
.type burrow_qemu_test_failure, %function
burrow_qemu_test_failure:
    cmp w0, #FIRST_ENTRY_FAIL_MINIMUM
    b.lo .Linvalid_failure_code_wait
    cmp w0, #FIRST_ENTRY_FAIL_MAXIMUM
    b.hi .Linvalid_failure_code_wait

    mov w19, w0
    movz x24, #QEMU_PL011_BASE_HIGH, lsl #16
    adrp x25, .Lfailure_marker
    add x25, x25, :lo12:.Lfailure_marker

    mov w6, #10
    udiv w7, w19, w6
    msub w8, w7, w6, w19
    add w7, w7, #'0'
    add w8, w8, #'0'
    adrp x6, .Lfailure_code_digits
    add x6, x6, :lo12:.Lfailure_code_digits
    strb w7, [x6]
    strb w8, [x6, #1]

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
    .asciz "WARREN_TEST:1:PASS:burrow-first-entry\r\n"
#elif defined(WARREN_QEMU_TEST_RESULT_FAIL)
    .asciz "WARREN_TEST:1:FAIL:burrow-first-entry:64\r\n"
#elif defined(WARREN_QEMU_TEST_RESULT_PANIC)
    .asciz "WARREN_TEST:1:PANIC:burrow-first-entry:2\r\n"
#endif
.Lexception_marker:
    .asciz "WARREN_TEST:1:PANIC:burrow-first-entry:4\r\n"

.section .data.burrow.qemu_test_result, "aw", %progbits
.p2align 3
.Lfailure_marker:
    .ascii "WARREN_TEST:1:FAIL:burrow-first-entry:"
.Lfailure_code_digits:
    .ascii "00\r\n"
    .byte 0
.p2align 3
.global burrow_qemu_exit_arguments
.hidden burrow_qemu_exit_arguments
burrow_qemu_exit_arguments:
    .quad 0x20026
    .quad WARREN_QEMU_RESULT_CODE

.section .note.GNU-stack, "", %progbits
