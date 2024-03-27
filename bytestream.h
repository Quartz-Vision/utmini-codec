#ifndef __UT_BYTESTREAM_H__
#define __UT_BYTESTREAM_H__

#include "defs.h"
#include "utils.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct GetByteContext {
    const uint8_t *buffer, *buffer_end, *buffer_start;
} GetByteContext;

typedef struct PutByteContext {
    uint8_t *buffer_end, *buffer, *buffer_start;
    bool eof;
} PutByteContext;

static av_always_inline void bytestream_init(
    GetByteContext *g, const uint8_t *buf, uint32_t buf_size
) {
    av_assert0(buf_size >= 0);
    g->buffer       = buf;
    g->buffer_start = buf;
    g->buffer_end   = buf + buf_size;
}

static av_pure_expr int bytestream_get_bytes_left(GetByteContext *g) {
    return g->buffer_end - g->buffer;
}

static av_always_inline void bytestream_skipu(GetByteContext *g, uint32_t size) {
    g->buffer += size;
}


#define DEF(type, name, bytes, read, write)                                    \
static av_always_inline void bytestream_put_ ## name ## u(PutByteContext *p,   \
                                                           const type value)   \
{                                                                              \
    write(p->buffer, value);                                                   \
    p->buffer += bytes;                                                        \
}                                                                              \
static av_always_inline void bytestream_put_ ## name(PutByteContext *p,        \
                                                      const type value)        \
{                                                                              \
    if (!p->eof && (p->buffer_end - p->buffer >= bytes)) {                     \
        write(p->buffer, value);                                               \
        p->buffer += bytes;                                                    \
    } else                                                                     \
        p->eof = 1;                                                            \
}                                                                              \
static av_always_inline type bytestream_get_ ## name ## u(GetByteContext *g)   \
{                                                                              \
    g->buffer += bytes;                                                        \
    return read(g->buffer - bytes);                                            \
}                                                                              \
static av_always_inline type bytestream_get_ ## name(GetByteContext *g)        \
{                                                                              \
    if (g->buffer_end - g->buffer < bytes) {                                   \
        g->buffer = g->buffer_end;                                             \
        return 0;                                                              \
    }                                                                          \
    return bytestream_get_ ## name ## u(g);                                    \
}                                                                              \
static av_pure_expr type bytestream_peek_ ## name ## u(GetByteContext *g)      \
{                                                                              \
    return read(g->buffer);                                                    \
}                                                                              \
static av_pure_expr type bytestream_peek_ ## name(GetByteContext *g)           \
{                                                                              \
    if (g->buffer_end - g->buffer < bytes)                                     \
        return 0;                                                              \
    return bytestream_peek_ ## name ## u(g);                                   \
}

DEF(uint32_t, le32, 4, READ_U32, WRITE_U32)
DEF(uint8_t, byte, 1, READ_U8, WRITE_U8)

#endif // __UT_BYTESTREAM_H__
