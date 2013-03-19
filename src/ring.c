/*
 ** ring.c
 ** General ring array implementation
 **
 ** ------------------------------------------------------------------------
 ** Copyright (C) 2007-2012 Carnegie Mellon University. All Rights Reserved.
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
#include <yaf/ring.h>

struct rgaRing_st {
    size_t          elt_sz;
    size_t          cap;
    size_t          count;
    size_t          peak;
    size_t          hrsv;
    size_t          trsv;
    uint8_t         *base;
    uint8_t         *end;
    uint8_t         *head;
    uint8_t         *tail;
#if YAF_RING_THREAD
    GMutex          *mtx;
    GCond           *cnd_zero;
    GCond           *cnd_full;
    uint32_t        interrupt;
#endif
};

/**
 * rgaAlloc
 *
 *
 *
 */
rgaRing_t *rgaAlloc(
    size_t          elt_sz,
    size_t          cap)
{
    rgaRing_t        *ring = NULL;
    size_t            alignedEltSize = elt_sz;

#   if HAVE_ALIGNED_ACCESS_REQUIRED
    alignedEltSize += (elt_sz % (sizeof(uint64_t)));
#   endif
    /* allocate the structure */
    ring = yg_slice_new0(rgaRing_t);

    /* allocate the buffer */
    ring->base = yg_slice_alloc0(alignedEltSize * cap);

    /* note last element in array */
    ring->end = ring->base + (alignedEltSize * (cap - 1));

    /* set head and tail pointers to start of ring */
    ring->head = ring->tail = ring->base;

    /* stash element size and capacity */
    ring->elt_sz = alignedEltSize;
    ring->cap = cap;

    /* All done. */
    return ring;
}

#if YAF_RING_THREAD
/**
 * rgaAllocThreaded
 *
 *
 *
 */
rgaRing_t *rgaAllocThreaded(
    size_t          elt_sz,
    size_t          cap)
{
    rgaRing_t        *ring = rgaAlloc(elt_sz, cap);

    /* allocate mutex and conditions */
    ring->mtx = g_mutex_new();
    ring->cnd_zero = g_cond_new();
    ring->cnd_full = g_cond_new();

    return ring;
}
#endif

/**
 * rgaFree
 *
 *
 *
 */
void rgaFree(
    rgaRing_t       *ring)
{
    size_t UNUSED(base_sz);

    base_sz = ring->elt_sz * ring->cap;

#if YAF_RING_THREAD
    /* free conditions and mutex if present */
    if (ring->cnd_zero) {
        g_cond_free(ring->cnd_zero);
    }

    if (ring->cnd_full) {
        g_cond_free(ring->cnd_full);
    }

    if (ring->mtx) {
        g_mutex_free(ring->mtx);
    }
#endif

    /* free buffer */
    yg_slice_free1(base_sz, ring->base);

    /* free structure */
    yg_slice_free(rgaRing_t, ring);
}

/**
 * rgaNextHead
 *
 *
 *
 */
uint8_t *rgaNextHead(
    rgaRing_t       *ring)
{
    uint8_t         *head;

    /* return null if buffer full */
    if (ring->count >= (ring->cap - ring->trsv)) {
        return NULL;
    }

    /* get head pointer */
    head = ring->head;

    /* advance head pointer and wrap */
    ring->head += ring->elt_sz;
    if (ring->head > ring->end) {
        ring->head = ring->base;
    }

    /* keep count and peak */
    ++(ring->count);
    if (ring->count > ring->peak) {
        ring->peak = ring->count;
    }

    /* return head pointer */
    return head;
}

/**
 * rgaForceHead
 *
 *
 *
 */
uint8_t *rgaForceHead(
    rgaRing_t       *ring)
{
    uint8_t         *head;
        
    /* get head pointer */
    head = ring->head;
    
    /* advance head pointer and wrap */
    ring->head += ring->elt_sz;
    if (ring->head > ring->end) {
        ring->head = ring->base;
    }

    /* count addition */
    ++(ring->count);
    
    /* advance tail pointer if buffer full */
    if (ring->count >= (ring->cap - ring->trsv)) {
        ring->tail += ring->elt_sz;
        if (ring->tail > ring->end) {
            ring->tail = ring->base;
        }
        --(ring->count);
    }
    
    /* track post-force peak */
    if (ring->count > ring->peak) {
        ring->peak = ring->count;
    }

    
    
    /* return head pointer */
    return head;
}

/**
 * rgaPeekHead
 */

uint8_t *rgaPeekHead(rgaRing_t       *ring)
{
    /* return null if buffer empty */
    if (ring->count <= 0) {
        return NULL;
    }
    
    /* return head pointer */
    return ring->head;
}



#if YAF_RING_THREAD
/**
 * rgaNextHead
 *
 *
 *
 */
uint8_t *rgaNextHead(
    rgaRing_t       *ring)
{
    uint8_t         *head = NULL;

    g_mutex_lock(ring->mtx);
    while (!ring->interrupt && ((head = rgaNextHead(ring)) == NULL)) {
        g_cond_wait(ring->cnd_full, ring->mtx);
    }
    if (ring->interrupt) {
        head = NULL;
        goto end;
    }
    if (++(ring->hrsv) > ring->cap) {
        ring->hrsv = ring->cap;
    }
    g_cond_signal(ring->cnd_zero);
end:
    g_mutex_unlock(ring->mtx);
    return head;
}
#endif

#if YAF_RING_THREAD
/**
 * rgaReleaseHead
 *
 *
 *
 */
void rgaReleaseHead(
    rgaRing_t       *ring,
    size_t          rsv)
{
    g_mutex_lock(ring->mtx);
    if (rsv > ring->hrsv) {
        rsv = ring->hrsv;
    }
    ring->hrsv -= rsv;
    g_cond_signal(ring->cnd_full);
    g_mutex_unlock(ring->mtx);
}
#endif

/**
 * rgaNextTail
 *
 *
 *
 */
uint8_t *rgaNextTail(
    rgaRing_t       *ring)
{
    uint8_t         *tail;

    /* return null if buffer empty */
    if (ring->count <= ring->hrsv) {
        return NULL;
    }

    /* get tail pointer */
    tail = ring->tail;

    /* advance tail pointer and wrap */
    ring->tail += ring->elt_sz;
    if (ring->tail > ring->end) {
        ring->tail = ring->base;
    }

    /* keep count */
    --(ring->count);

    /* return tail pointer */
    return tail;
}

/**
 * rgaPeekTail
 */

uint8_t *rgaPeekTail(
    rgaRing_t       *ring)
{
    /* return null if buffer empty */
    if (ring->count <= 0) {
        return NULL;
    }

    /* return tail pointer */
    return ring->tail;
}

#if YAF_RING_THREAD
/**
 * rgaWaitTail
 *
 *
 *
 */
uint8_t *rgaWaitTail(
    rgaRing_t       *ring)
{
    uint8_t         *tail = NULL;

    g_mutex_lock(ring->mtx);
    while (!ring->interrupt && ((tail = rgaNextTail(ring)) == NULL)) {
        g_cond_wait(ring->cnd_zero, ring->mtx);
    }
    if (ring->interrupt) {
        tail = NULL;
        goto end;
    }
    if (++(ring->trsv) >= ring->cap) {
        ring->trsv = ring->cap;
    }
    g_cond_signal(ring->cnd_full);
end:
    g_mutex_unlock(ring->mtx);
    return tail;
}
#endif

#if YAF_RING_THREAD
/**
 * rgaReleaseTail
 *
 *
 *
 */
void rgaReleaseTail(
    rgaRing_t       *ring,
    size_t          rsv)
{
    g_mutex_lock(ring->mtx);
    if (rsv > ring->trsv) {
        rsv = ring->trsv;
    }
    ring->trsv -= rsv;
    g_cond_signal(ring->cnd_zero);
    g_mutex_unlock(ring->mtx);
}
#endif

#if YAF_RING_THREAD
/**
 * rgaSetInterrupt
 *
 *
 *
 */
void rgaSetInterrupt(
    rgaRing_t       *ring)
{
    g_mutex_lock(ring->mtx);
    ++(ring->interrupt);
    g_cond_broadcast(ring->cnd_zero);
    g_cond_broadcast(ring->cnd_full);
    g_mutex_unlock(ring->mtx);
}
#endif

#if YAF_RING_THREAD
/**
 * rgaClearInterrupt
 *
 *
 *
 */
void rgaClearInterrupt(
    rgaRing_t       *ring)
{
    g_mutex_lock(ring->mtx);
    --(ring->interrupt);
    g_mutex_unlock(ring->mtx);
}
#endif

/**
 * rgaCount
 *
 *
 *
 */
size_t rgaCount(
    rgaRing_t       *ring)
{
    return ring->count;
}

/**
 * rgaPeak
 *
 *
 *
 */
size_t rgaPeak(
    rgaRing_t       *ring)
{
    return ring->peak;
}
