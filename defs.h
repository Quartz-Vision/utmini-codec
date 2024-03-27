#ifndef __UT_DEFS_H__
#define __UT_DEFS_H__

#define av_alias __attribute__((may_alias))
#define av_packed __attribute__((packed))
#define av_transparent __attribute__((transparent_union))
#define av_union av_packed av_transparent av_alias
#define av_unused __attribute__((unused))
#define av_const __attribute__((const))
#define av_cold __attribute__((cold))
#define av_always_inline __attribute__((always_inline)) inline
#define av_pure_expr av_always_inline av_const


/**
 * @ingroup lavc_decoding
 * Required number of additionally allocated bytes at the end of the input bitstream for decoding.
 * This is mainly needed because some optimized bitstream readers read
 * 32 or 64 bit at once and could read over the end.<br>
 * Note: If the first 23 bits of the additional bytes are not 0, then damaged
 * MPEG bitstreams could cause overread and segfault.
 */
#define AV_INPUT_BUFFER_PADDING_SIZE 64

#define UT_COLOR_PLANES 3
#define UT_MAX_VLC_DEPTH 3
#define UT_VLC_BITS 11
#define UT_HUFF_ELEMS 256
#define UT_VLC_SYMBOLS_SIZE 2

#endif
