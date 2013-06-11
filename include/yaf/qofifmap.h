/**
 ** @file qofifmap.h
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

#ifndef _QOF_IFMAP_H_
#define _QOF_IFMAP_H_

#include <yaf/autoinc.h>
#include <yaf/yafcore.h>
#include <yaf/yaftab.h>

struct qfIfMapEntry4_st;
typedef struct qfIfMapEntry4_st qfIfMapEntry4_t;

struct qfIfMapEntry6_st;
typedef struct qfIfMapEntry6_st qfIfMapEntry6_t;

struct qfIfMap_st {
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
};

struct qfNetList_st {
    /* IPv4 addresses */
    qfIfMapEntry4_t     *ip4map;
    size_t              ip4map_sz;
    qfIfMapEntry6_t     *ip6map;
    size_t              ip6map_sz;
};

typedef enum {
    QF_DIR_IN,
    QF_DIR_OUT,
    QF_DIR_EXT,
    QF_DIR_INT
} qfNetDirection_t;

#define QF_IFMAP_INIT {NULL, 0, NULL, 0, NULL, 0, NULL, 0}

void qfIfMapInit(qfIfMap_t *map);

void qfIfMapFree(qfIfMap_t *map);

void qfIfMapAddIPv4Mapping(qfIfMap_t      *map,
                           uint32_t       addr,
                           uint8_t        pfx,
                           uint8_t        ingress,
                           uint8_t        egress);

void qfIfMapAddIPv6Mapping(qfIfMap_t      *map,
                           uint8_t        *addr,
                           uint8_t        pfx,
                           uint8_t        ingress,
                           uint8_t        egress);

void qfIfMapAddresses(qfIfMap_t           *map,
                      yfFlowKey_t         *key,
                      uint8_t             *ingress,
                      uint8_t             *egress);

void qfIfMapDump(FILE*                      out,
                 qfIfMap_t                  *map);

void qfNetListAddIPv4(qfNetList_t       *list,
                      uint32_t          addr,
                      uint8_t           pfx);

void qfNetListAddIPv6(qfNetList_t       *list,
                      uint8_t           *addr,
                      uint8_t           pfx);

void qfMacListAdd(qfMacList_t           *list,
                  uint8_t               *macaddr);

qfNetDirection_t qfFlowDirection(qfNetList_t       *srclist,
                                 yfFlowKey_t       *key);

#endif
