#ifndef __UT_MEM_H__
#define __UT_MEM_H__

#include "defs.h"
#include <stddef.h>
#include <stdlib.h>

#define MEM_ALIGN_SIZE 4

void av_always_inline * av_malloc(size_t size) {
    return aligned_alloc(MEM_ALIGN_SIZE, size);
}


void av_always_inline * av_realloc_f(void *ptr, size_t nelem, size_t elsize) {
    void * ret = realloc(ptr, nelem * elsize);
    if (!ret)
        free(ptr);
    return ret;
}

#endif
