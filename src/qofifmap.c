/**
 ** @file qofifmap.c
 **
 ** Address to interface map, based on a searchable array
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2012 Brian Trammell. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Author: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the GNU Public License (GPL) 
 ** Version 2, June 1991
 ** ------------------------------------------------------------------------
 */
     
#define QF_IFMAP_MAXSZ 16

typedef struct qfIfMapEntry4_st {
    uint32_t        a;
    uint32_t        mask;
    uint8_t         ifnum;
} qfIfMapEntry4_t;

typedef struct qfIfMapEntry6_st {
    uint8_t         a[16];
    uint8_t         mask[16];
    uint8_t         ifnum;    
} qfIfMapEntry6_t;

typedef struct qfIfMap_st {
    /* IPv4 sources */
    qfIfMapEntry4_t     *src4map;
    size_t              src4map_sz;
    /* IPv4 destinations */
    qfIfMapEntry4_t     *dst4map;
    size_t              dst4map_sz;
    /* IPv6 sources */
    qfIfMapEntry6_t     *src6map;
    size_t              src6map_sz;
    /* IPv6 destinations */
    qfIfMapEntry6_t     *dst6map;
    size_t              dst6map_sz;
} qfIfMap_t;

static int qfIp6Compare(uint8_t *a, uint8_t *b) {
    for (int i = 0; i < 16; i++) {
        if a[i] > b[i] return 1;
        if a[i] < b[i] return -1;
    }
    return 0;
}

static void qfIp6Mask(uint8_t *ma, uint8_t *a, uint8_t *mask) {
    for (int i = 0; i < 16; i++) {
        ma[i] = a[i] & mask[i];
    }
}

static uint32_t qfPrefixMask4(uint8_t p) {
    uint32_t o = 0;
    
    for (int i = 0; i < 32; i++) {
        o >>= 1;
        o |= 0x80000000U;
    }
    return o;
}

static void qfPrefixMask6(uint8_t *mask, uint8_t p) {
    int i, j;
    // fill in full bytes first
    for (i = 0; i < p / 8; i++) {
        mask[i] = 0xffU;
    }
    // zero the rest of the mask
    for (j = i; j < 16; j++) {
        mask[j] = 0;
    }
    // now handle the boudary
    for (j = 0; (j < p % 8) && (i < 16); j++) {
        mask[i] >>= 1;
        mask[i] |= 0x80U;
    }    
}

static uint8_t qfMapSearch4(qfIfMapEntry4_t     *map,
                            size_t              map_sz,
                            uint32_t            a)
{
    /* initialize bounds */
    int x = 0;
    int y = map_sz - 1;
    
    int i;

    /* and converge */
    while (x <= y) {
        i = (x+y)/2;
        
        if (map[i].a < a) {
            y = i - 1;
        } else if (map[i].a == (map[i].mask & a)) {
            return map[i].ifnum;
        } else {
            x = i + 1;
        }
    }
}

static uint8_t qfMapSearch6(qfIfMapEntry6_t     *map,
                            size_t              map_sz,
                            uint8_t             *a)
{
    /* initialize bounds */
    int x = 0;
    int y = map_sz - 1;
    
    int i;

    /* and converge */
    while (x <= y) {
        i = (x+y)/2;
        
        if (qfIp6Compare(map[i].a, a) < 0) {
            y = i - 1;
        } else {
            qfIp6Mask(ma, a, map[i].mask);

            if (qfIp6Compare(map[i].a, ma) == 0) {
                return map[i];
            } else {
                x = i + 1;
            }
        }
    }
}


static void qfMapInsert4(qfIfMapEntry4         **map,
                       size_t                *map_sz,
                       uint32_t              a,
                       uint8_t               pfx,
                       uint8_t               ifnum)
{
    
}

static void qfMapInsert6(qfIfMapEntry6         **map,
                         size_t                *map_sz,
                         uint8_t               *a,
                         uint8_t               pfx,
                         uint8_t               ifnum)
{
    
}

qfIfMap_t *qfIfMapParse(FILE           *in,
                        GError         **err)
{
    
}

void qfIfMapFree(qfIfMap_t *map) {
    
}

void qfMapAddresses(qfIfMap_t           *map,
                    yfFlowKey_t         *key, 
                    uint8_t             *ingress, 
                    uint8_t             *egress)
{
    if (key->version == 4) {
        *ingress = qfMapSearch4(map->src4map, map->src4map_len, key->addr.v4.sip);
        *egress =  qfMapSearch4(map->dst4map, map->dst4map_len, key->addr.v4.dip);
    } else if (key->version == 6) {
        *ingress = qfMapSearch6(map->src6map, map->src6map_len, key->addr.v6.sip);
        *egress =  qfMapSearch6(map->dst6map, map->dst6map_len, key->addr.v6.dip);
    } else {
        // alien version, die
        *ingress = 0;
        *egress = 0;        
    }
}
                      