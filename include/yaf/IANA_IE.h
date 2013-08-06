/*
** @file IANA_IE.h
** Definition of IANA information elements used by QoF but not supported
** by libfixbuf. This is a bit of a hack.
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

#ifndef IANA_IE_H_
#define IANA_IE_H_

static fbInfoElement_t yaf_iana_extra_info_elements[] = {
    FB_IE_INIT("transportOctetDeltaCount", 0, 401, 8, FB_IE_F_ENDIAN | FB_IE_F_REVERSIBLE),
    FB_IE_INIT("transportPacketDeltaCount", 0, 402, 8, FB_IE_F_ENDIAN ),
    FB_IE_NULL
};

#endif