//
// Created by Zack Shrout on 8/13/26.
// Copyright (c) 2026 BunnySoft. All rights reserved.
//

// Burrow's first AArch64 instruction boundary. This code deliberately uses no
// stack, imports, relocatable pointers, firmware services, or C++ runtime.

.equ BOOT_MAGIC,                  0x49424e4552524157
.equ BOOT_MAJOR,                  1
.equ BOOT_HEADER_SIZE,            256
.equ BOOT_PAGE_SIZE,              4096
.equ BOOT_FEATURE_EARLY_CONSOLE,  0x8
.equ BOOT_CONSOLE_PL011,          1
.equ BOOT_CONSOLE_OUTPUT,         1
.equ BOOT_CONSOLE_KNOWN_FLAGS,    3

.equ HEADER_MAJOR,                0x008
.equ HEADER_SIZE,                 0x00c
.equ TOTAL_SIZE,                  0x010
.equ PAGE_SIZE,                   0x014
.equ PRESENT_FEATURES,            0x018
.equ SELF_PHYSICAL_ADDRESS,       0x028
.equ KERNEL_PHYSICAL_START,       0x030
.equ KERNEL_PHYSICAL_SIZE,        0x038
.equ KERNEL_LOAD_BIAS,            0x040
.equ KERNEL_ENTRY,                0x048
.equ STACK_PHYSICAL_START,        0x050
.equ STACK_SIZE,                  0x058
.equ EARLY_CONSOLE_SECTION,       0x0a8

.equ SECTION_OFFSET,              0x00
.equ SECTION_COUNT,               0x04
.equ SECTION_STRIDE,              0x08
.equ SECTION_RESERVED,            0x0c

.equ CONSOLE_KIND,                0x00
.equ CONSOLE_FLAGS,               0x04
.equ CONSOLE_PHYSICAL_ADDRESS,    0x08
.equ CONSOLE_REGISTER_STRIDE,     0x10
.equ CONSOLE_REGISTER_WIDTH,      0x14
.equ CONSOLE_RESERVED_0,          0x24
.equ CONSOLE_RESERVED_1,          0x28
.equ CONSOLE_SIZE,                64

.equ PL011_DATA,                  0x00
.equ PL011_FLAG,                  0x18
.equ PL011_TX_FULL,               0x20

.equ FIRST_ENTRY_FAIL_REGISTERS,        65
.equ FIRST_ENTRY_FAIL_EXCEPTION_LEVEL,  66
.equ FIRST_ENTRY_FAIL_MACHINE_STATE,    67
.equ FIRST_ENTRY_FAIL_HEADER,           68
.equ FIRST_ENTRY_FAIL_IMAGE,            69
.equ FIRST_ENTRY_FAIL_STACK,            70
.equ FIRST_ENTRY_FAIL_CONSOLE_SECTION,  71
.equ FIRST_ENTRY_FAIL_CONSOLE_RECORD,   72

.section .text.burrow_aarch64_entry, "ax", %progbits
.p2align 2
.global burrow_aarch64_entry
.hidden burrow_aarch64_entry
.type burrow_aarch64_entry, %function
burrow_aarch64_entry:
    mov x20, x0
    mov x22, sp
    mrs x21, CurrentEL
    mrs x23, DAIF

    adrp x4, burrow_aarch64_emergency_vectors
    add x4, x4, :lo12:burrow_aarch64_emergency_vectors
    cmp x21, #4
    b.eq .Linstall_el1_vectors
    cmp x21, #8
    b.ne .Lfail_exception_level
    msr VBAR_EL2, x4
    b .Lemergency_vectors_installed
.Linstall_el1_vectors:
    msr VBAR_EL1, x4
.Lemergency_vectors_installed:
    isb

    adrp x4, burrow_aarch64_transition_stage
    add x4, x4, :lo12:burrow_aarch64_transition_stage
    mov x5, #1
    str x5, [x4]

    adrp x4, burrow_first_entry_boot_information
    add x4, x4, :lo12:burrow_first_entry_boot_information
    stp x20, x21, [x4]
    stp x22, x23, [x4, #16]

    cbz x20, .Lfail_registers
    tst x20, #7
    b.ne .Lfail_registers
    orr x4, x1, x2
    orr x4, x4, x3
    cbnz x4, .Lfail_registers
    cmp x21, #4
    b.eq .Lcurrent_el_valid
    cmp x21, #8
    b.ne .Lfail_exception_level
.Lcurrent_el_valid:
    tst x22, #15
    b.ne .Lfail_machine_state
    and x4, x23, #0x3c0
    cmp x4, #0x3c0
    b.ne .Lfail_machine_state

    ldr x4, [x20]
    movz x5, #(BOOT_MAGIC & 0xffff)
    movk x5, #((BOOT_MAGIC >> 16) & 0xffff), lsl #16
    movk x5, #((BOOT_MAGIC >> 32) & 0xffff), lsl #32
    movk x5, #((BOOT_MAGIC >> 48) & 0xffff), lsl #48
    cmp x4, x5
    b.ne .Lfail_header
    ldrh w4, [x20, #HEADER_MAJOR]
    cmp w4, #BOOT_MAJOR
    b.ne .Lfail_header
    ldr w4, [x20, #HEADER_SIZE]
    cmp w4, #BOOT_HEADER_SIZE
    b.ne .Lfail_header
    ldr w19, [x20, #TOTAL_SIZE]
    cmp w19, #BOOT_HEADER_SIZE
    b.lo .Lfail_header
    ldr w4, [x20, #PAGE_SIZE]
    cmp w4, #BOOT_PAGE_SIZE
    b.ne .Lfail_header
    ldr x4, [x20, #PRESENT_FEATURES]
    tst x4, #BOOT_FEATURE_EARLY_CONSOLE
    b.eq .Lfail_header
    ldr x4, [x20, #SELF_PHYSICAL_ADDRESS]
    cmp x4, x20
    b.ne .Lfail_header

    ldr x7, [x20, #KERNEL_PHYSICAL_START]
    ldr x8, [x20, #KERNEL_PHYSICAL_SIZE]
    cbz x8, .Lfail_image
    tst x7, #(BOOT_PAGE_SIZE - 1)
    b.ne .Lfail_image
    tst x8, #(BOOT_PAGE_SIZE - 1)
    b.ne .Lfail_image
    adds x9, x7, x8
    b.cs .Lfail_image
    ldr x10, [x20, #KERNEL_ENTRY]
    adrp x11, burrow_aarch64_entry
    add x11, x11, :lo12:burrow_aarch64_entry
    cmp x10, x11
    b.ne .Lfail_image
    cmp x10, x7
    b.lo .Lfail_image
    cmp x10, x9
    b.hs .Lfail_image
    ldr x4, [x20, #KERNEL_LOAD_BIAS]
    cmp x4, x7
    b.ne .Lfail_image

    ldr x4, [x20, #STACK_PHYSICAL_START]
    ldr x5, [x20, #STACK_SIZE]
    cbz x5, .Lfail_stack
    tst x4, #(BOOT_PAGE_SIZE - 1)
    b.ne .Lfail_stack
    tst x5, #(BOOT_PAGE_SIZE - 1)
    b.ne .Lfail_stack
    adds x4, x4, x5
    b.cs .Lfail_stack
    cmp x4, x22
    b.ne .Lfail_stack

    add x6, x20, #EARLY_CONSOLE_SECTION
    ldr w7, [x6, #SECTION_OFFSET]
    cmp w7, #BOOT_HEADER_SIZE
    b.lo .Lfail_console_section
    tst w7, #7
    b.ne .Lfail_console_section
    ldr w4, [x6, #SECTION_COUNT]
    cmp w4, #1
    b.ne .Lfail_console_section
    ldr w4, [x6, #SECTION_STRIDE]
    cmp w4, #CONSOLE_SIZE
    b.ne .Lfail_console_section
    ldr w4, [x6, #SECTION_RESERVED]
    cbnz w4, .Lfail_console_section
    adds w4, w7, #CONSOLE_SIZE
    b.cs .Lfail_console_section
    cmp w4, w19
    b.hi .Lfail_console_section
    adds x6, x20, x7
    b.cs .Lfail_console_section

    ldr w4, [x6, #CONSOLE_KIND]
    cmp w4, #BOOT_CONSOLE_PL011
    b.ne .Lfail_console_record
    ldr w4, [x6, #CONSOLE_FLAGS]
    tst w4, #BOOT_CONSOLE_OUTPUT
    b.eq .Lfail_console_record
    bic w5, w4, #BOOT_CONSOLE_KNOWN_FLAGS
    cbnz w5, .Lfail_console_record
    ldr x24, [x6, #CONSOLE_PHYSICAL_ADDRESS]
    cbz x24, .Lfail_console_record
    tst x24, #3
    b.ne .Lfail_console_record
    adds x4, x24, #PL011_FLAG
    b.cs .Lfail_console_record
    ldr w4, [x6, #CONSOLE_REGISTER_STRIDE]
    cmp w4, #4
    b.ne .Lfail_console_record
    ldr w4, [x6, #CONSOLE_REGISTER_WIDTH]
    cmp w4, #32
    b.ne .Lfail_console_record
    ldr w4, [x6, #CONSOLE_RESERVED_0]
    cbnz w4, .Lfail_console_record
    ldp x4, x5, [x6, #CONSOLE_RESERVED_1]
    ldr x6, [x6, #(CONSOLE_RESERVED_1 + 16)]
    orr x4, x4, x5
    orr x4, x4, x6
    cbnz x4, .Lfail_console_record

    adrp x4, burrow_aarch64_emergency_console_physical
    add x4, x4, :lo12:burrow_aarch64_emergency_console_physical
    str x24, [x4]
    dmb ish
    adrp x4, burrow_aarch64_emergency_console_ready
    add x4, x4, :lo12:burrow_aarch64_emergency_console_ready
    mov x5, #1
    str x5, [x4]

    adrp x25, .Ldiagnostic_prefix
    add x25, x25, :lo12:.Ldiagnostic_prefix
    bl .Lwrite_string
    cmp x21, #8
    mov w4, #'1'
    mov w5, #'2'
    csel w4, w5, w4, eq
    bl .Lwrite_character
    adrp x25, .Ldiagnostic_boot
    add x25, x25, :lo12:.Ldiagnostic_boot
    bl .Lwrite_string
    mov x26, x20
    mov w27, #60
.Lwrite_hex:
    lsr x4, x26, x27
    and x4, x4, #0xf
    cmp x4, #10
    add x5, x4, #'0'
    add x6, x4, #('A' - 10)
    csel x4, x5, x6, lo
    bl .Lwrite_character
    subs w27, w27, #4
    b.pl .Lwrite_hex
    mov w4, #'\r'
    bl .Lwrite_character
    mov w4, #'\n'
    bl .Lwrite_character

#if defined(WARREN_AARCH64_FAULT_INJECT_EMERGENCY)
    brk #0x777
#endif

#if defined(WARREN_ENABLE_QEMU_TEST_RESULT)
    mov x0, x24
    b burrow_qemu_test_result
#endif

.Lfail_registers:
    mov w0, #FIRST_ENTRY_FAIL_REGISTERS
    b .Lreport_first_entry_failure
.Lfail_exception_level:
    mov w0, #FIRST_ENTRY_FAIL_EXCEPTION_LEVEL
    b .Lreport_first_entry_failure
.Lfail_machine_state:
    mov w0, #FIRST_ENTRY_FAIL_MACHINE_STATE
    b .Lreport_first_entry_failure
.Lfail_header:
    mov w0, #FIRST_ENTRY_FAIL_HEADER
    b .Lreport_first_entry_failure
.Lfail_image:
    mov w0, #FIRST_ENTRY_FAIL_IMAGE
    b .Lreport_first_entry_failure
.Lfail_stack:
    mov w0, #FIRST_ENTRY_FAIL_STACK
    b .Lreport_first_entry_failure
.Lfail_console_section:
    mov w0, #FIRST_ENTRY_FAIL_CONSOLE_SECTION
    b .Lreport_first_entry_failure
.Lfail_console_record:
    mov w0, #FIRST_ENTRY_FAIL_CONSOLE_RECORD
.Lreport_first_entry_failure:
#if defined(WARREN_ENABLE_QEMU_TEST_RESULT)
    b burrow_qemu_test_failure
#else
    b .Lwait
#endif

.Lwait:
    msr DAIFSet, #0xf
    wfe
    b .Lwait

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
    str w4, [x24, #PL011_DATA]
    ret
.size burrow_aarch64_entry, . - burrow_aarch64_entry

.section .rodata.burrow.first_entry, "a", %progbits
.p2align 3
.Ldiagnostic_prefix:
    .asciz "BURROW_FIRST_ENTRY:EL"
.Ldiagnostic_boot:
    .asciz ":boot=0x"

.section .data.burrow.first_entry, "aw", %progbits
.p2align 3
.global burrow_first_entry_boot_information
.hidden burrow_first_entry_boot_information
burrow_first_entry_boot_information:
    .quad 0
.global burrow_first_entry_current_el
.hidden burrow_first_entry_current_el
burrow_first_entry_current_el:
    .quad 0
.global burrow_first_entry_stack_pointer
.hidden burrow_first_entry_stack_pointer
burrow_first_entry_stack_pointer:
    .quad 0
.global burrow_first_entry_daif
.hidden burrow_first_entry_daif
burrow_first_entry_daif:
    .quad 0

.section .note.GNU-stack, "", %progbits
