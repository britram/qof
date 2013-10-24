/**
 ** @file qofmaclist.h
 **
 ** MAC address set, based on a sorted array search.
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2013 Brian Trammell. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Author: Brian Trammell <brian@trammell.ch>
 ** ------------------------------------------------------------------------
 ** QoF is made available under the terms of the GNU Public License (GPL)
 ** Version 2, June 1991
 ** ------------------------------------------------------------------------
 */

#ifndef _QOF_MACLIST_H_
#define _QOF_MACLIST_H_

#include <qof/autoinc.h>
#include <qof/yafcore.h>
#include <qof/yaftab.h>

struct qfMacList_st {
    /* IPv4 addresses */
    uint8_t             *macaddrs;
    size_t              macaddrs_sz;
};

#define QF_MACLIST_INIT {NULL, 0}

void qfMacListInit(qfMacList_t         *list);

void qfMacListFree(qfMacList_t         *list);

void qfMacListAdd(qfMacList_t           *list,
                  uint8_t               *macaddr);

int qfMacListContains(qfMacList_t           *list,
                      uint8_t               *macaddr);


#endif