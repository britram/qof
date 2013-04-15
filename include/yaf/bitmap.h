/**
 ** bitmap.h
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

#ifndef _QOF_BITMAP_H_
#define _QOF_BITMAP_H_

#include <yaf/autoinc.h>

#define k64Bits (sizeof(uint64_t) * 8)

typedef struct bimBitmap_st {
    uint64_t    *v;
    uint32_t    sz;
    uint32_t    base;
} bimBitmap_t;

typedef enum {
    /** No part of range already set */
    BIM_NONE = 0,
    /** Only start of range already set */
    BIM_START = 1,
    /** Only start of range already set */
    BIM_END = 2,
    /** Only edges of range already set */
    BIM_EDGE = 3,
    /** All of range already set */
    BIM_FULL = 4,
    /** Part of range already set */
    BIM_PART = 7
} bimIntersect_t;

void            bimInit(bimBitmap_t *bitmap, uint32_t capacity);

void            bimFree(bimBitmap_t *bitmap);

bimIntersect_t  bimTestAndSetRange(bimBitmap_t *bitmap, uint32_t a, uint32_t b);

unsigned int    bimTestBit(bimBitmap_t *bitmap, uint32_t a);

unsigned int    bimCountSet(uint64_t v);

unsigned int    bimHighBitSet(uint64_t v);

uint64_t        bimShiftDown(bimBitmap_t *bitmap);

#endif
