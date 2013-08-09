/**
 ** bitmap.c
 ** basic bitmap data structure for QoF 
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2013      Brian Trammell. All Rights Reserved
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the GNU Public License (GPL)
 ** Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#define _YAF_SOURCE_
#include <yaf/bitmap.h>

static const uint64_t bimBit[] = {
    0x0000000000000001ULL, 0x0000000000000002ULL,
    0x0000000000000004ULL, 0x0000000000000008ULL,
    0x0000000000000010ULL, 0x0000000000000020ULL,
    0x0000000000000040ULL, 0x0000000000000080ULL,
    0x0000000000000100ULL, 0x0000000000000200ULL,
    0x0000000000000400ULL, 0x0000000000000800ULL,
    0x0000000000001000ULL, 0x0000000000002000ULL,
    0x0000000000004000ULL, 0x0000000000008000ULL,
    0x0000000000010000ULL, 0x0000000000020000ULL,
    0x0000000000040000ULL, 0x0000000000080000ULL,
    0x0000000000100000ULL, 0x0000000000200000ULL,
    0x0000000000400000ULL, 0x0000000000800000ULL,
    0x0000000001000000ULL, 0x0000000002000000ULL,
    0x0000000004000000ULL, 0x0000000008000000ULL,
    0x0000000010000000ULL, 0x0000000020000000ULL,
    0x0000000040000000ULL, 0x0000000080000000ULL,
    0x0000000100000000ULL, 0x0000000200000000ULL,
    0x0000000400000000ULL, 0x0000000800000000ULL,
    0x0000001000000000ULL, 0x0000002000000000ULL,
    0x0000004000000000ULL, 0x0000008000000000ULL,
    0x0000010000000000ULL, 0x0000020000000000ULL,
    0x0000040000000000ULL, 0x0000080000000000ULL,
    0x0000100000000000ULL, 0x0000200000000000ULL,
    0x0000400000000000ULL, 0x0000800000000000ULL,
    0x0001000000000000ULL, 0x0002000000000000ULL,
    0x0004000000000000ULL, 0x0008000000000000ULL,
    0x0010000000000000ULL, 0x0020000000000000ULL,
    0x0040000000000000ULL, 0x0080000000000000ULL,
    0x0100000000000000ULL, 0x0200000000000000ULL,
    0x0400000000000000ULL, 0x0800000000000000ULL,
    0x1000000000000000ULL, 0x2000000000000000ULL,
    0x4000000000000000ULL, 0x8000000000000000ULL
};

static const uint64_t bimMaskA[] = {
    0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFEULL,
    0xFFFFFFFFFFFFFFFCULL, 0xFFFFFFFFFFFFFFF8ULL,
    0xFFFFFFFFFFFFFFF0ULL, 0xFFFFFFFFFFFFFFE0ULL,
    0xFFFFFFFFFFFFFFC0ULL, 0xFFFFFFFFFFFFFF80ULL,
    0xFFFFFFFFFFFFFF00ULL, 0xFFFFFFFFFFFFFE00ULL,
    0xFFFFFFFFFFFFFC00ULL, 0xFFFFFFFFFFFFF800ULL,
    0xFFFFFFFFFFFFF000ULL, 0xFFFFFFFFFFFFE000ULL,
    0xFFFFFFFFFFFFC000ULL, 0xFFFFFFFFFFFF8000ULL,
    0xFFFFFFFFFFFF0000ULL, 0xFFFFFFFFFFFE0000ULL,
    0xFFFFFFFFFFFC0000ULL, 0xFFFFFFFFFFF80000ULL,
    0xFFFFFFFFFFF00000ULL, 0xFFFFFFFFFFE00000ULL,
    0xFFFFFFFFFFC00000ULL, 0xFFFFFFFFFF800000ULL,
    0xFFFFFFFFFF000000ULL, 0xFFFFFFFFFE000000ULL,
    0xFFFFFFFFFC000000ULL, 0xFFFFFFFFF8000000ULL,
    0xFFFFFFFFF0000000ULL, 0xFFFFFFFFE0000000ULL,
    0xFFFFFFFFC0000000ULL, 0xFFFFFFFF80000000ULL,
    0xFFFFFFFF00000000ULL, 0xFFFFFFFE00000000ULL,
    0xFFFFFFFC00000000ULL, 0xFFFFFFF800000000ULL,
    0xFFFFFFF000000000ULL, 0xFFFFFFE000000000ULL,
    0xFFFFFFC000000000ULL, 0xFFFFFF8000000000ULL,
    0xFFFFFF0000000000ULL, 0xFFFFFE0000000000ULL,
    0xFFFFFC0000000000ULL, 0xFFFFF80000000000ULL,
    0xFFFFF00000000000ULL, 0xFFFFE00000000000ULL,
    0xFFFFC00000000000ULL, 0xFFFF800000000000ULL,
    0xFFFF000000000000ULL, 0xFFFE000000000000ULL,
    0xFFFC000000000000ULL, 0xFFF8000000000000ULL,
    0xFFF0000000000000ULL, 0xFFE0000000000000ULL,
    0xFFC0000000000000ULL, 0xFF80000000000000ULL,
    0xFF00000000000000ULL, 0xFE00000000000000ULL,
    0xFC00000000000000ULL, 0xF800000000000000ULL,
    0xF000000000000000ULL, 0xE000000000000000ULL,
    0xC000000000000000ULL, 0x8000000000000000ULL,
};

static const uint64_t bimMaskB[] = {
    0x0000000000000001ULL, 0x0000000000000003ULL,
    0x0000000000000007ULL, 0x000000000000000FULL,
    0x000000000000001FULL, 0x000000000000003FULL,
    0x000000000000007FULL, 0x00000000000000FFULL,
    0x00000000000001FFULL, 0x00000000000003FFULL,
    0x00000000000007FFULL, 0x0000000000000FFFULL,
    0x0000000000001FFFULL, 0x0000000000003FFFULL,
    0x0000000000007FFFULL, 0x000000000000FFFFULL,
    0x000000000001FFFFULL, 0x000000000003FFFFULL,
    0x000000000007FFFFULL, 0x00000000000FFFFFULL,
    0x00000000001FFFFFULL, 0x00000000003FFFFFULL,
    0x00000000007FFFFFULL, 0x0000000000FFFFFFULL,
    0x0000000001FFFFFFULL, 0x0000000003FFFFFFULL,
    0x0000000007FFFFFFULL, 0x000000000FFFFFFFULL,
    0x000000001FFFFFFFULL, 0x000000003FFFFFFFULL,
    0x000000007FFFFFFFULL, 0x00000000FFFFFFFFULL,
    0x00000001FFFFFFFFULL, 0x00000003FFFFFFFFULL,
    0x00000007FFFFFFFFULL, 0x0000000FFFFFFFFFULL,
    0x0000001FFFFFFFFFULL, 0x0000003FFFFFFFFFULL,
    0x0000007FFFFFFFFFULL, 0x000000FFFFFFFFFFULL,
    0x000001FFFFFFFFFFULL, 0x000003FFFFFFFFFFULL,
    0x000007FFFFFFFFFFULL, 0x00000FFFFFFFFFFFULL,
    0x00001FFFFFFFFFFFULL, 0x00003FFFFFFFFFFFULL,
    0x00007FFFFFFFFFFFULL, 0x0000FFFFFFFFFFFFULL,
    0x0001FFFFFFFFFFFFULL, 0x0003FFFFFFFFFFFFULL,
    0x0007FFFFFFFFFFFFULL, 0x000FFFFFFFFFFFFFULL,
    0x001FFFFFFFFFFFFFULL, 0x003FFFFFFFFFFFFFULL,
    0x007FFFFFFFFFFFFFULL, 0x00FFFFFFFFFFFFFFULL,
    0x01FFFFFFFFFFFFFFULL, 0x03FFFFFFFFFFFFFFULL,
    0x07FFFFFFFFFFFFFFULL, 0x0FFFFFFFFFFFFFFFULL,
    0x1FFFFFFFFFFFFFFFULL, 0x3FFFFFFFFFFFFFFFULL,
    0x7FFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL
};


unsigned int bimCountSet(uint64_t v) {
    static const uint64_t mask[] = {
        0x5555555555555555ULL,
        0x3333333333333333ULL,
        0x0F0F0F0F0F0F0F0FULL,
        0x00FF00FF00FF00FFULL,
        0x0000FFFF0000FFFFULL,
        0x00000000FFFFFFFFULL
    };
    
    static const unsigned int shift[] = { 1, 2, 4, 8, 16, 32 };
    
    uint64_t c;
    
    c = v - ((v >> shift[0]) & mask[0]);
    c = ((c >> shift[1]) & mask[1]) + (c & mask[1]);
    c = ((c >> shift[2]) + c) & mask[2];
    c = ((c >> shift[3]) + c) & mask[3];
    c = ((c >> shift[4]) + c) & mask[4];
    return (unsigned int)((c >> shift[5]) + c) & mask[5];
}

unsigned int    bimHighBitSet(uint64_t v)
{
    uint64_t m = 0x8000000000000000ULL;
    unsigned int bit = 64;
    
    while (m > v) { m >>= 1; bit--; }
    return bit;
}

void bimInit(bimBitmap_t *bitmap, uint32_t capacity) {
    /* zero structure */
    memset(bitmap, 0, sizeof(*bitmap));
    
    /* allocate bitmap array */
    bitmap->sz = capacity / k64Bits;
    if (capacity % k64Bits) bitmap->sz += 1;
    bitmap->v = yg_slice_alloc0(bitmap->sz * sizeof(uint64_t));
}

void            bimFree(bimBitmap_t *bitmap) {
    if (bitmap->v) yg_slice_free1(bitmap->sz * sizeof(uint64_t), bitmap->v);
}

static bimIntersect_t bimTSRCombine (bimIntersect_t prev, bimIntersect_t cur) {
    switch (prev) {
        case BIM_START:
            switch (cur) {
                case BIM_END: return BIM_EDGE;
                case BIM_NONE: return BIM_START;
                default: return BIM_PART;
            }
        case BIM_NONE:
            return (cur == BIM_NONE) ? BIM_NONE: BIM_PART;
        case BIM_FULL:
            return (cur == BIM_FULL) ? BIM_FULL: BIM_PART;
        case BIM_END:
        case BIM_EDGE:
        default:
            return BIM_PART;
    }
}

static bimIntersect_t bimTestAndSetInner(uint64_t *v, uint32_t a, uint32_t b) {
    bimIntersect_t res;
    uint64_t m = bimMaskA[a] & bimMaskB[b];
    
    if ((*v & m) == 0) res = BIM_NONE;
    else if ((*v & m) == m) res = BIM_FULL;
    else if ((*v & m) == bimBit[a]) res = BIM_START;
    else if ((*v & m) == bimBit[b]) res = BIM_END;
    else res = BIM_PART;
    
    *v |= m;
    
    return res;
}

bimIntersect_t  bimTestAndSetRange(bimBitmap_t *bitmap, uint32_t a, uint32_t b) {
    uint32_t        i, j;
    bimIntersect_t  res;
    
    /* compute indices */
    i = (bitmap->base + (a / k64Bits)) % bitmap->sz;
    j = (bitmap->base + (b / k64Bits)) % bitmap->sz;
    a %= k64Bits;
    b %= k64Bits;
    
    if (i == j) {
        /* only one range */
        res = bimTestAndSetInner(&bitmap->v[i], a, b);
    } else {
        res = bimTestAndSetInner(&bitmap->v[i], a, 63);
        for (i = (i + 1) % bitmap->sz;
             i != j;
             i = (i + 1) % bitmap->sz) {
            res = bimTSRCombine(res,bimTestAndSetInner(&bitmap->v[i], 0, 63));
        }
        res = bimTSRCombine(res, bimTestAndSetInner(&bitmap->v[i], 0, b));
    }
    
    return res;
}

unsigned int    bimTestBit(bimBitmap_t *bitmap, uint32_t a) {
    uint32_t        i;
    
    /* compute index */
    i = (bitmap->base + (a / k64Bits)) % bitmap->sz;
    a %= k64Bits;
    
    return bitmap->v[i] & bimBit[a] ? 1 : 0;
}

uint64_t bimShiftDown(bimBitmap_t *bitmap) {
    uint64_t v = bitmap->v[bitmap->base];
    bitmap->v[bitmap->base] = 0;
    bitmap->base = (bitmap->base + 1) % bitmap->sz;
    return v;
}







