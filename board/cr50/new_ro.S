
	.section .text
	.global new_b1_ro_start, new_b1_ro_end
	.global new_b2_ro_start, new_b2_ro_end
new_b1_ro_start:
	.incbin "board/cr50/b1.ro_b.bin"
new_b1_ro_end:
new_b2_ro_start:
	.incbin "board/cr50/b2.ro_b.bin"
new_b2_ro_end:
