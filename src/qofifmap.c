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
    uint8_t         iface;
} qfIfMapEntry4_t;

typedef struct qfIfMapEntry6_st {
    uint8_t         a[16];
    uint8_t         mask[16];
    uint8_t         iface;    
} qfIfMapEntry6_t;

typedef struct qfIfMap_st {
    qfIfMapEntry4_t     src4map[QF_IFMAP_MAXSZ];
    size_t              src4map_len;
    qfIfMapEntry4_t     dst4map[QF_IFMAP_MAXSZ];
    size_t              dst4map_len;
    qfIfMapEntry6_t     src6map[QF_IFMAP_MAXSZ];
    size_t              src6map_len;
    qfIfMapEntry6_t     dst6map[QF_IFMAP_MAXSZ];
    size_t              dst6map_len;
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

static uint32_t qfMapMask4(uint8_t p) {

}

static void qfMapMask6(uint8_t *mask, uint8_t p) {
    
}

static uint8_t qfMapSearch4(qfIfMapEntry4_t     *m,
                            size_t              l,
                            uint32_t            a)
{
    size_t i = l/2;
    while (1) {
        if (a < m[i].a) {
            // search address less than range start
            if (i == 0) {
                break;
            } else {
                i /= 2;
            }
        } else if ((a & m[i].mask) == m[i].a) {
            // match, return interface
            return m[i].iface
        } else if (i >= l) {
            // search address greater and at EOA
            break;
        } else if (a < m[i+1].a) {
            // search address greater but less than next
            break;
        } else {
            // search address greater than range start
            i += (l-i)/2;
        }
    }
}

static uint8_t qfMapSearch6(qfIfMapEntry4_t     *m,
                            size_t              l,
                            uint8_t             *a)
{
    uint8_t ma[16];
    size_t i = l/2;
    
    while (1) {
        qfIp6Mask(ma, a, m[i].mask);
        
        if (qfIp6Compare(a, m[i].a) < 0) {
            // search address less than range start
            if (i == 0) {
                break;
            } else {
                i /= 2;
            }
        } else if (qfIp6Compare(ma, m[i].a) < 0) {
            // match, return interface
            return m[i].iface
        } else if (i >= l) {
            // search address greater and at EOA
            break;
        } else if (qfIp6Compare(a, m[i+1].a) < 0) {
            // search address greater but less than next
            break;
        } else {
            // search address greater than range start
            i += (l-i)/2;
        }
    }
}

void qfMapAddresses(qfIfMap_t           *map,
                    yfFlowKey_t         *key, 
                    uint8_t             *ingress, 
                    uint8_t             *egress 
{
    if (key->version == 4) {
        *ingress = qfMapSearch4(map->src4map, map->src4map_len, key->addr.v4.sip);
        *egress =  qfMapSearch4(map->dst4map, map->dst4map_len, key->addr.v4.dip);
    } else if (key->version == 6) {
        *ingress = qfMapSearch6(map->src6map, map->src6map_len, key->addr.v6.sip);
        *egress =  qfMapSearch6(map->dst6map, map->dst6map_len, key->addr.v6.dip);
    } else {
        // alien version, die
    }
    
}