#include "compile_time_macros.h"

/*
 * Preserved ram buffers exist in uninitialized ram. The buffers are semi persistent across
 * reboots and sysjumps. A regular buffer and ring buffer version is provided. The word size
 * can be any scalar type.
 *
 * Preserved ram buffers are designed to be especially fast for writes. This makes the buffers
 * well suited for tracing and logging use cases. Locking must be provided at a higher level.
 *
 * A rolling checksum ensures the buffer is uncorrupted. Corruption may be caused
 * by power spikes, misalignment with RO, or conflicting writes. Verification is slow, so 
 * it should only be performed before dumping the buffer.
 *
 * For performance reasons, the ring buffer does not use a tail pointer and does not support
 * dequeuing. The buffer is intended to be dumped as an entire chunk.
 */

/* Regular Linear Buffer */

typedef struct {
    uint32_t checksum;
    uint32_t buffer_size;
    uint32_t word_size;
    uint32_t version;
} preserved_ram_buf_noinit_t;

typedef struct {
    const uint32_t buffer_size;
    const uint32_t version;
    const uint32_t word_size;
} preserved_ram_buf_const_t;

#define DEFINE_PRESERVED_RAM_BUF_TYPE(word_type) \
typedef struct { \
    volatile word_type *buffer; \
    volatile preserved_ram_buf_noinit_t *noinit; \
    const preserved_ram_buf_const_t constants; \
} preserved_ram_buf_##word_type

#define DEFINE_PRESERVED_RAM_BUF_READ(word_type) \
static inline word_type preserved_ram_buf_read_##word_type(const preserved_ram_buf_##word_type *buf, uint32_t offset) \
{ \
    if (offset >= buf->constants.buffer_size) \
        return 0; \
    return buf->buffer[offset]; \
}

#define DEFINE_PRESERVED_RAM_BUF_WRITE(word_type) \
static inline void preserved_ram_buf_write_##word_type(const preserved_ram_buf_##word_type *buf, word_type word, uint32_t offset) \
{ \
    if (offset >= buf->constants.buffer_size) \
        return; \
    buf->noinit->checksum += word - buf->buffer[offset]; \
    buf->buffer[offset] = word; \
}

#define DEFINE_PRESERVED_RAM_BUF_CALC_CHECKSUM(word_type) \
static inline uint32_t preserved_ram_buf_calc_checksum_##word_type(const preserved_ram_buf_##word_type *buf) \
{ \
    uint32_t sum = 0; \
    for(int i=0; i < buf->constants.buffer_size; i++) \
        sum += buf->buffer[i]; \
    return sum; \
}

#define DEFINE_PRESERVED_RAM_BUF_VERIFY(word_type) \
static inline bool preserved_ram_buf_verify_##word_type(preserved_ram_buf_##word_type *buf) \
{ \
    return (buf->noinit->version == buf->constants.version && \
            buf->noinit->buffer_size == buf->constants.buffer_size && \
            buf->noinit->word_size == buf->constants.word_size && \
            buf->noinit->checksum == preserved_ram_buf_calc_checksum_##word_type(buf)); \
}

#define DEFINE_PRESERVED_RAM_BUF_RESET(word_type) \
static inline void preserved_ram_buf_reset_##word_type(preserved_ram_buf_##word_type *buf) \
{ \
    buf->noinit->checksum = 0; \
    buf->noinit->version = buf->constants.version; \
    buf->noinit->buffer_size = buf->constants.buffer_size; \
    buf->noinit->word_size = buf->constants.word_size; \
    memset((void *)buf->buffer, 0, buf->constants.buffer_size * buf->constants.word_size); \
}

#define DEFINE_PRESERVED_RAM_BUF(word_type) \
    BUILD_ASSERT((sizeof(word_type) & (sizeof(word_type) - 1)) == 0); \
    DEFINE_PRESERVED_RAM_BUF_TYPE(word_type); \
    DEFINE_PRESERVED_RAM_BUF_READ(word_type); \
    DEFINE_PRESERVED_RAM_BUF_WRITE(word_type); \
    DEFINE_PRESERVED_RAM_BUF_CALC_CHECKSUM(word_type); \
    DEFINE_PRESERVED_RAM_BUF_VERIFY(word_type); \
    DEFINE_PRESERVED_RAM_BUF_RESET(word_type)


#define DECLARE_PRESERVED_RAM_BUF(name, word_type, _buffer_size, _version) \
    BUILD_ASSERT((_buffer_size & (_buffer_size - 1)) == 0); \
    static volatile word_type __preserved_ram_buf_##name##_buffer[_buffer_size] __uncached __aligned(4) __noinit_ram(_preserved_ram_buf_##name##_buffer); \
    static volatile preserved_ram_buf_noinit_t __preserved_ram_buf_##name##_noinit __uncached __aligned(4) __noinit_ram(_preserved_ram_buf_##name##_noinit); \
    static preserved_ram_buf_##word_type __preserved_ram_buf_##name##_data = { \
        .buffer = __preserved_ram_buf_##name##_buffer, \
        .noinit = &__preserved_ram_buf_##name##_noinit, \
        .constants = { \
            .buffer_size = _buffer_size, \
            .version = _version, \
            .word_size = sizeof(word_type), \
        }, \
    }; \
    static preserved_ram_buf_##word_type *name = &__preserved_ram_buf_##name##_data


/* Ring Buffer */

typedef struct {
    uint32_t checksum;
    uint32_t head;
    uint32_t buffer_size;
    uint32_t word_size;
    uint32_t version;
} preserved_ram_ring_buf_noinit_t;

#define DEFINE_PRESERVED_RAM_RING_BUF_TYPE(word_type) \
typedef struct { \
    volatile word_type *buffer; \
    volatile preserved_ram_ring_buf_noinit_t *noinit; \
    const preserved_ram_buf_const_t constants; \
} preserved_ram_ring_buf_##word_type

/*
 * Read at an offset from the tail of the ring buffer.
 * Before the buffer is full, the tail is 0.
 * When the buffer is full, the tail is at head.
 */
#define DEFINE_PRESERVED_RAM_RING_BUF_READ(word_type) \
static inline word_type preserved_ram_ring_buf_read_##word_type(const preserved_ram_ring_buf_##word_type *buf, uint32_t offset) \
{ \
    uint32_t tail = 0; \
    if (buf->noinit->head > buf->constants.buffer_size) \
        tail = buf->noinit->head; \
    return buf->buffer[(tail + offset) % buf->constants.buffer_size]; \
}

#define DEFINE_PRESERVED_RAM_RING_BUF_WRITE(word_type) \
static inline void preserved_ram_ring_buf_write_##word_type(const preserved_ram_ring_buf_##word_type *buf, word_type word) \
{ \
    uint32_t write_index = buf->noinit->head++ % buf->constants.buffer_size; \
    buf->noinit->checksum += word - buf->buffer[write_index]; \
    buf->buffer[write_index] = word; \
}

/*
 * Before the buffer is filled, the size is head.
 * After the buffer is filled, the size is always the buffer_size.
 */
#define DEFINE_PRESERVED_RAM_RING_BUF_LEN(word_type) \
static inline uint32_t preserved_ram_ring_buf_len_##word_type(const preserved_ram_ring_buf_##word_type *buf) \
{ \
    if (buf->noinit->head < buf->constants.buffer_size) \
        return buf->noinit->head; \
    return buf->constants.buffer_size; \
}

#define DEFINE_PRESERVED_RAM_RING_BUF_CALC_CHECKSUM(word_type) \
static inline uint32_t preserved_ram_ring_buf_calc_checksum_##word_type(const preserved_ram_ring_buf_##word_type *buf) \
{ \
    uint32_t sum = 0; \
    for(int i=0; i < preserved_ram_ring_buf_len_##word_type(buf); i++) \
        sum += buf->buffer[i]; \
    return sum; \
}

#define DEFINE_PRESERVED_RAM_RING_BUF_VERIFY(word_type) \
static inline bool preserved_ram_ring_buf_verify_##word_type(preserved_ram_ring_buf_##word_type *buf) \
{ \
    return (buf->noinit->version == buf->constants.version && \
            buf->noinit->buffer_size == buf->constants.buffer_size && \
            buf->noinit->word_size == buf->constants.word_size && \
            buf->noinit->checksum == preserved_ram_ring_buf_calc_checksum_##word_type(buf)); \
}

#define DEFINE_PRESERVED_RAM_RING_BUF_RESET(word_type) \
static inline void preserved_ram_ring_buf_reset_##word_type(preserved_ram_ring_buf_##word_type *buf) \
{ \
    buf->noinit->checksum = 0; \
    buf->noinit->head = 0; \
    buf->noinit->version = buf->constants.version; \
    buf->noinit->buffer_size = buf->constants.buffer_size; \
    buf->noinit->word_size = buf->constants.word_size; \
    memset((void *)buf->buffer, 0, buf->constants.buffer_size * buf->constants.word_size); \
}

#define DEFINE_PRESERVED_RAM_RING_BUF(word_type) \
    BUILD_ASSERT((sizeof(word_type) & (sizeof(word_type) - 1)) == 0); \
    DEFINE_PRESERVED_RAM_RING_BUF_TYPE(word_type); \
    DEFINE_PRESERVED_RAM_RING_BUF_READ(word_type); \
    DEFINE_PRESERVED_RAM_RING_BUF_WRITE(word_type); \
    DEFINE_PRESERVED_RAM_RING_BUF_LEN(word_type); \
    DEFINE_PRESERVED_RAM_RING_BUF_CALC_CHECKSUM(word_type); \
    DEFINE_PRESERVED_RAM_RING_BUF_VERIFY(word_type); \
    DEFINE_PRESERVED_RAM_RING_BUF_RESET(word_type)


#define DECLARE_PRESERVED_RAM_RING_BUF(name, word_type, _buffer_size, _version) \
    static volatile word_type __preserved_ram_ring_buf_##name##_buffer[_buffer_size] __uncached __aligned(4) __noinit_ram(_preserved_ram_ring_buf_##name##_buffer); \
    static volatile preserved_ram_ring_buf_noinit_t __preserved_ram_ring_buf_##name##_noinit __uncached __aligned(4) __noinit_ram(_preserved_ram_ring_buf_##name##_noinit); \
    static preserved_ram_ring_buf_##word_type __preserved_ram_ring_buf_##name##_data = { \
        .buffer = __preserved_ram_ring_buf_##name##_buffer, \
        .noinit = &__preserved_ram_ring_buf_##name##_noinit, \
        .constants = { \
            .buffer_size = _buffer_size, \
            .version = _version, \
            .word_size = sizeof(word_type), \
        }, \
    }; \
    static preserved_ram_ring_buf_##word_type *name = &__preserved_ram_ring_buf_##name##_data
