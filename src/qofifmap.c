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
    return memcmp(a, b, 16);
}

static void qfIp6Mask(uint8_t *ma, uint8_t *a, uint8_t *mask) {
    int i;
    for (i = 0; i < 16; i++) {
        ma[i] = a[i] & mask[i];
    }
}

static uint32_t qfPrefixMask4(uint8_t p) {
    int i;
    uint32_t o = 0;
    
    for (i = 0; i < p; i++) {
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
    int i;
    for (i = 0; i < 16; i++) addr[i] = ~addr[i];
}

static void qfIp6BitAnd(uint8_t *lhs, uint8_t *a, uint8_t *b) {
    int i;
    for (i = 0; i < 16; i++) lhs[i] = a[i] & b[i];
}

static void qfIp6BitOr(uint8_t *lhs, uint8_t *a, uint8_t *b) {
    int i;
    for (i = 0; i < 16; i++) lhs[i] = a[i] | b[i];
}


static uint8_t qfMapSearch4(qfIfMapEntry4_t     *map,
                            size_t              map_sz,
                            uint32_t            addr)
{
    /* initialize bounds */
    ssize_t x = 0;
    ssize_t y = map_sz - 1;
    
    size_t i;

    /* and converge */
    while (x <= y) {
        i = (x+y)/2;
        
        if (addr >= map[i].a && addr <= map[i].b) {
            return map[i].ifnum;
        } else if (map[i].b < addr) {
            x = i + 1;
        } else {
            y = i - 1;
        }
    }
    
    return 0;
}

static uint8_t qfMapSearch6(qfIfMapEntry6_t     *map,
                            size_t              map_sz,
                            uint8_t             *addr)
{
    /* initialize bounds */
    ssize_t x = 0;
    ssize_t y = map_sz - 1;
    
    size_t i;

    /* and converge */
    while (x <= y) {
        i = (x+y)/2;

        if ((qfIp6Compare(addr, map[i].a) > -1) &&
            (qfIp6Compare(addr, map[i].b) < 1)) {
            return map[i].ifnum;
        } else if (qfIp6Compare(map[i].a, addr) < 0) {
            x = i + 1;
        } else {
            y = i - 1;
        }
    }
    
    return 0;
}


static void qfIfMap4Dump(FILE*                     out,
                         qfIfMapEntry4_t           *map,
                         size_t                    map_sz)
{
    int i;
    uint32_t a, b;
    char abuf[16], bbuf[16];
    
    for (i = 0; i < map_sz; i++) {
        a = ntohl(map[i].a);
        b = ntohl(map[i].b);
        (void)inet_ntop(AF_INET, &a, abuf, sizeof(abuf));
        (void)inet_ntop(AF_INET, &b, bbuf, sizeof(bbuf));
        fprintf(out, "%s - %s -> %3u\n", abuf, bbuf, map[i].ifnum);
    }
}

static void qfIfMap6Dump(FILE*                     out,
                         qfIfMapEntry6_t           *map,
                         size_t                    map_sz)
{
    int i;
    char abuf[40], bbuf[40];
    
    for (i = 0; i < map_sz; i++) {
        (void)inet_ntop(AF_INET6, map[i].a, abuf, sizeof(abuf));
        (void)inet_ntop(AF_INET6, map[i].b, bbuf, sizeof(bbuf));
        fprintf(out, "%s - %s -> %3u\n", abuf, bbuf, map[i].ifnum);
    }
}

static void qfMapInsert4(qfIfMapEntry4_t       **map,
                         size_t                *map_sz,
                         uint32_t              addr,
                         uint8_t               pfx,
                         uint8_t               ifnum)
{
    size_t i = 0;
    uint32_t mask;
    
    qfIfMapEntry4_t     *new_map;
    
    // linear search to insertion point
    if (*map_sz == 0 || addr < (*map)[0].a) {
        i = 0;
    } else if (addr > (*map)[(*map_sz)-1].b) {
        i = *map_sz;
    } else for (i = 1; i < *map_sz; i++) {
        if (((*map)[i-1].b < addr) && (addr < (*map)[i].a)) break;
    }
        
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
        memcpy(&((*map)[i+1]), &((*map)[i]),
               (*map_sz - (i+1)) * sizeof(qfIfMapEntry4_t));
    }
    
    // create new element
    mask = qfPrefixMask4(pfx);
    (*map)[i].a = addr & mask;
    (*map)[i].b = (addr | (~mask));
    (*map)[i].ifnum = ifnum;
}

static void qfMapInsert6(qfIfMapEntry6_t       **map,
                         size_t                *map_sz,
                         uint8_t               *addr,
                         uint8_t               pfx,
                         uint8_t               ifnum)
{
    size_t i = 0;
    uint8_t mask[16];
    
    qfIfMapEntry6_t     *new_map;

    // linear search to insertion point
    if (*map_sz == 0 || qfIp6Compare(addr, (*map)[0].a) < 0) {
        i = 0;
    } else if (qfIp6Compare(addr, (*map)[(*map_sz)-1].b) > 0) {
        i = *map_sz;
    } else for (i = 1; i < *map_sz; i++) {
        if ((qfIp6Compare((*map)[i-1].b, addr) < 0) &&
            (qfIp6Compare(addr, (*map)[i].a) < 0)) break;
    }

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
        memcpy(&((*map)[i+1]), &((*map)[i]),
               (*map_sz - (i+1)) * sizeof(qfIfMapEntry6_t));
    }
    
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

void qfIfMapFree(qfIfMap_t *map) {
    if (map->src4map) free(map->src4map);
    if (map->dst4map) free(map->dst4map);
    if (map->src6map) free(map->src6map);
    if (map->dst6map) free(map->dst6map);
    qfIfMapInit(map);
}

void qfIfMapAddIPv4Mapping(qfIfMap_t    *map,
                         uint32_t       addr,
                         uint8_t        pfx,
                         uint8_t        ingress,
                         uint8_t        egress)
{
//    char addrbuf[16];
//    uint32_t naddr = htonl(addr);
//    if (inet_ntop(AF_INET, &naddr, addrbuf, sizeof(addrbuf))) {
//        fprintf(stderr, "** qofifmap adding IPv4 mapping %s/%u => (%u,%u)\n",
//                addrbuf, pfx, ingress, egress);
//    } else {
//        fprintf(stderr, "error unparsing IPv4 address 0x%08x: %s\n", naddr, strerror(errno));
//    }
    
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
//    char addrbuf[40];
//    if (inet_ntop(AF_INET6, addr, addrbuf, sizeof(addrbuf))) {
//        fprintf(stderr, "** qofifmap adding IPv6 mapping %s/%u => (%u,%u)\n",
//                addrbuf, pfx, ingress, egress);
//    } else {
//        fprintf(stderr, "error unparsing IPv6 address: %s\n", strerror(errno));
//    }
    
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

void qfIfMapDump(FILE*                      out,
                 qfIfMap_t                  *map)
{
    fprintf(out, "ip4 source map:\n");
    qfIfMap4Dump(out, map->src4map, map->src4map_sz);
    fprintf(out, "ip6 source map:\n");
    qfIfMap6Dump(out, map->src6map, map->src6map_sz);
    fprintf(out, "ip4 dest map:\n");
    qfIfMap4Dump(out, map->dst4map, map->dst4map_sz);
    fprintf(out, "ip6 dest map:\n");
    qfIfMap6Dump(out, map->dst6map, map->dst6map_sz);
}

void qfNetListAddIPv4(qfNetList_t       *list,
                      uint32_t          addr,
                      uint8_t           pfx)
{
    qfMapInsert4(&(list->ip4map), &(list->ip4map_sz), addr, pfx, 1);
}

void qfNetListAddIPv6(qfNetList_t       *list,
                      uint8_t           *addr,
                      uint8_t           pfx)
{
    qfMapInsert6(&(list->ip6map), &(list->ip6map_sz), addr, pfx, 1);
}



qfNetDirection_t qfFlowDirection(qfNetList_t       *srclist,
                                 yfFlowKey_t       *key)
{
    int ss, ds;
    
    if (key->version == 4) {
        ss = qfMapSearch4(srclist->ip4map, srclist->ip4map_sz, key->addr.v4.sip);
        ds = qfMapSearch4(srclist->ip4map, srclist->ip4map_sz, key->addr.v4.dip);
    } else if (key->version == 6) {
        ss = qfMapSearch6(srclist->ip6map, srclist->ip6map_sz, key->addr.v6.sip);
        ds = qfMapSearch6(srclist->ip6map, srclist->ip6map_sz, key->addr.v6.dip);
    } else {
        ss = 0;
        ds = 0;
    }
    
    if (ss && ds) {
        return QF_DIR_INT;
    } else if (ss) {
        return QF_DIR_IN;
    } else if (ds) {
        return QF_DIR_OUT;
    } else {
        return QF_DIR_EXT;
    }
}

                      