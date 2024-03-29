#ifndef __UT_BITSTREAM_H__
#define __UT_BITSTREAM_H__

#include "defs.h"
#include "utils.h"
#include <stdint.h>
#include <limits.h>
#include <stddef.h>


typedef struct BitstreamContext {
    uint32_t bits;       // stores bits read from the buffer
    const uint8_t *ptr;  // pointer to the position inside a buffer
    uint8_t bits_valid; // number of bits left in bits field
    const uint8_t *buffer;
    uint32_t size_in_bits;
    const uint8_t *buffer_end;
} BitstreamContext;

typedef BitstreamContext GetBitContext;


static av_always_inline void bits_refill_32(BitstreamContext * restrict bc) {
    bc->bits |= (av_bswap32(READ_U32(bc->ptr))) << bc->bits_valid;
    bc->ptr += 4;
    bc->bits_valid += 32;
}

static av_always_inline void bits_refill_16(BitstreamContext * restrict bc) {
    bc->bits |= ((uint32_t)av_bswap16(READ_U16(bc->ptr))) << (16 - bc->bits_valid);
    bc->ptr += 2;
    bc->bits_valid += 16;
}

static av_pure_expr uint32_t bits_get_left(const BitstreamContext * restrict bc) {
    return ((uint32_t)(bc->buffer_end - bc->ptr) << 3) + bc->bits_valid;
}

static av_pure_expr uint32_t bits_get_32(const BitstreamContext *bc, uint8_t n) {
    return bc->bits >> (32 - n);
}

static av_pure_expr uint16_t bits_get_16(const BitstreamContext *bc, uint8_t n) {
    return bc->bits >> (32 - n);
}

static av_always_inline uint16_t bits_peek16(BitstreamContext * restrict bc, const uint8_t n) {
    if (bc->bits_valid > n)
        return bits_get_16(bc, n);
    
    bits_refill_16(bc);
    return bits_get_16(bc, n);
}

static av_always_inline void bits_skip(BitstreamContext * restrict bc, const uint8_t n) {
    bc->bits_valid -= n;
    bc->bits <<= n;
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
    bc->bits       = 0;

    bits_refill_32(bc);

    return 0;
}

#endif // __UT_BITSTREAM_H__
