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

/**
 * @file
 *
 * Generic Pickable Queue. A pickable queue's elements may be removed from any
 * point in the queue, and added to the queue's head or tail. Together with a
 * hash table to locate mid-queue elements, this can be used to implement idle
 * timeout of its elements.
 *
 * Where present, a <tt>vq</tt> argument is a void pointer to a queue. A
 * queue's first two members must be a pointer to the tail (last node) of the
 * queue and a pointer to the head (first node) of the queue.
 *
 * Where present, a <tt>vn</tt> argument is a void pointer to a queue node.
 * A queue node's first two elements must be a pointer to the previous node
 * in the queue and a pointer to the next node in the queue.
 */

/* idem hack */
#ifndef _YAF_PICQ_H_
#define _YAF_PICQ_H_
#include <qof/autoinc.h>


/**
 * Pick a node from a given pickable queue. The node is removed from the queue,
 * and its previous and next pointers are set to NULL. It is assumed that the
 * node is actually an element of the given queue; undefined behavior may
 * result if this is not the case.
 *
 * @param vq queue to remove from
 * @param vn node to remove
 */

void piqPick (
    void        *vq,
    void        *vn);

/**
 * Enqueue a node at the head of a given pickable queue. The node must not be
 * an element in another queue; that is, its own previous and next pointers
 * must be NULL. To move a node from one queue to another, use piqPick()
 * first.
 *
 * @param vq queue to enqueue to
 * @param vn node to enqueue
 */

void piqEnQ(
    void        *vq,
    void        *vn);

/**
 * Enqueue a node at the tail of a given pickable queue. The node must not be
 * an element in another queue; that is, its own previous and next pointers
 * must be NULL. To move a node from one queue to another, use piqPick()
 * first.
 *
 * @param vq queue to enqueue to
 * @param vn node to enqueue
 */

void piqUnshift(
    void        *vq,
    void        *vn);

/**
 * Dequeue a node from the head of a given pickable queue. Analogous to finding
 * the head, picking it, then returning it. The node is removed from the queue,
 * and its previous and next pointers are set to NULL. Returns NULL if the
 * queue is empty.
 *
 * @param vq queue to remove from
 * @return the dequeued head of the queue, or NULL if empty.
 */

void *piqShift(
    void        *vq);

/**
 * Dequeue a node from the tail of a given pickable queue. Analogous to finding
 * the tail, picking it, then returning it. The node is removed from the queue,
 * and its previous and next pointers are set to NULL. Returns NULL if the
 * queue is empty.
 *
 * @param vq queue to remove from
 * @return the dequeued tail of the queue, or NULL if empty.
 */

void *piqDeQ(
    void        *vq);

/* end idem */
#endif
