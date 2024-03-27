#ifndef __UT_VIDEO_H__
#define __UT_VIDEO_H__

#include "defs.h"
#include "mem.h"
#include "utils.h"
#include <stdint.h>
#include <string.h>

#define LINE_ALIGNMENT_PAD 16


typedef struct VideoContext {
    uint16_t w;
    uint16_t h;
    uint32_t slices;
    
    int linesize;
    uint8_t * frame_data[UT_COLOR_PLANES];

    uint32_t * result_frame_data;

    uint8_t * packet_data;
    uint32_t packet_size;

    uint8_t * slice_buf;
    uint32_t slice_buf_size;
    
    uint8_t * vlc_buf;
    uint32_t vlc_buf_size;
} VideoContext;


int video_init(VideoContext * ctx) {
    for (int i = 0; i < UT_COLOR_PLANES; i++) {
        // The linesize can be larger than frame width
        ctx->frame_data[i] = av_malloc((ctx->w + LINE_ALIGNMENT_PAD) * ctx->h);
    }
    ctx->linesize = ctx->w + LINE_ALIGNMENT_PAD;

    ctx->packet_data = av_malloc(ctx->w * ctx->h * 4);

    ctx->slice_buf_size = ctx->w * ctx->h * 4 + ctx->w * 4;
    ctx->slice_buf = av_malloc(ctx->slice_buf_size);

    ctx->vlc_buf_size = ctx->w + 8;
    ctx->vlc_buf = av_malloc(ctx->vlc_buf_size);
    memset(ctx->vlc_buf, 0, ctx->vlc_buf_size);

    return 0;
}

int video_from_data(VideoContext * c, uint8_t * data) {
    c->w = CONSUME_U16(data);
    c->h = CONSUME_U16(data);
    
    // fps, frames
    data += 6;

    c->slices = CONSUME_U32(data);
    
    return video_init(c);
}

void video_free(VideoContext * ctx) {
    for (int i = 0; i < UT_COLOR_PLANES; i++) {
        free(ctx->frame_data[i]);
    }
    free(ctx->packet_data);
    free(ctx->slice_buf);
    free(ctx->vlc_buf);
}


#endif // __UT_VIDEO_H__
