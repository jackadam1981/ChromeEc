.global queue_add_unit; .type queue_add_unit, %function

.global queue_space; .type queue_space, %function
.global queue_advance_tail; .type queue_advance_tail, %function

  # This nop should misalign the jal to queue_space and cause the keyboard to
  # break, and it does appear to.
  nop
queue_add_unit:
	//size_t tail = q->state->tail & q->buffer_units_mask;
	lw	a5,0(a0)	// state pointer
	# Allocate stack frame, store callee-saved regs
	addi	sp,sp,-32
	sw	s0,24(sp)
	sw	s1,20(sp)
	sw	s2,16(sp)
	sw	ra,28(sp)
	//size_t tail = q->state->tail & q->buffer_units_mask;
	lw	s1,4(a5)	// tail
	lw	s2,12(a0)	// buffer_units_mask
	// {
	mv	s0,a0
	sw	a1,12(sp)
	//if (queue_space(q) == 0)
	jal	queue_space
	//return 0;
	beqz	a0,_queue_no_space
	//__asm__ volatile (" nop");
	//if (q->unit_bytes == 1)
	lw	a2,16(s0)
	li	a4,1
	//q->buffer[tail] = *((uint8_t *) src);
	lw	a0,20(s0)
	//if (q->unit_bytes == 1)
	lw	a1,12(sp)
	//size_t tail = q->state->tail & q->buffer_units_mask;
	and	a5,s1,s2
	//if (q->unit_bytes == 1)
	bne	a2,a4,_queue_add_memcpy
	//	q->buffer[tail] = *((uint8_t *) src);
	lbu	a4,0(a1)
	add	a0,a0,a5
	sb	a4,0(a0)
	//else
	//memcpy(q->buffer + tail * q->unit_bytes, src, q->unit_bytes);
	//return queue_advance_tail(q, 1);
_queue_add_advance:
	mv	a0,s0
	//}
	lw	s0,24(sp)
	lw	ra,28(sp)
	lw	s1,20(sp)
	lw	s2,16(sp)
	//return queue_advance_tail(q, 1);
	li	a1,1
	//}
	addi	sp,sp,32
	//return queue_advance_tail(q, 1);
	j	queue_advance_tail
	//	memcpy(q->buffer + tail * q->unit_bytes, src, q->unit_bytes);
_queue_add_memcpy:
	mul	a5,a2,a5
	add	a0,a0,a5
	jal	ra,memcpy
	j	_queue_add_advance
	//}
_queue_no_space:
	lw	ra,28(sp)
	lw	s0,24(sp)
	lw	s1,20(sp)
	lw	s2,16(sp)
	addi	sp,sp,32
	ret
