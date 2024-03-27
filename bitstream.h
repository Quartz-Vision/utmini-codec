#ifndef __UT_BITSTREAM_H__
#define __UT_BITSTREAM_H__

#include "utils.h"
#include <stdint.h>
#include <limits.h>
#include <stddef.h>


typedef struct BitstreamContext {
    U64 bits;       // stores bits read from the buffer
    const uint8_t *ptr;  // pointer to the position inside a buffer
    uint8_t bits_valid; // number of bits left in bits field
    const uint8_t *buffer;
    uint32_t size_in_bits;
    const uint8_t *buffer_end;
} BitstreamContext;

typedef BitstreamContext GetBitContext;


static inline void bits_refill_64(BitstreamContext * restrict bc) {
    bc->bits.v = av_bswap64(READ_U64(bc->ptr));
    bc->ptr += 8;
    bc->bits_valid = 64;
}

static inline void bits_refill_32(BitstreamContext * restrict bc) {
    bc->bits.v |= ((uint64_t)av_bswap32(READ_U32(bc->ptr))) << (32 - bc->bits_valid);
    bc->ptr += 4;
    bc->bits_valid += 32;
}

static av_pure_expr uint32_t bits_get_left(const BitstreamContext * restrict bc) {
    return (bc->buffer_end - bc->ptr) * 8 + bc->bits_valid;
}

static av_pure_expr uint32_t bits_get_32(const BitstreamContext *bc, uint8_t n) {
    return bc->bits.p.h >> (32 - n);
}

// Read and possibly refill 0-32 bits.
static av_always_inline uint32_t bits_peek(BitstreamContext * restrict bc, const uint8_t n) {
    if (bc->bits_valid <= n)
        bits_refill_32(bc);
    
    return bits_get_32(bc, n);
}

static av_always_inline void bits_skip(BitstreamContext * restrict bc, const uint8_t n) {
    bc->bits_valid -= n;
    bc->bits.v <<= n;
}

static inline int bits_init(
    BitstreamContext *bc,
    const uint8_t *buffer,
    uint32_t bit_size
) {
    if (bit_size > INT_MAX - 7 || !buffer) {
        bc->buffer     = NULL;
        bc->ptr        = NULL;
        bc->bits_valid = 0;
        return AVERROR_INVALIDDATA;
    }

    bc->buffer       = buffer;
    bc->buffer_end   = buffer + ((bit_size + 7) >> 3);
    bc->ptr          = buffer;
    bc->size_in_bits = bit_size;
    bc->bits_valid   = 0;
    bc->bits.v       = 0;

    bits_refill_64(bc);

    return 0;
}

#endif // __UT_BITSTREAM_H__
