/**
** qofiat.c
** Interarrival time and TCP flight data structures and algorithms for QoF
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

#ifndef _QOF_IAT_H_
#define _QOF_IAT_H_

#include <yaf/autoinc.h>
#include <yaf/streamstat.h>

typedef struct qfIat_st {
    sstV_t     segiat;
    sstV_t     fliat;
} qfIat_t;

#endif