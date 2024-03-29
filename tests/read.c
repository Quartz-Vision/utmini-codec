#include "decoder.h"
#include "video.h"

#include <stdint.h>
#include <stdio.h>

#define HEADER_START_KEY 0xF0FF00F0
#define HEADER_END_KEY 0x7FF1

#define PACKET_START_KEY 0xFFF0F0F0
#define PACKET_END_KEY 0xF0F1


static VideoContext ctx;

int video_read_header(FILE * f) {
    static uint8_t buf[256];

    // Header size and end-key
    if (fread(buf, sizeof(uint8_t) + sizeof(uint16_t), 1, f) == 0) {
        // End of file or error reading
        return 0;
    }

    if (*(uint16_t*)(buf + 1) != HEADER_END_KEY) {
        // Not a video packet
        return 0;
    }

    // Read the header data
    if (fread(buf, *buf, 1, f) == 0) {
        // End of file or error reading
        return 0;
    }

    video_from_data(&ctx, buf);

    ctx.result_frame_data = av_malloc((ctx.w + LINE_ALIGNMENT_PAD) * ctx.h * 4);

    return 1;
}


int video_read_next_frame(FILE * file) {
    static uint8_t buf[256];

    // Find a frame
    while (1) {
        // Read the start key
        if (fread(buf, sizeof(uint32_t), 1, file) == 0) {
            // End of file or error reading
            return 0;
        }
        log_info("Key: %x\n", *(uint32_t*)buf);
        
        if (*(uint32_t*)buf == HEADER_START_KEY) {
            video_read_header(file);
            continue;
        }

        if (*(uint32_t*)buf != PACKET_START_KEY) continue;

        // Header size and end-key
        if (fread(buf, sizeof(uint8_t) + sizeof(uint16_t), 1, file) == 0) {
            // End of file or error reading
            return 0;
        }
        log_info("Header size: %d\n", *buf);

        if (*(uint16_t*)(buf + 1) != PACKET_END_KEY) {
            // Not a video packet
            continue;
        }

        // Read the header data
        if (fread(buf, *buf, 1, file) == 0) {
            // End of file or error reading
            return 0;
        }
        ctx.packet_size = *(uint32_t*)buf;
        log_info("Data size: %d\n", ctx.packet_size);

        break;
    }


    // Read the packet
    if (fread(ctx.packet_data, ctx.packet_size, 1, file) == 0) {
        // End of file or error reading
        return 0;
    }

    log_info("Packet read\n");

    int got_frame = 0;
    int err = 0;
    if ((err = decode_frame(&ctx, &got_frame)) < 0) {
        log_info("Error decoding frame: %d\n", err);
        return 0;
    }

    if (got_frame) {
        log_info("Frame decoded\n");
    }

    return 1;
}


int main(int argc, char ** argv) {
    if (argc < 3) {
        printf("Usage: %s <lav file (in)> <file (out)>\n", argv[0]);
        return 1;
    }
    FILE * file_in = fopen(argv[1], "rb");
    FILE * file_out = fopen(argv[2], "wb");

    if (file_in == NULL) {
        printf("Error opening file\n");
        return 1;
    }

    int ttt = 0;
    while (video_read_next_frame(file_in)) {
        // Process the frame
        //fwrite((uint8_t*)ctx.result_frame_data, (ctx.w + LINE_ALIGNMENT_PAD) * ctx.h * 4, 1, file_out);
        //break;
        ttt++;
    }

    printf("Frames: %d\n", ttt);

    fclose(file_out);
    fclose(file_in);
    free(ctx.result_frame_data);
    video_free(&ctx);
    return 0;
}
