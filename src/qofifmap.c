/**
 ** @file qofifmap.c
 **
 ** Address to interface map, based on a sorted array search.
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2012-2013 Brian Trammell. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Author: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the GNU Public License (GPL) 
 ** Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#define _YAF_SOURCE_
#include <yaf/autoinc.h>
#include <yaf/yaftab.h>

#include <yaf/qofifmap.h>

#include <arpa/inet.h>

struct qfIfMapEntry4_st {
    uint32_t        a;
    uint32_t        b;
    uint8_t         ifnum;
};

struct qfIfMapEntry6_st {
    uint8_t         a[16];
    uint8_t         b[16];
    uint8_t         ifnum;    
};

static int qfIp6Compare(uint8_t *a, uint8_t *b) {
    for (int i = 0; i < 16; i++) {
        if (a[i] > b[i]) return 1;
        if (a[i] < b[i]) return -1;
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
    // now handle the boundary
    for (j = 0; (j < p % 8) && (i < 16); j++) {
        mask[i] >>= 1;
        mask[i] |= 0x80U;
    }    
}

static void qfIp6BitInvert(uint8_t *addr) {
    for (int i = 0; i < 16; i++) addr[i] = ~addr[i];
}

static void qfIp6BitAnd(uint8_t *lhs, uint8_t *a, uint8_t *b) {
    for (int i = 0; i < 16; i++) lhs[i] = a[i] & b[i];
}

static void qfIp6BitOr(uint8_t *lhs, uint8_t *a, uint8_t *b) {
    for (int i = 0; i < 16; i++) lhs[i] = a[i] | b[i];
}


static uint8_t qfMapSearch4(qfIfMapEntry4_t     *map,
                            size_t              map_sz,
                            uint32_t            addr)
{
    /* initialize bounds */
    size_t x = 0;
    size_t y = map_sz - 1;
    
    int i;

    /* and converge */
    while (x <= y) {
        i = (x+y)/2;
        
        if (map[i].b < addr) {
            y = i - 1;
        } else if (addr >= map[i].a && addr <= map[i].b) {
            return map[i].ifnum;
        } else {
            x = i + 1;
        }
    }
    
    return 0;
}

static uint8_t qfMapSearch6(qfIfMapEntry6_t     *map,
                            size_t              map_sz,
                            uint8_t             *addr)
{
    /* initialize bounds */
    size_t x = 0;
    size_t y = map_sz - 1;
    
    int i;

    /* and converge */
    while (x <= y) {
        i = (x+y)/2;
        
        if (qfIp6Compare(map[i].a, addr) < 0) {
            y = i - 1;
        } else {
            if ((qfIp6Compare(map[i].a, addr) < 1) &&
                (qfIp6Compare(map[i].b, addr) > -1)) {
                return map[i].ifnum;
            } else {
                x = i + 1;
            }
        }
    }
    
    return 0;
}

static void qfMapInsert4(qfIfMapEntry4_t       **map,
                         size_t                *map_sz,
                         uint32_t              addr,
                         uint8_t               pfx,
                         uint8_t               ifnum)
{
    size_t i, j;
    uint32_t mask;
    
    qfIfMapEntry4_t     *new_map;
    
    // linear search to insertion point
    for (i = 0; (i < *map_sz) && (addr < (*map)[i].b); i++);
    
    // FIXME check for overlap
    
    // reallocate map
    new_map = realloc(*map, (*map_sz+1) * sizeof(qfIfMapEntry4_t));
    if (!new_map) {
        // FIXME integrate a horrible death into qof's normal error handling
        exit(1);
    }
    *map = new_map;
    (*map_sz)++;
    
    // shift further elements
    if (i < *map_sz - 1) {
        memcpy(&(*map)[i+1], &(*map)[i],
               (*map_sz - (i+1)) * sizeof(qfIfMapEntry4_t));
    }
    
    // create new element
    mask = qfPrefixMask4(pfx);
    (*map)[i].a = addr & mask;
    (*map)[i].b = addr | ~mask;
    (*map)[i].ifnum = ifnum;
}

static void qfMapInsert6(qfIfMapEntry6_t       **map,
                         size_t                *map_sz,
                         uint8_t               *addr,
                         uint8_t               pfx,
                         uint8_t               ifnum)
{
    size_t i, j;
    uint8_t mask[16];
    
    qfIfMapEntry6_t     *new_map;

    // linear search to insertion point
    for (i = 0; (i < *map_sz) && (qfIp6Compare(addr,(*map)[i].b) < 0); i++);
    
    // FIXME check for overlap
    
    // reallocate map
    new_map = realloc(*map, (*map_sz+1) * sizeof(qfIfMapEntry6_t));
    if (!new_map) {
        // FIXME integrate a horrible death into qof's normal error handling
        exit(1);
    }
    *map = new_map;
    (*map_sz)++;
    
    // shift further elements
    if (i < *map_sz - 1) {
        memcpy(&(*map)[i+1], &(*map)[i],
               (*map_sz - (i+1)) * sizeof(qfIfMapEntry6_t));
    }
        
//    if (*map_sz > 1) {
//        for (j = (*map_sz) - 1; j > i; j--) {
//            memcpy(&map[j], &map[j-1], sizeof(map[j-1]));
//        }
//    }
    
    // create new element
    qfPrefixMask6(mask,pfx);
    qfIp6BitAnd((*map)[i].a, addr, mask);
    qfIp6BitInvert(mask);
    qfIp6BitOr((*map)[i].b, addr, mask);
    (*map)[i].ifnum = ifnum;
}

void qfIfMapInit(qfIfMap_t *map)
{
    map->src4map = NULL;
    map->src6map = NULL;
    map->dst4map = NULL;
    map->dst6map = NULL;
    map->src4map_sz = 0;
    map->src6map_sz = 0;
    map->dst4map_sz = 0;
    map->dst6map_sz = 0;
}

void qfIfMapAddIPv4Mapping(qfIfMap_t    *map,
                         uint32_t       addr,
                         uint8_t        pfx,
                         uint8_t        ingress,
                         uint8_t        egress)
{
    char addrbuf[16];
    uint32_t naddr = htonl(addr);
    if (inet_ntop(AF_INET, &naddr, addrbuf, sizeof(addrbuf))) {
        fprintf(stderr, "** qofifmap adding IPv4 mapping %s/%u => (%u,%u)\n",
                addrbuf, pfx, ingress, egress);
    } else {
        fprintf(stderr, "error unparsing IPv4 address 0x%08x: %s\n", naddr, strerror(errno));
    }
    
    if (ingress) {
        qfMapInsert4(&(map->src4map), &(map->src4map_sz),
                     addr, pfx, ingress);
    }
    if (egress) {
        qfMapInsert4(&(map->dst4map), &(map->dst4map_sz),
                     addr, pfx, egress);
    }
}


void qfIfMapAddIPv6Mapping(qfIfMap_t      *map,
                           uint8_t        *addr,
                           uint8_t        pfx,
                           uint8_t        ingress,
                           uint8_t        egress)
{
    char addrbuf[40];
    if (inet_ntop(AF_INET6, addr, addrbuf, sizeof(addrbuf))) {
        fprintf(stderr, "** qofifmap adding IPv6 mapping %s/%u => (%u,%u)\n",
                addrbuf, pfx, ingress, egress);
    } else {
        fprintf(stderr, "error unparsing IPv6 address: %s\n", strerror(errno));
    }
    
    if (ingress) {
        qfMapInsert6(&(map->src6map), &(map->src6map_sz),
                     addr, pfx, ingress);
    }
    if (egress) {
        qfMapInsert6(&(map->dst6map), &(map->dst6map_sz),
                     addr, pfx, egress);
    }

}

void qfIfMapAddresses(qfIfMap_t           *map,
                      yfFlowKey_t         *key, 
                      uint8_t             *ingress, 
                      uint8_t             *egress)
{
    if (key->version == 4) {
        *ingress = qfMapSearch4(map->src4map, map->src4map_sz, key->addr.v4.sip);
        *egress =  qfMapSearch4(map->dst4map, map->dst4map_sz, key->addr.v4.dip);
    } else if (key->version == 6) {
        *ingress = qfMapSearch6(map->src6map, map->src6map_sz, key->addr.v6.sip);
        *egress =  qfMapSearch6(map->dst6map, map->dst6map_sz, key->addr.v6.dip);
    } else {
        // FIXME alien version, die
        *ingress = 0;
        *egress = 0;        
    }
}

                      