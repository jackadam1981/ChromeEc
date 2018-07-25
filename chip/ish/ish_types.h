#ifndef __ISH_TYPES_H
#define __ISH_TYPES_H

#include <stdint.h>
/*#include <attributes.h>*/

/* ISH physical address */
typedef uint32_t paddr_t;

/* ISH virtual address */
typedef uint32_t vaddr_t;

/* DRAM address */
typedef uint32_t dram_addr_t;

typedef struct {
	uint16_t idt_size;
	uint32_t idt_start;
} __attribute__((packed)) idt_ptr_t;


/* memory resource descriptor */
typedef struct _mrd_t
{
	struct _mrd_t  *next;  /* Pointer to next data buffer */
	void const*    buffer;
	uint32_t       length; /* Number of bytes in the region */
} mrd_t;

#endif /* __ISH_TYPES_H */