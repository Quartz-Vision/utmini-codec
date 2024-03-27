#ifndef __UT_VLC_H__
#define __UT_VLC_H__

#include <stdint.h>
#include <stdlib.h>
#include <memory.h>

#include "utils.h"
#include "bitstream.h"
#include "mem.h"


#define VLC_MULTI_MAX_SYMBOLS 6

// When changing this, be sure to also update tableprint_vlc.h accordingly.
typedef int16_t VLCBaseType;

typedef struct VLCElem {
    VLCBaseType sym, len;
} VLCElem;

typedef struct VLC {
    VLCElem *table;
    int table_size, table_allocated;
} VLC;

typedef struct VLC_MULTI_ELEM {
    uint8_t val[VLC_MULTI_MAX_SYMBOLS];
    int8_t len; // -31,32
    uint8_t num;
} VLC_MULTI_ELEM;

typedef struct VLC_MULTI {
    VLC_MULTI_ELEM *table;
    int table_size, table_allocated;
} VLC_MULTI;

typedef struct RL_VLC_ELEM {
    int16_t level;
    int8_t len;
    uint8_t run;
} RL_VLC_ELEM;

typedef struct VLCcode {
    uint8_t bits;
    VLCBaseType symbol;
    /** codeword, with the first bit-to-be-read in the msb
     * (even if intended for a little-endian bitstream reader) */
    uint32_t code;
} VLCcode;


#define LOCALBUF_ELEMS 1500 // the maximum currently needed is 1296 by rv34
#define VLC_INIT_USE_STATIC     1
#define VLC_INIT_STATIC_OVERLONG (2 | VLC_INIT_USE_STATIC)
/* If VLC_INIT_INPUT_LE is set, the LSB bit of the codes used to
 * initialize the VLC table is the first bit to be read. */
#define VLC_INIT_INPUT_LE       4
/* If set the VLC is intended for a little endian bitstream reader. */
#define VLC_INIT_OUTPUT_LE      8
#define VLC_INIT_LE             (VLC_INIT_INPUT_LE | VLC_INIT_OUTPUT_LE)


#define VLC_GET_DATA(v, table, i, wrap, size)               \
{                                                           \
    const uint8_t *ptr = (const uint8_t *)table + i * wrap; \
    switch(size) {                                          \
    case 1:                                                 \
        v = *(const uint8_t *)ptr;                          \
        break;                                              \
    case 2:                                                 \
        v = *(const uint16_t *)ptr;                         \
        break;                                              \
    case 4:                                                 \
    default:                                                \
        v = *(const uint32_t *)ptr;                         \
        break;                                              \
    }                                                       \
}


static inline int vlc_set_idx(
    BitstreamContext * restrict bc,
    const int code,
    int * restrict n,
    int * restrict nb_bits,
    const VLCElem * table
) {
    *nb_bits = -*n;
    const unsigned idx = bits_peek(bc, *nb_bits) + code;
    *n = table[idx].len;
    return table[idx].sym;
}


/**
 * Parse a vlc / vlc_multi code.
 * @param dst the parsed symbol(s) will be stored here. Up to 8 bytes are written
 * @returns number of symbols parsed
 */
static inline int vlc_read_multi(
    BitstreamContext *bc, uint8_t dst[8],
    const VLC_MULTI_ELEM *const Jtable,
    const VLCElem *const table
) {
    // Read BITS bits from the cache (refilling it if necessary)
    const unsigned idx = bits_peek(bc, UT_VLC_BITS);

    int ret, nb_bits, code, n = Jtable[idx].len;
    if (Jtable[idx].num) {
        COPY_U64(dst, Jtable[idx].val);
        ret = Jtable[idx].num;
    } else {
        code = table[idx].sym;
        n = table[idx].len;
        if (n < 0) {  // depth 2
            bits_skip(bc, UT_VLC_BITS);

            code = vlc_set_idx(bc, code, &n, &nb_bits, table);
            if (n < 0) {  // depth 3
                bits_skip(bc, nb_bits);
                code = vlc_set_idx(bc, code, &n, &nb_bits, table);
            }
        }
        WRITE_U16(dst, code);
        ret = n > 0;
    }
    bits_skip(bc, n);

    return ret;
}


static inline int vlc_read(
    BitstreamContext *bc, const VLCElem *table
) {
    int nb_bits;
    unsigned idx = bits_peek(bc, UT_VLC_BITS);
    int code     = table[idx].sym;
    int n        = table[idx].len;

    if (n < 0) {
        bits_skip(bc, UT_VLC_BITS);
        code = vlc_set_idx(bc, code, &n, &nb_bits, table);
        if (n < 0) {
            bits_skip(bc, nb_bits);
            code = vlc_set_idx(bc, code, &n, &nb_bits, table);
        }
    }
    bits_skip(bc, n);

    return code;
}

static int alloc_table(VLC *vlc, int size) {
    int index = vlc->table_size;

    vlc->table_size += size;
    if (vlc->table_size > vlc->table_allocated) {
        vlc->table_allocated += (1 << UT_VLC_BITS);
        vlc->table = av_realloc_f(vlc->table, vlc->table_allocated, sizeof(*vlc->table));
        if (!vlc->table) {
            return AVERROR(ENOMEM);
        }
        memset(vlc->table + vlc->table_allocated - (1 << UT_VLC_BITS), 0, sizeof(*vlc->table) << UT_VLC_BITS);
    }
    return index;
}

/**
 * Build VLC decoding tables suitable for use with get_vlc().
 *
 * @param vlc            the context to be initialized
 *
 * @param table_nb_bits  max length of vlc codes to store directly in this table
 *                       (Longer codes are delegated to subtables.)
 *
 * @param nb_codes       number of elements in codes[]
 *
 * @param codes          descriptions of the vlc codes
 *                       These must be ordered such that codes going into the same subtable are contiguous.
 *                       Sorting by VLCcode.code is sufficient, though not necessary.
 */
static int build_table(
    VLC *vlc, int table_nb_bits, int nb_codes, VLCcode *codes
) {
    int table_size, table_index;
    VLCElem *table;

    if (table_nb_bits > 30)
       return AVERROR(EINVAL);

    table_size = 1 << table_nb_bits;
    table_index = alloc_table(vlc, table_size);
    log_info("new table index=%d size=%d\n", table_index, table_size);
    if (table_index < 0)
        return table_index;
    table = &vlc->table[table_index];

    /* first pass: map codes and compute auxiliary table sizes */
    for (int i = 0; i < nb_codes; i++) {
        int         n = codes[i].bits;
        uint32_t code = codes[i].code;
        int    symbol = codes[i].symbol;
        log_info("i=%d n=%d code=0x%x\n", i, n, code);
        if (n <= table_nb_bits) {
            /* no need to add another table */
            int   j = code >> (32 - table_nb_bits);
            int  nb = 1 << (table_nb_bits - n);

            for (int k = 0; k < nb; k++) {
                int   bits = table[j].len;
                int oldsym = table[j].sym;
                log_info("%4x: code=%d n=%d\n", j, i, n);
                
                if ((bits || oldsym) && (bits != n || oldsym != symbol)) {
                    log_info("incorrect codes\n");
                    return AVERROR_INVALIDDATA;
                }

                table[j].len = n;
                table[j].sym = symbol;
                j++;
            }
        } else {
            /* fill auxiliary table recursively */
            uint32_t code_prefix;
            int index, subtable_bits, j, k;

            n -= table_nb_bits;
            code_prefix = code >> (32 - table_nb_bits);
            subtable_bits = n;
            codes[i].bits = n;
            codes[i].code = code << table_nb_bits;

            for (k = i + 1; k < nb_codes; k++) {
                n = codes[k].bits - table_nb_bits;
                code = codes[k].code;
                if (n <= 0 || code >> (32 - table_nb_bits) != code_prefix)
                    break;
                codes[k].bits = n;
                codes[k].code = code << table_nb_bits;
                subtable_bits = MAX(subtable_bits, n);
            }

            subtable_bits = MIN(subtable_bits, table_nb_bits);
            j = code_prefix;
            table[j].len = -subtable_bits;
            log_info("%4x: n=%d (subtable)\n", j, codes[i].bits + table_nb_bits);

            index = build_table(vlc, subtable_bits, k-i, codes+i);
            if (index < 0)
                return index;
            /* note: realloc has been done, so reload tables */
            table = &vlc->table[table_index];
            table[j].sym = index;

            if (table[j].sym != index) {
                log_info("strange codes\n");
                return AVERROR_PATCHWELCOME;
            }
            i = k-1;
        }
    }

    for (int i = 0; i < table_size; i++) {
        if (table[i].len == 0)
            table[i].sym = -1;
    }

    return table_index;
}

static void add_level(
    VLC_MULTI_ELEM *table,
    const int num,
    const VLCcode *buf,
    uint32_t curcode, int curlen,
    int curlimit, const int curlevel,
    const int minlen, const int max,
    unsigned* levelcnt, VLC_MULTI_ELEM info
) {
    const int next_level = curlevel + 1;
    for (int i = num-1; i >= max; i--) {
        int l = buf[i].bits;
        uint32_t code;
        int sym = buf[i].symbol;
        if (l >= curlimit)
            return;
        code = curcode + (buf[i].code >> curlen);
        int newlimit = curlimit - l;
        l += curlen;
        info.val[curlevel] = sym&0xFF;
        if (curlevel) { // let's not add single entries
            uint32_t val = code >> (32 - UT_VLC_BITS);
            uint32_t nb = val + (1U << (UT_VLC_BITS - l));
            info.len = l;
            info.num = next_level;
            for (; val < nb; val++) {
                table[val] = info;
            }
            levelcnt[curlevel-1]++;
        }
        if (next_level < VLC_MULTI_MAX_SYMBOLS && newlimit >= minlen) {
            add_level(table, num, buf,
                      code, l, newlimit, curlevel+1,
                      minlen, max, levelcnt, info);
        }

        if (i > max) {
            i--;
            l = buf[i].bits;
            sym = buf[i].symbol;
            if (l >= curlimit)
                return;
            code = curcode + (buf[i].code >> curlen);
            newlimit = curlimit - l;
            l += curlen;
            info.val[curlevel] = sym&0xFF;
            if (curlevel) { // let's not add single entries
                uint32_t val = code >> (32 - UT_VLC_BITS);
                uint32_t nb = val + (1U << (UT_VLC_BITS - l));
                info.len = l;
                info.num = next_level;
                for (; val < nb; val++) {
                    table[val] = info;
                }
                levelcnt[curlevel-1]++;
            }
            if (next_level < VLC_MULTI_MAX_SYMBOLS && newlimit >= minlen) {
                add_level(table, num, buf,
                          code, l, newlimit, curlevel+1,
                          minlen, max, levelcnt, info);
            }
        }
    }
}


static int vlc_multi_gen(VLC_MULTI_ELEM *table, const VLC *single,
                         const int nb_codes,
                         VLCcode *buf)
{
    int minbits, maxbits, max;
    unsigned count[VLC_MULTI_MAX_SYMBOLS-1] = { 0, };
    VLC_MULTI_ELEM info = { { 0, }, 0, 0, };
    int count0 = 0;

    for (int j = 0; j < 1<<UT_VLC_BITS; j++) {
        if (single->table[j].len > 0) {
            count0++;
            j += (1 << (UT_VLC_BITS - single->table[j].len)) - 1;
        }
    }

    minbits = 32;
    maxbits = 0;

    for (int n = nb_codes - count0; n < nb_codes; n++) {
        minbits = MIN(minbits, buf[n].bits);
        maxbits = MAX(maxbits, buf[n].bits);
    }
    av_assert0(maxbits <= UT_VLC_BITS);

    for (max = nb_codes; max > nb_codes - count0; max--) {
        // We can only add a code that fits with the shortest other code into the table
        // We assume the table is sorted by bits and we skip subtables which from our
        // point of view are basically random corrupted entries
        // If we have not a single useable vlc we end with max = nb_codes
        if (buf[max - 1].bits+minbits > UT_VLC_BITS)
            break;
    }

    for (int j = 0; j < 1<<UT_VLC_BITS; j++) {
        table[j].len = single->table[j].len;
        table[j].num = single->table[j].len > 0 ? 1 : 0;
        WRITE_U16(table[j].val, single->table[j].sym);
    }

    add_level(table, nb_codes, buf,
              0, 0, MIN(maxbits, UT_VLC_BITS), 0, minbits, max, count, info);

    log_info("Joint: %d/%d/%d/%d/%d codes min=%ubits max=%u\n",
           count[0], count[1], count[2], count[3], count[4], minbits, max);

    return 0;
}

static av_always_inline void vlc_free(VLC *vlc) {
    free(vlc->table);
    vlc->table = NULL;
}

static av_always_inline void vlc_free_multi(VLC_MULTI *vlc) {
    free(vlc->table);
    vlc->table = NULL;
}

static int vlc_common_end(
    VLC *vlc,
    int nb_bits,
    int nb_codes,
    VLCcode *codes,
    VLCcode localbuf[LOCALBUF_ELEMS]
) {
    int ret = build_table(vlc, nb_bits, nb_codes, codes);

    if (codes != localbuf)
        free(codes);
    if (ret < 0) {
        free(vlc->table);
        vlc->table = NULL;
        return ret;
    }
    return 0;
}

static int vlc_init_common(VLC *vlc, int nb_codes,
                           VLCcode **buf)
{
    vlc->table_size = 0;
    vlc->table           = NULL;
    vlc->table_allocated = 0;
    if (nb_codes > LOCALBUF_ELEMS) {
        *buf = av_malloc(nb_codes * sizeof(VLCcode));
        if (!*buf)
            return AVERROR(ENOMEM);
    }

    return 0;
}

int vlc_init_multi_from_lengths(
    VLC *vlc, VLC_MULTI *multi,
    int nb_codes,
    const int8_t *lens, int lens_wrap,
    const void *symbols, int symbols_wrap
) {
    VLCcode localbuf[LOCALBUF_ELEMS], *buf = localbuf;
    uint64_t code;
    int ret, j, len_max = MIN(32, 3 * UT_VLC_BITS);

    ret = vlc_init_common(vlc, nb_codes, &buf);
    if (ret < 0)
        return ret;

    multi->table = av_malloc(sizeof(VLC_MULTI_ELEM) << UT_VLC_BITS);
    if (!multi->table)
        return AVERROR(ENOMEM);

    j = code = 0;
    for (int i = 0; i < nb_codes; i++, lens += lens_wrap) {
        int len = *lens;
        if (len > 0) {
            unsigned sym;

            buf[j].bits = len;
            if (symbols)
                VLC_GET_DATA(sym, symbols, i, symbols_wrap, UT_VLC_SYMBOLS_SIZE)
            else
                sym = i;
            buf[j].symbol = sym;
            buf[j++].code = code;
        } else if (len <  0) {
            len = -len;
        } else
            continue;
        if (len > len_max || code & ((1U << (32 - len)) - 1)) {
            log_info("Invalid VLC (length %d)\n", len);
            goto fail;
        }
        code += 1U << (32 - len);
        if (code > UINT32_MAX + 1ULL) {
            log_info("Overdetermined VLC tree\n");
            goto fail;
        }
    }
    ret = vlc_common_end(vlc, UT_VLC_BITS, j, buf, buf);
    if (ret < 0)
        goto fail;
    ret = vlc_multi_gen(multi->table, vlc, j, buf);
    if (buf != localbuf)
        free(buf);
    log_info("Ret=%d\n", ret);
    return ret;
fail:
    if (buf != localbuf)
        free(buf);
    vlc_free_multi(multi);
    return AVERROR_INVALIDDATA;
}

#undef VLC_GET_DATA

#endif // __UT_VLC_H__
