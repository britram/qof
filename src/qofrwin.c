/**
 ** qofrwin.c
 ** Receiver window functions for QoF
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
#include <qof/qofrwin.h>

void qfRwinScale(qfRwin_t       *qr,
                 uint8_t        wscale)
{
    qr->scale = wscale;
}

void qfRwinSegment(qfRwin_t     *qr,
                   uint32_t     unscaled)
{
    if ((qr->val.last > 0) && (unscaled == 0)) {
        qr->stall++;
    }
    
    sstMeanAdd(&qr->val, unscaled << qr->scale);
}
