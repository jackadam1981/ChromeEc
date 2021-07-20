SECTIONS
{
	.text.fips :  ALIGN(4)  SUBALIGN(4)
	{
		. = ALIGN(4);
		/* .text and .rodata hold most program code and immutable constants */
		/* .gnu.linkonce hold C++ elements with vague linkage
		https://gcc.gnu.org/onlinedocs/gcc/Vague-Linkage.html */
		*(SORT_BY_NAME(.text) SORT_BY_NAME(.text.*) .gnu.linkonce.t.*)
		. = ALIGN(4);
	} =0xffffffff
	.rodata.fips : {
		/* combine constant data */
		*(.rodata.str1.* .rodata.str.* .rodata..L__unnamed* .rodata..Lanon.*)
		*(.srodata* .rodata .rodata.* .gnu.linkonce.r.*)
	}
	.data.fips :
	{
		*(.ramfunc .ramfunc.*);
		*(.sdata .sdata.* .gnu.linkonce.r.*)
		*(.data .data.*);
	}
	ASSERT(SIZEOF(.data.fips) == 0, "No .data for FIPS module is allowed")
	.bss.fips (NOLOAD) :
	{
		. = ALIGN(4);
		*(.sbss .sbss.* .bss .bss.*);
		*(COMMON)
		. = ALIGN(4);
	}
	/* Debug data; this is stripped from the final binary. */
	.debug_abbrev 0 : { *(.debug_abbrev) }
	.debug_aranges 0 : { *(.debug_aranges) }
	.debug_frame 0 : { *(.debug_frame) }
	.debug_info 0 : { *(.debug_info) }
	.debug_line 0 : { *(.debug_line) }
	.debug_loc 0 : { *(.debug_loc) }
	.debug_ranges 0 : { *(.debug_ranges) }
	.debug_str 0 : { *(.debug_str) }
	.debug_macinfo 0 : { *(.debug_macinfo) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	.debug_pubtypes 0 : { *(.debug_pubtypes) }
	.symtab 0 : { *(.symtab) }
	.symtab_shndx 0 : { *(.symtab_shndx) }
	.shstrtab 0 : { *(.shstrtab) }
	.strtab 0 : { *(.strtab) }
	/* .stack_sizes (INFO)  makes the section not allocatable so it won't be loaded into memory */
	.stack_sizes 0 :
	{
		KEEP(*(.stack_sizes));
	}
	/DISCARD/ :
	{
	/* Discard RISC-V relevant .eh_frame, we are not doing unwind on panic
		so it is not needed. */
	*(.eh_frame);
	/* Comments and unused attributes */
	*(.comment*)
	*(.ARM.attributes*)
	*(.riscv.attributes*)
	*(.llvm_addrsig*)
	*(.note.GNU-stack*)
	*(.note.gnu.build-id)
	}
}
