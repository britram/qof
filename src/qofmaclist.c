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
#include <yaf/qofmaclist.h>

#define MAC_SZ 6

static int qfMacCompare(uint8_t *a, uint8_t *b) {
    return memcmp(a, b, MAC_SZ);
}

void qfMacListInit(qfMacList_t         *list) {
    memset(list, 0, sizeof(qfMacList_t));
}

void qfMacListFree(qfMacList_t         *list) {
    if (list->macaddrs) {
        free(list->macaddrs);
    }
}

void qfMacListAdd(qfMacList_t           *list,
                  uint8_t               *macaddr)
{
    size_t i = 0;
    uint8_t *new_macaddrs;
    
    // linear search to insertion point
    if (list->macaddrs_sz == 0 || qfMacCompare(macaddr, &list->macaddrs[0])) {
        i = 0;
    } else if (qfMacCompare(macaddr,
                            &list->macaddrs[(list->macaddrs_sz - 1) * MAC_SZ]) > 0)
    {
        i = list->macaddrs_sz;
    } else for (i = 1; i < list->macaddrs_sz; i++) {
        if ((qfMacCompare(&list->macaddrs[(i-1) * MAC_SZ], macaddr) > 0) &&
            (qfMacCompare(macaddr, &list->macaddrs[i * MAC_SZ]) > 0 )) break;
    }
    
    new_macaddrs = realloc(list->macaddrs, (list->macaddrs_sz + 1) * MAC_SZ);
    if (!new_macaddrs) {
        // FIXME integrate a horrible death into qof's normal error handling
        exit(1);
    }
    list->macaddrs = new_macaddrs;
    
    // shift up
    if (i < list->macaddrs_sz - 1) {
        memcpy(list->macaddrs[(i + 1) * MAC_SZ],
               list->macaddrs[i * MAC_SZ],
               (list->macaddrs_sz - (i+1)) * MAC_SZ);
    }
    
    // copy element in
    memcpy(list->macaddrs[i * MAC_SZ], macaddr, MAC_SZ);
    
}

int qfMacListContains(qfMacList_t           *list,
                      uint8_t               *macaddr)
{
    /* initialize bounds */
    ssize_t x = 0;
    ssize_t y = list->macaddrs_sz - 1;
    
    size_t i;
    
    /* and converge */
    while (x <= y) {
        i = (x+y)/2;
        
        if (qfMacCompare(macaddr, &list->macaddrs[i * MAC_SZ]) == 0) {
            return 1;
        } else if (qfMacCompare(&list->macaddrs[i * MAC_SZ], macaddr) < 0) {
            x = i + 1;
        } else {
            y = i - 1;
        }
    }
    
    return 0;
    
}