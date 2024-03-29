#include "defs.h"
#include "video.h"
#include <stddef.h>
#include <stdint.h>
#include <memory.h>
#include "utils.h"
#include "bitstream.h"
#include "bytestream.h"
#include "vlc.h"


static void restore_rgb_planes(
    uint8_t *r,
    uint8_t *g,
    uint8_t *b,
    ptrdiff_t linesize,
    int width, int height,
    uint32_t *out
) {
    int i, j;
    int rest = linesize - width;
    uint64_t r0, g0, b0;

    for (j = 0; j < height; j++) {
        for (i = 0; i < width; i += 8) {
            r0 = READ_U64(r);
            g0 = READ_U64(g);
            b0 = READ_U64(b);

            b0 = b0 + g0 - 0x8080808080808080ull;
            r0 = r0 + g0 - 0x8080808080808080ull;
            
#define U64_READ_U8(v, i) (uint8_t)(((uint64_t)v & (0xFFull << i)) >> i)

#define NEW_OUT(x) (0xFF000000 | (             \
    (uint32_t)                                 \
    (U64_READ_U8(b0, x) << 16)                 \
    | (U64_READ_U8(g0, x) << 8)                \
    | (U64_READ_U8(r0, x) & 0x000000FF)        \
))                                             \

            *(out++) = NEW_OUT(0);
            *(out++) = NEW_OUT(8);
            *(out++) = NEW_OUT(16);
            *(out++) = NEW_OUT(24);
            *(out++) = NEW_OUT(32);
            *(out++) = NEW_OUT(40);
            *(out++) = NEW_OUT(48);
            *(out++) = NEW_OUT(56);

#undef NEW_OUT
#undef U64_READ_U8

            r+=8;
            g+=8;
            b+=8;
        }
        r += rest;
        g += rest;
        b += rest;
        out += rest;
    }
}


static int add_left_pred(
    uint8_t *dst, const uint8_t *src, ptrdiff_t w, int acc
) {
    int i;

    if (w >= 8) {
        for (i = 0; i < w - 7; i += 8) {
            acc   += src[i];
            dst[i] = acc;
            acc   += src[i + 1];
            dst[i + 1] = acc;
            acc   += src[i + 2];
            dst[i + 2] = acc;
            acc   += src[i + 3];
            dst[i + 3] = acc;
            acc   += src[i + 4];
            dst[i + 4] = acc;
            acc   += src[i + 5];
            dst[i + 5] = acc;
            acc   += src[i + 6];
            dst[i + 6] = acc;
            acc   += src[i + 7];
            dst[i + 7] = acc;
        }
    } else {
        for (i = 0; i < w - 1; i++) {
            acc   += src[i];
            dst[i] = acc;
            i++;
            acc   += src[i];
            dst[i] = acc;
        }
    }

    for (; i < w; i++) {
        acc   += src[i];
        dst[i] = acc;
    }

    return acc;
}


typedef struct HuffEntry {
    uint8_t len;
    uint16_t sym;
} HuffEntry;

int build_huff(VideoContext *ctx, const uint8_t *src, VLC *vlc,
                      VLC_MULTI *multi, int *fsym)
{
    int i;
    uint8_t v;
    HuffEntry he[1024];
    uint8_t bits[1024];
    uint16_t codes_count[33] = { 0 };

    *fsym = -1;
    for (i = 0; i < UT_HUFF_ELEMS; i++) {
        v = src[i];
        
        switch (v) {
            case 0:
                *fsym = i;
                return 0;
            case 255:
                bits[i] = 0;
                break;
            case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23: case 24: case 25: case 26: case 27: case 28: case 29: case 30: case 31: case 32:
                bits[i] = v;
                break;
            default:
                return AVERROR_INVALIDDATA;
        }

        codes_count[bits[i]]++;
    }
    if (codes_count[0] == UT_HUFF_ELEMS)
        return AVERROR_INVALIDDATA;

    /* For Ut Video, longer codes are to the left of the tree and
     * for codes with the same length the symbol is descending from
     * left to right. So after the next loop --codes_count[i] will
     * be the index of the first (lowest) symbol of length i when
     * indexed by the position in the tree with left nodes being first. */
    for (int i = 31; i >= 0; i--) 
        codes_count[i] += codes_count[i + 1];

    for (unsigned i = 0; i < UT_HUFF_ELEMS; i++)
        he[--codes_count[bits[i]]] = (HuffEntry) { bits[i], i };

    // The last arg is the log context, f it for now
    int a = vlc_init_multi_from_lengths(
        vlc, multi, codes_count[0],
        &he[0].len, sizeof(*he),
        &he[0].sym, sizeof(*he)
    );

    log_info("A=%d\n", a);
    return a;
}


#define PLANE_END_PAD 5

static int decode_plane(
    VideoContext *ctx, int plane_no,
    uint8_t *dst, ptrdiff_t stride,
    int width, int height,
    const uint8_t *src
) {
    int i, j, slice, pix;
    int sstart, send;
    VLC_MULTI multi;
    VLC vlc;
    GetBitContext gb;
    int ret, prev, fsym;

    if (build_huff(ctx, src, &vlc, &multi, &fsym)) {
        return AVERROR_INVALIDDATA;
    }
    
    if (fsym >= 0) { // build_huff reported a symbol to fill slices with
        send = 0;
        for (slice = 0; slice < ctx->slices; slice++) {
            uint8_t *dest;

            sstart = send;
            send   = (height * (slice + 1) / ctx->slices);
            dest   = dst + sstart * stride;

            prev = 0x80;
            for (j = sstart; j < send; j++) {
                for (i = 0; i < width; i++) {
                    pix = fsym;
                    prev += (unsigned)pix;
                    pix   = prev;
                    dest[i] = pix;
                }
                dest += stride;
            }
        }
        return 0;
    }

    src += 256;

    send = 0;
    for (slice = 0; slice < ctx->slices; slice++) {
        uint8_t *dest, *buf;
        int32_t slice_data_start, slice_data_end, slice_size;

        sstart = send;
        send   = (height * (slice + 1) / ctx->slices);
        dest   = dst + sstart * stride;

        // slice offset and size validation was done earlier
        slice_data_start = slice ? READ_U32(src + slice * 4 - 4) : 0;
        slice_data_end   = READ_U32(src + slice * 4);
        slice_size       = slice_data_end - slice_data_start;

        if (!slice_size) {
            goto fail;
        }

        // ???
        // The VLC is in Big Endian, so we need to reverse the byte order.
        // So they code it like:
        // The comand is 0x1234, so we have [0x34, 0x12], but we wanna go BE, so we have [0x12, 0x34]
        // Then we have to decode it, that's why we:
        //
        // Add padding 0-bytes to the end of the slice buffer.
        // Put the slice data into 32-bit integers.
        // Reverse the byte order of the integers, so the bits are in the same order as in the memory.
        // Initialize the bitstream reader.
        // Ex:
        // [0x0A, 0x0B, 0x0C, 0x0D, | 0x0E, 0x0F, 0x10, 0x11, | 0x01, 0x02, 0x03, 0x04]
        // ->
        // [0x0D0C0B0A, | 0x11100F0E, | 0x04030201]
        //
        // Then we read the bits as 64-bit integers, thus reversing the int32 order.
        // Ex:
        // ->
        // [0x11'10'0F'0E|0D'0C'0B'0A, | 0x04'03'02'01|00'00'00'00]
        //
        // After the int64 read, we can read the bits and we read them from the end.
        // Ex:
        // Read 11 bits from 0x11'10'0F'0E|0D'0C'0B'0A and we get 0x04'0B'0A
        
        bswap_buf(
            (uint32_t *) ctx->slice_buf,
            (uint32_t *)(src + slice_data_start + ctx->slices * 4),
            (slice_data_end - slice_data_start + 3) >> 2
        );
        bits_init(&gb, ctx->slice_buf, slice_size << 3);

        prev = 0x80;
        for (j = sstart; j < send; j++) {
            buf = ctx->vlc_buf;
            i = 0;
            while(i < (width - PLANE_END_PAD)) {
                ret = vlc_read_multi(
                    &gb,
                    buf + i,
                    multi.table,
                    vlc.table
                );

                i += ret;
                
                if (ret <= 0)
                    goto fail;
            }
            for (; i < width; i++)
                buf[i] = vlc_read(&gb, vlc.table);
            
            // ???
            add_left_pred(dest, buf, width, prev);
            prev = dest[width-1];
            dest += stride;
        }
    }

    vlc_free(&vlc);
    vlc_free_multi(&multi);
    return 0;
fail:
    vlc_free(&vlc);
    vlc_free_multi(&multi);
    return AVERROR_INVALIDDATA;
}

#undef A
#undef B
#undef C


static int decode_frame(VideoContext * ctx, int *got_frame)
{
    const uint8_t *buf = ctx->packet_data;
    int buf_size = ctx->packet_size;
    int i, j;
    const uint8_t *plane_start[5] = { 0 };
    int plane_size, max_slice_size = 0, slice_start, slice_end, slice_size;
    int ret;
    GetByteContext gb;

    /* parse plane structure to get frame flags and validate slice offsets */
    bytestream_init(&gb, buf, buf_size);

    for (i = 0; i < UT_COLOR_PLANES; i++) {
        plane_start[i] = gb.buffer;
        if (bytestream_get_bytes_left(&gb) < 256 + 4 * ctx->slices) {
            log_info("Insufficient data for a plane\n");
            return AVERROR_INVALIDDATA;
        }
        bytestream_skipu(&gb, 256);
        slice_start = 0;
        slice_end   = 0;
        for (j = 0; j < ctx->slices; j++) {
            slice_end   = bytestream_get_le32u(&gb);
            if (slice_end < 0 || slice_end < slice_start ||
                bytestream_get_bytes_left(&gb) < slice_end) {
                log_info("Incorrect slice size\n");
                return AVERROR_INVALIDDATA;
            }
            slice_size  = slice_end - slice_start;
            slice_start = slice_end;
            max_slice_size = MAX(max_slice_size, slice_size);
        }
        plane_size = slice_end;
        bytestream_skipu(&gb, plane_size);
    }
    plane_start[UT_COLOR_PLANES] = gb.buffer;
    
    for (i = 0; i < UT_COLOR_PLANES; i++) {
        ret = decode_plane(
            ctx, i, ctx->frame_data[i], ctx->linesize, ctx->w, ctx->h, plane_start[i]
        );
        if (ret)
            return ret;
    }
    // ???
    restore_rgb_planes(
        ctx->frame_data[2], ctx->frame_data[0], ctx->frame_data[1],
        ctx->linesize,
        ctx->w, ctx->h,
        ctx->result_frame_data
    );

    *got_frame = 1;

    /* always report that the buffer was completely consumed */
    log_info("OK\n");
    return buf_size;
}

