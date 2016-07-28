
	.section .text
	.global new_ro_a_start, new_ro_a_end
	.global new_ro_b_start, new_ro_b_end
new_ro_a_start:
	.incbin "board/cr50/ro_a.bin"
new_ro_a_end:
new_ro_b_start:
	.incbin "board/cr50/ro_b.bin"
new_ro_b_end:
