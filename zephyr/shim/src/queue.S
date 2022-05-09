.global queue_remove_unit; .type queue_remove_unit, %function

.global queue_count.isra.0; .type queue_count.isra.0, %function
.global queue_advance_head; .type queue_advance_head, %function

  # This nop should misalign the jal to queue_space and cause the keyboard to
  # break, and it does appear to.

#if 0
queue_remove_unit:
	//size_t head = q->state->head & q->buffer_units_mask;
	lw	a5,0(a0)
	addi	sp,sp,-32
	sw	s0,24(sp)
	sw	s1,20(sp)
	sw	s2,16(sp)
	sw	s3,12(sp)
	sw	ra,28(sp)
	//size_t head = q->state->head & q->buffer_units_mask;
	lw	s1,0(a5)
	lw	s3,12(a0)
	mv	s0,a0
	mv	s2,a1
	nop
	//if (queue_count(q) == 0)
	lw	a0,0(a0)
	jal	ra,queue_count.isra.0
	beqz	a0,queue_remove_unit_1 //800036fc <queue_remove_unit+0x5a>
	//	return 0;
	//if (q->unit_bytes == 1)
	lw	a2,16(s0)
	li	a4,1
	//*((uint8_t *) dest) = q->buffer[head];
	lw	a1,20(s0)
	//size_t head = q->state->head & q->buffer_units_mask;
	and	a5,s1,s3
	//if (q->unit_bytes == 1)
	bne	a2,a4,queue_remove_unit_2 //800036ee <queue_remove_unit+0x4c>
	//*((uint8_t *) dest) = q->buffer[head];
	add	a1,a1,a5
	lbu	a5,0(a1)
	sb	a5,0(s2)
	//else {
	//	//__asm__ volatile (" nop");
	//	memcpy(dest, q->buffer + head * q->unit_bytes, q->unit_bytes);
	//}

	//return queue_advance_head(q, 1);
queue_remove_unit_3:
	mv	a0,s0
//}
	lw	s0,24(sp)
	lw	ra,28(sp)
	lw	s1,20(sp)
	lw	s2,16(sp)
	lw	s3,12(sp)
	//return queue_advance_head(q, 1);
	li	a1,1
//}
	addi	sp,sp,32
	//return queue_advance_head(q, 1);
	j	queue_advance_head
	//	memcpy(dest, q->buffer + head * q->unit_bytes, q->unit_bytes);
queue_remove_unit_2:
	mul	a5,a2,a5
	mv	a0,s2
	add	a1,a1,a5
	jal	ra,memcpy
	j	queue_remove_unit_3 //800036dc <queue_remove_unit+0x3a>
queue_remove_unit_1:
	lw	ra,28(sp)
	lw	s0,24(sp)
	lw	s1,20(sp)
	lw	s2,16(sp)
	lw	s3,12(sp)
	addi	sp,sp,32
	ret
#endif