/*
 * Created by Zack Shrout on 8/13/26.
 * Copyright (c) 2026 BunnySoft. All rights reserved.
 */

/*
 * Initial AArch64 Burrow image layout.
 *
 * The ELF-relative image begins at zero so one loader-selected physical bias
 * describes every loaded byte. Read-only metadata, executable code, and
 * writable data occupy distinct 4 KiB pages and distinct PT_LOAD segments.
 * Debug sections remain available in the symbol image but are never loaded.
 */

OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
ENTRY(burrow_aarch64_entry)

PHDRS
{
    headers PT_LOAD FILEHDR PHDRS FLAGS(4);
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
    dynamic PT_DYNAMIC FLAGS(6);
    relro PT_GNU_RELRO FLAGS(4);
    stack PT_GNU_STACK FLAGS(6);
}

SECTIONS
{
    HIDDEN(burrow_image_start = 0);
    . = SIZEOF_HEADERS;

    .dynsym : ALIGN(8)
    {
        *(.dynsym)
    } :headers

    .gnu.hash : ALIGN(8)
    {
        *(.gnu.hash)
    } :headers

    .hash : ALIGN(8)
    {
        *(.hash)
    } :headers

    .dynstr : ALIGN(1)
    {
        *(.dynstr)
    } :headers

    .rela.dyn : ALIGN(8)
    {
        *(.rela.dyn)
    } :headers

    .rodata : ALIGN(16)
    {
        HIDDEN(burrow_read_only_start = .);
        KEEP(*(.rodata.burrow.layout_sentinel))
        *(.rodata .rodata.*)
        HIDDEN(burrow_read_only_end = .);
    } :headers

    . = ALIGN(0x1000);
    .text : ALIGN(16)
    {
        HIDDEN(burrow_text_start = .);
        KEEP(*(.text.burrow_aarch64_entry))
        KEEP(*(.text.burrow.layout_sentinel))
        *(.text .text.*)
        HIDDEN(burrow_text_end = .);
    } :text

    . = ALIGN(0x1000);
    .data : ALIGN(16)
    {
        HIDDEN(burrow_writable_start = .);
        KEEP(*(.data.burrow.layout_sentinel))
        *(.data .data.*)
    } :data

    .got : ALIGN(8)
    {
        *(.got .got.*)
    } :data

    .dynamic : ALIGN(8)
    {
        *(.dynamic)
    } :data :dynamic :relro

    .bss (NOLOAD) : ALIGN(16)
    {
        KEEP(*(.bss.burrow.layout_sentinel))
        *(.bss .bss.*)
        *(COMMON)
        HIDDEN(burrow_writable_end = .);
    } :data

    HIDDEN(burrow_image_end = .);

    .burrow_forbidden :
    {
        *(.init .init.*)
        *(.fini .fini.*)
        *(.init_array .init_array.*)
        *(.fini_array .fini_array.*)
        *(.preinit_array .preinit_array.*)
        *(.ctors .ctors.*)
        *(.dtors .dtors.*)
        *(.tdata .tdata.*)
        *(.tbss .tbss.*)
        *(.eh_frame .eh_frame.*)
        *(.gcc_except_table .gcc_except_table.*)
    }

    .debug_abbrev 0 : { *(.debug_abbrev .debug_abbrev.*) }
    .debug_addr 0 : { *(.debug_addr .debug_addr.*) }
    .debug_aranges 0 : { *(.debug_aranges .debug_aranges.*) }
    .debug_frame 0 : { *(.debug_frame .debug_frame.*) }
    .debug_info 0 : { *(.debug_info .debug_info.*) }
    .debug_line 0 : { *(.debug_line .debug_line.*) }
    .debug_line_str 0 : { *(.debug_line_str .debug_line_str.*) }
    .debug_loc 0 : { *(.debug_loc .debug_loc.*) }
    .debug_loclists 0 : { *(.debug_loclists .debug_loclists.*) }
    .debug_names 0 : { *(.debug_names .debug_names.*) }
    .debug_pubnames 0 : { *(.debug_pubnames .debug_pubnames.*) }
    .debug_pubtypes 0 : { *(.debug_pubtypes .debug_pubtypes.*) }
    .debug_ranges 0 : { *(.debug_ranges .debug_ranges.*) }
    .debug_rnglists 0 : { *(.debug_rnglists .debug_rnglists.*) }
    .debug_str 0 : { *(.debug_str .debug_str.*) }
    .debug_str_offsets 0 : { *(.debug_str_offsets .debug_str_offsets.*) }

    /DISCARD/ :
    {
        *(.comment)
        *(.note.GNU-stack)
    }

    ASSERT(SIZEOF(.rodata) > 0, "Burrow requires real read-only content")
    ASSERT(SIZEOF(.text) > 0, "Burrow requires real executable content")
    ASSERT(SIZEOF(.data) > 0, "Burrow requires real initialized writable content")
    ASSERT(SIZEOF(.bss) > 0, "Burrow requires real zero-filled content")
    ASSERT(SIZEOF(.burrow_forbidden) == 0, "Burrow contains forbidden runtime sections")
    ASSERT((ADDR(.text) & 0xfff) == 0, "Burrow text is not page-aligned")
    ASSERT((ADDR(.data) & 0xfff) == 0, "Burrow data is not page-aligned")
    ASSERT(burrow_image_end <= 0x80000000, "Burrow exceeds the 2 GiB ELF-relative window")
}
