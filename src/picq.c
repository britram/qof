/*
 ** picq.c
 ** General pickable queue implementation
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2006-2012 Carnegie Mellon University. All Rights Reserved.
 ** ------------------------------------------------------------------------
 ** Authors: Brian Trammell
 ** ------------------------------------------------------------------------
 ** @OPENSOURCE_HEADER_START@
 ** Use of the YAF system and related source code is subject to the terms
 ** of the following licenses:
 **
 ** GNU Public License (GPL) Rights pursuant to Version 2, June 1991
 ** Government Purpose License Rights (GPLR) pursuant to DFARS 252.227.7013
 **
 ** NO WARRANTY
 **
 ** ANY INFORMATION, MATERIALS, SERVICES, INTELLECTUAL PROPERTY OR OTHER
 ** PROPERTY OR RIGHTS GRANTED OR PROVIDED BY CARNEGIE MELLON UNIVERSITY
 ** PURSUANT TO THIS LICENSE (HEREINAFTER THE "DELIVERABLES") ARE ON AN
 ** "AS-IS" BASIS. CARNEGIE MELLON UNIVERSITY MAKES NO WARRANTIES OF ANY
 ** KIND, EITHER EXPRESS OR IMPLIED AS TO ANY MATTER INCLUDING, BUT NOT
 ** LIMITED TO, WARRANTY OF FITNESS FOR A PARTICULAR PURPOSE,
 ** MERCHANTABILITY, INFORMATIONAL CONTENT, NONINFRINGEMENT, OR ERROR-FREE
 ** OPERATION. CARNEGIE MELLON UNIVERSITY SHALL NOT BE LIABLE FOR INDIRECT,
 ** SPECIAL OR CONSEQUENTIAL DAMAGES, SUCH AS LOSS OF PROFITS OR INABILITY
 ** TO USE SAID INTELLECTUAL PROPERTY, UNDER THIS LICENSE, REGARDLESS OF
 ** WHETHER SUCH PARTY WAS AWARE OF THE POSSIBILITY OF SUCH DAMAGES.
 ** LICENSEE AGREES THAT IT WILL NOT MAKE ANY WARRANTY ON BEHALF OF
 ** CARNEGIE MELLON UNIVERSITY, EXPRESS OR IMPLIED, TO ANY PERSON
 ** CONCERNING THE APPLICATION OF OR THE RESULTS TO BE OBTAINED WITH THE
 ** DELIVERABLES UNDER THIS LICENSE.
 **
 ** Licensee hereby agrees to defend, indemnify, and hold harmless Carnegie
 ** Mellon University, its trustees, officers, employees, and agents from
 ** all claims or demands made against them (and any related losses,
 ** expenses, or attorney's fees) arising out of, or relating to Licensee's
 ** and/or its sub licensees' negligent use or willful misuse of or
 ** negligent conduct or willful misconduct regarding the Software,
 ** facilities, or other rights or assistance granted by Carnegie Mellon
 ** University under this License, including, but not limited to, any
 ** claims of product liability, personal injury, death, damage to
 ** property, or violation of any laws or regulations.
 **
 ** Carnegie Mellon University Software Engineering Institute authored
 ** documents are sponsored by the U.S. Department of Defense under
 ** Contract FA8721-05-C-0003. Carnegie Mellon University retains
 ** copyrights in all material produced under this contract. The U.S.
 ** Government retains a non-exclusive, royalty-free license to publish or
 ** reproduce these documents, or allow others to do so, for U.S.
 ** Government purposes only pursuant to the copyright license under the
 ** contract clause at 252.227.7013.
 **
 ** @OPENSOURCE_HEADER_END@
 ** ------------------------------------------------------------------------
 */

#define _YAF_SOURCE_
#include <qof/picq.h>

typedef struct _PicQNode {
    struct _PicQNode    *p;
    struct _PicQNode    *n;
} PicQNode;

typedef struct _PicQ {
    PicQNode            *tail;
    PicQNode            *head;
} PicQ;

void piqPick (
    void        *vq,
    void        *vn)
{
    PicQ        *queue = (PicQ *)vq;
    PicQNode    *node = (PicQNode *)vn;

    /* only allow picking a double-null node if it's both head and tail. */
    if (!node->n && !node->p &&
        !(node == queue->head && node == queue->tail))
    {
        return;
    }

    /* connect previous to next */
    if (node->p) {
        node->p->n = node->n;
    } else {
        queue->tail = node->n;
    }

    /* connect next to previous */
    if (node->n) {
        node->n->p = node->p;
    } else {
        queue->head = node->p;
    }

    /* mark node picked */
    node->n = NULL;
    node->p = NULL;
}

void piqEnQ(
    void        *vq,
    void        *vn)
{
    PicQ        *queue = (PicQ *)vq;
    PicQNode    *node = (PicQNode *)vn;

    g_assert(!node->n && !node->p);

    if (queue->head) {
        queue->head->n = node;
    } else {
        queue->tail = node;
    }

    node->p = queue->head;
    queue->head = node;
}

void piqUnshift(
    void        *vq,
    void        *vn)
{
    PicQ        *queue = (PicQ *)vq;
    PicQNode    *node = (PicQNode *)vn;

    g_assert(!node->n && !node->p);

    if (queue->tail) {
        queue->tail->p = node;
    } else {
        queue->head = node;
    }

    node->n = queue->tail;
    queue->tail = node;
}

void *piqShift(
    void        *vq)
{
    PicQ        *queue = (PicQ *)vq;
    PicQNode    *node = NULL;

    if (queue->head) {
        node = queue->head;
        piqPick(queue, node);
    }
    return node;
}

void *piqDeQ(
    void        *vq)
{
    PicQ        *queue = (PicQ *)vq;
    PicQNode    *node = NULL;

    if (queue->tail) {
        node = queue->tail;
        piqPick(queue, node);
    }
    return node;
}
