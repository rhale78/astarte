/**********************************************************************
 * File:    rts/append.c
 * Purpose: Append function handler and evaluator
 * Author:  Karl Abrahamson
 **********************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 1997 Karl Abrahamson					*
 * All rights reserved.							*
 *									*
 * Redistribution and use in source and binary forms, with or without	*
 * modification, are permitted provided that the following conditions	*
 * are met:								*
 *									*
 * 1. Redistributions of source code must retain the above copyright	*
 *    notice, this list of conditions and the following disclaimer.	*
 *									*
 * 2. Redistributions in binary form must reproduce the above copyright	*
 *    notice in the documentation and/or other materials provided with 	*
 *    the distribution.							*
 *									*
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY		*
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE	*
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 	*
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE	*
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 	*
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 	*
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 	*
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,*
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE *
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,    * 
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.			*
 *									*
 ************************************************************************/
#endif

/************************************************************************
 * This file provides support for the append operation on lists.  It	*
 * implements Okasaki's list data structure, providing operations for	*
 * doing appends and for evaluating the result of lazy appends.		*
 *									*
 * Okasaki's data structure implements a list as a tree.  The list	*
 * represented by a tree consists of a preorder traversal of the tree.	*
 * Each tree node is either occupied, in which case the head of the	*
 * list is in the root, or is vacant, in which case some rearrangement	*
 * is required to find the head.					*
 *									*
 * The children	of each node are stored in a queue, with the leftmost	*
 * child being at the front of the queue and the rightmost child being	*
 * at the rear of the queue.						*
 *									*
 * Each queue is stored as a pair of lists.  Pair (f,r) represents the	*
 * queue whose contents are the contents of list f followed by the	*
 * reversal of list r.  For example, pair ([1,2,3][5,4]) represents	*
 * the queue [1,2,3,4,5].  						*
 *									*
 * One key to the efficiency of the algorithm is not to let f become	*
 * shorter than r.  When an extraction is done from the queue that	*
 * makes f shorter than r, the queue representation is changed by	*
 * reversing r and appending it to the end of f, leaving r empty.	*
 *									*
 * Another key is to apend the reversal r is a lazy way, so that the	*
 * reversal will not be done until it is needed.  The append must 	*
 * also be lazy.							*
 *									*
 * For efficiency, appends are stored in two different forms.  A	*
 * TREE_TAG is a node as described above.  An APPEND_TAG has no		*
 * value at its root, but represents the append of its children.	*
 * When a queue is rebalanced by lazily reversing its reverse-part,	*
 * the lazy append is done by building an APPEND_TAG node.		*
 *									*
 * APPEND_TAG nodes have an important restriction.  They do not		*
 * allow their left subtrees to have any lazy parts in their list	*
 * structures (although they may have unevaluated MEMBERS in their      *
 * left subtrees).  That simplifies the algorithms for them.		*
 *									*
 * Here are descriptions of APPEND_TAG and TREE_TAG entities.		*
 *									*
 * APPEND_TAG								*
 *	This is the concatenation of two lists.  The value is a		*
 *	pointer to a triple of entities (l,r,n) where l is the		*
 *	left-hand list, r is the right-hand list, and n is the		*
 *	length of l.  If n = -1, then the length of l is unknown.	*
 *									*
 *	APPEND_TAGs are only used in special circumstances, to		*
 *	create a lazy append.  The left-subtree of an			*
 *	APPEND_TAG does not have any lazy parts in its list structure,  *
 *      but may have lazy members.					*
 *									*
 * TREE_TAG								*
 *	Operator ++ uses a list data structure due to Okasaki.  A	*
 *	list is represented as a multi-way branching tree, where	*
 *	the list consists of a preorder traversal of the tree.		*
 *	Each node of the tree contains a value, except nodes that	*
 *	are *vacant*.  Vacant nodes are skipped in the preorder		*
 *	traversal.							*
 *									*
 *	The children of a tree node are stored, left-to-right,		*
 *	as a queue.  The queue, in turn, is stored by four		*
 *	quantities: (f,m,r,n) where f and r are lists, m is		*
 *	the length of f and n is the length of r.  The queue		*
 *	contains f followed by the reversal of r.			*
 *									*
 *	A tree node therefore has five values associated with it:	*
 *	(key,f,m,r,n).  If key is NOTHING, then the node is		*
 *	vacant.  The implementation of Okasaki's algorithm is		*
 *	in file product.c						*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../evaluate/instruc.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../standard/stdids.h"
#include "../tables/tables.h"
#include "../show/prtent.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/********************************************************
 *			QUICK_APPEND			*
 ********************************************************
 * Append a and b using APPEND_TAG.  Note that a must   *
 * be fully evaluated in its list structure to do this, *
 * due to the restriction on use of APPEND_TAG.	        *
 ********************************************************/

ENTITY quick_append(ENTITY a, ENTITY b)
{
  ENTITY *h;
  int tag;

  /*---------------------------------------*
   * Check for special cases involving nil.*
   *---------------------------------------*/

  if(IS_NIL(a)) return b;
  if(IS_NIL(b)) return a;

  /*-------------------------------------------------*
   * Check for a special case that can be handled by *
   * building a pair node.			     *
   *-------------------------------------------------*/

  tag = TAG(a);
  if(TAG(a) == PAIR_TAG) {

    /*---------------------*
     * Check for [h] ++ b. *
     *---------------------*/

    h = ENTVAL(a);
    if(IS_NIL(h[1])) return ast_pair(h[0], b);
  }

  /*---------------*
   * General case. *
   *---------------*/

  h = allocate_entity(2);
  h[0] = a;
  h[1] = b;
  return ENTP(APPEND_TAG, h);
}


/********************************************************
 *			TREE_APPEND			*
 ********************************************************
 * Append a and b using Okasaki's algorithm.		*
 ********************************************************/

ENTITY tree_append(ENTITY a, ENTITY b)
{
  ENTITY *h;
  int tag;

  /*----------------------------------------*
   * Check for special cases involving nil. *
   *----------------------------------------*/

  if(IS_NIL(a)) return b;
  if(IS_NIL(b)) return a;

  /*----------------------------------------------------*
   * Check for special cases that can be done with a 	*
   * pair node or a quick-append node.			*
   *----------------------------------------------------*/

  tag = TAG(a);
  switch(tag) {

    /*----------------------------------*/
    case PAIR_TAG:

      /*---------------------*
       * Check for [h] ++ b. *
       *---------------------*/

      h = ENTVAL(a);
      if(IS_NIL(h[1])) return ast_pair(h[0], b);
      else goto general;  /* Default case, at end of switch. */

    /*----------------------------------*/
    case STRING_TAG:

      /*-------------------------------------------*
       * Check for a string of length 1. Otherwise *
       * use quick_append to do the job.           *
       *-------------------------------------------*/

      if(STRING_SIZE(BIVAL(a)) == 1) {
	return ast_pair(ENTCH(*STRING_BUFF(BIVAL(a))), b);
      }
      else return quick_append(a,b);

    /*----------------------------------*/
    case CSTR_TAG:

      /*--------------------------------------------*
       * Check for a string of length 1. Otherwise, *
       * use quick_append to do the job.            *
       *--------------------------------------------*/

      {char* a_buff = CSTRVAL(a);
       if(strlen(a_buff) == 1) {
	 return ast_pair(ENTCH(*a_buff), b);
       }
       else return quick_append(a,b);
     }

    /*----------------------------------*/
    case ARRAY_TAG:

      /*--------------------------------------------------------*
       * An array is unit depth and fully evaluated, provided	*
       * it has no follower.  So use quick_append. 		*
       *--------------------------------------------------------*/

      {ENTITY* ap    = ENTVAL(a);
       int     a1tag = TAG(ap[1]);
       if(a1tag == BOX_TAG || IS_NIL(ap[2])) {
         return quick_append(a, b);
       }
       else goto general;
      }

    /*----------------------------------*/
    case INDIRECT_TAG:
    case GLOBAL_INDIRECT_TAG:
      {ENTITY aa = *ENTVAL(a);
       if(TAG(aa) != TREE_TAG) {
	 goto general;  /* Default case, at end of switch */
       }
       a = aa;
      }
      /* No break: continue with TREE_TAG case. */

    /*----------------------------------*/
    case TREE_TAG:

      /*-------------------------------------------------------------*
       * Handle the case where a is a tree node.  In this case, just *
       * add b to the end of the child list of a. Since the child    *
       * list is represented as a queue, we must to an insertion     *
       * into the queue.					     *
       *-------------------------------------------------------------*/

      {ENTITY *p = ENTVAL(a);
       LONG   m  = IVAL(p[2]);  /* Length of front of queue */
       LONG   n  = IVAL(p[4]);  /* Length of rear of queue */
       ENTITY result, *rp;

       /*-----------------------------------------------------------*
        * We set rp to point to a block of memory to hold the tree  *
	* data, and result to be the new tree node.  If the root of *
	* a is vacant (p[0] = NOTHING), then an indirection is      *
	* needed, since such a node will change when evaluated.     *
        *-----------------------------------------------------------*/

       if(ENT_EQ(p[0], NOTHING)) {
	 rp     = allocate_entity(6);
	 rp[0]  = ENTP(TREE_TAG, rp+1);
	 result = ENTP(INDIRECT_TAG, rp);
	 rp++;
       }
       else {
	 rp     = allocate_entity(5);
	 result = ENTP(TREE_TAG, rp);
       }

       /*----------------------------------------------------------*
        * We need to keep the queue balanced.  If m > n, then just *
	* we can add b to the queue by adding it to the rear, and  *
	* still maintain the balance condition that m >= n.        *
        *----------------------------------------------------------*/

       if(m > n) {
	 memcpy(rp, p, 3*sizeof(ENTITY));
	 rp[4] = ENTI(n+1);		/* Size of new rear list */
	 rp[3] = ast_pair(b, p[3]);     /* New rear list. */
       }

       /*-------------------------------------------------------*
        * If m <= n, then adding to the rear will put too many 	*
	* members in the rear, destroying the balance.  We move *
	* the members of the rear list, along with b, to the 	*
	* front, lazily reversing the rear list, to bring the   *
	* queue back into balance. 				*
        *-------------------------------------------------------*/

       else {
         ENTITY xx;
	 rp[0] = p[0];			  /* Root label */
	 rp[2] = ENTI(m+n+1);             /* Size of front list */
	 rp[3] = nil;                     /* Rear list */
	 rp[4] = zero;                    /* Size of rear list */
	 xx    = ast_pair(b, p[3]);       /*  [b] ++ old rear list */
	 xx    = make_lazy_prim(REVERSE_TMO, xx, nil);
	 rp[1] = quick_append(p[1], xx);  /* front list: old front ++
						reversal of ([b] ++
						old rear). */
       }
       return result;
      }

     /*----------------------------------*/
     default:
     general:

       /*--------------------------------------------------------*
        * When the left-hand list (a) is not a tree, just create *
	* a new, vacant, tree node.  The children of this tree   *
	* node are a and b, so the child list is [a,b].  The     *
	* child list is stored as a queue with front part [a,b]  *
	* and empty rear.				         *
        *--------------------------------------------------------*/

       {ENTITY result;
	ENTITY* rp = allocate_entity(6);

	result = ENTP(INDIRECT_TAG, rp);
	rp[0] = ENTP(TREE_TAG, rp+1);
	rp[1] = NOTHING;                   /* Root label - vacant */
	rp[3] = ENTI(2);                   /* Size of front list */
	rp[4] = nil;                       /* Rear list */
	rp[5] = zero;                      /* Size of rear list */
	rp[2] = ast_triple(a, b, nil);     /* Front list */

	return result;
       }
  }
}


/*******************************************************************
 *			SPLIT_APPEND				   *
 *******************************************************************
 * Set *h to the head of l and *t to the tail of l followed by rr. *
 *								   *
 * l must be an append node, and must be evaluated up to the head. *
 *								   *
 * If mode = 1, might only set *h.  If mode = 2, might only set *t.*
 * If mode = 3, set both.					   *
 *******************************************************************/

void split_append(ENTITY l, ENTITY *h, ENTITY *t, ENTITY rr, int mode)
{
  ENTITY p = l;
  ENTITY r = rr;

  /*--------------------------------------------------------------------*
   * The tail is computed as follows.                                	*
   * 								     	*
   *                  +                +				*
   *                 / \              / \				*
   *                +   d            t   +				*
   *               / \                  / \				*
   *              +   c       ==>      b   +				*
   *             / \                      / \				*
   *            a   b                    c   d				*
   *									*
   * where t is the tail of a, and a is assumed not to be an append node.*
   * Notice that we respect the requirement that the left-subtree does  *
   * not contain unevaluated things, since t,b and c were all already in*
   * the left-subtree of an append.					*
   *									*
   * If a is a string of length >1, then instead of building an		*
   * append node at the top level, build an array node.			*
   *--------------------------------------------------------------------*/

  while(TAG(p) == APPEND_TAG) {
    register ENTITY* pp = ENTVAL(p);
    if(mode & 2) r = quick_append(pp[1], r);
    p = *pp;
  }

  /*---------------------------------------------------------*
   * Special case for leftmost argument a STRING_TAG entity: *
   * use the follower of the ARRAY_TAG to do the append.     *
   *---------------------------------------------------------*/

  if(mode & 2) {
    int p_tag = TAG(p);
    if(p_tag == STRING_TAG) {
      split_string(p, h, t, r, mode);
      return;
    }
    else if(p_tag == ARRAY_TAG) {
      split_array(p, h, t, r, mode);
      return;
    }
  }

  /*---------------*
   * General case. *
   *---------------*/

  ast_split1(p, h, t, mode);
  if(mode & 2) *t = quick_append(*t, r);

}


/*******************************************************************
 *			SPLIT_TREE				   *
 *******************************************************************
 * Set *h to the head of l and *t to the tail of l.		   *
 *								   *
 * l must be a TREE_TAG node, and must be evaluated up to the	   *
 * head, so the root must be occupied.				   *
 *								   *
 * If mode = 1, might only set *h.  If mode = 2, might only set *t.*
 * If mode = 3, set both.					   *
 *******************************************************************/

void split_tree(ENTITY l, ENTITY *h, ENTITY *t, int mode)
{
  ENTITY* ap = ENTVAL(l);

  /*--------------------------------------------------------*
   * Since split_tree can only be called on an evaled node, *
   * it must be the case that the root is not vacant.  Get  *
   * the head from the root and get the tail by making a    *
   * tree with a vacant root. 				    *
   *--------------------------------------------------------*/

  *h = *ap;
  if(mode & 2) {
    ENTITY *bp = allocate_entity(6);
    memcpy(bp+2, ap+1, sizeof(ENTITY) << 2);
    bp[1] = NOTHING;                    /* Root label */
    bp[0] = ENTP(TREE_TAG, bp+1);       /* This tree */
    *t = ENTP(INDIRECT_TAG, bp);        /* The tail is an indirection,
					   since it is lazy.  */
  }
}


/********************************************************
 *		GET_SINGLETON_QUEUE_MEMBER		*
 ********************************************************
 * One of lists f and r is empty, and the other is      *
 * singleton.  Return the sole member of the singleton  *
 * list.  It is possible that f is a lazy reversal.  If *
 * so, the reversal will be computed, to get the sole   *
 * member of f.						*
 *							*
 * XREF: This function is part of handling ++ by	*
 * Okasaki's algorithm.					*
 ********************************************************/

PRIVATE ENTITY get_singleton_queue_member(ENTITY f, ENTITY r)
{
  ENTITY v = (TAG(r) == NOREF_TAG) ? f : r;
  LONG time = LONG_MAX;
  REG_TYPE mark = reg1_param(&v);

  if(TAG(v) == INDIRECT_TAG)  v = eval1(v, &time);

  {register ENTITY result = ast_head(v);
   unreg(mark);
   return result;
  }
}


/***********************************************************
 *			REBALANCE_QUEUE			   *
 ***********************************************************
 * Rebalance the queue of TREE_TAG node p, if necessary.   *
 *							   *
 * The queue of a TREE_TAG node is represented by a pair   *
 * of lists, the front part and the rear part.  (The 	   *
 * queue contains the front part followed by the reversal  *
 * of the rear part.)  It is important to keep the queue   *
 * balanced, in the respect that the front part is at      *
 * least as long as the rear part.			   *
 *							   *
 * If the rear part is longer than the front part, then    *
 * lazily reverse the rear part, and lazily append it to   *
 * the end of the front part.			           *
 *							   *
 * XREF: This function is part of handling ++ by	   *
 * Okasaki's algorithm.					   *
 ***********************************************************/

PRIVATE void rebalance_queue(ENTITY *p)
{
  LONG m = IVAL(p[2]);	/* length of the front part */
  LONG n = IVAL(p[4]);  /* length of the rear part */
  if(n > m) {
    if(m + n > SMALL_INT_MAX) failure = LIMIT_EX;
    else {
      p[1] = quick_append(p[1], make_lazy_prim(REVERSE_TMO, p[3], nil));
      p[2] = ENTI(m + n);
      p[3] = nil;
      p[4] = zero;
    }
  }
}


/****************************************************************
 *			EVAL_TREE				*
 ****************************************************************
 * Evaluate tree node e, at location loc.			*
 *								*
 * If weak is true, then do a weak evaluation.  That involves	*
 * possibly using an APPEND_TAG node if a file is at the	*
 * front of the tree, to make the file visible.			*
 *								*
 * Return 							*
 *    1		if evaluation is finished or timed-out.         *
 *    0		if evaluation should continue, since the	*
 *		result might need further evaluation.		*
 *								*
 * XREF: This function is part of handling ++ by		*
 * Okasaki's algorithm.						*
 ****************************************************************/

Boolean eval_tree(ENTITY *loc, ENTITY e, LONG *time_bound, Boolean weak)
{
  REG_TYPE mark;
  REGPTR_TYPE ptrmark;
  ENTITY* ap = ENTVAL(e);

  /*------------------------------------------------------------------*
   **** Case 1. If the root of the tree is occupied, then there is ****
   **** no evaluation to do. 					   ****
   *------------------------------------------------------------------*/

  if(ENT_NE(ap[0], NOTHING)) return 1;

  mark    = reg1_param(&e);
  ptrmark = reg1_ptrparam(&loc);
  reg1_ptrparam(&ap);


  /*----------------------------------------------------------------------*
   **** Case 2. The root of this tree is vacant.  The children are     ****
   **** given by (f = ap[1], f_len = ap[2], r = ap[3], r_len = ap[4]), ****
   **** where                                                          ****
   ****								       ****
   ****   f and r are lists;                                           ****
   ****   The children are given, in left-to-right order, by list f    ****
   ****	    followed by the reversal of r;			       ****
   ****   f_len is the length of f;				       ****
   ****   r_len is the length of r.                                    ****
   ****								       ****
   **** Get the head of f (f_head) and the tail of f (f_tail).         ****
   *----------------------------------------------------------------------*/

  {ENTITY f_head, f_tail;
   LONG f_len = IVAL(ap[2]);
   LONG r_len = IVAL(ap[4]);
   reg2(&f_head, &f_tail);

   /*-------------------------------------------------------------------*
    * Get f_head and f_tail.  But be careful: f might be a		*
    * deferred reversal.  It might also be an APPEND_TAG structure      *
    * with a deferred reversal terminating the left-chain from          *
    * the root of f.  If so, do the reversal now.  Note that a deferred *
    * reversal is always accessed indirectly, so it will have tag       *
    * INDIRECT_TAG.							*
    *-------------------------------------------------------------------*/

   {ENTITY* ffloc = ap + 1;
    ENTITY  ff = *ffloc;
    REGPTR_TYPE ptrmark1 = reg1_ptrparam(&ffloc);
    while(TAG(ff) == APPEND_TAG) {
      ffloc = ENTVAL(ff);
      ff = *ffloc;
    }
    if(TAG(ff) == INDIRECT_TAG) {
     LONG time = LONG_MAX;
     *ffloc = eval1(ff, &time);
    }
    unregptr(ptrmark1);
   }
   ast_split(ap[1], &f_head, &f_tail);

   /*-----------------------------------------------------------------*
    * Evaluate the leftmost child, f_head. This could lead to a deep  *
    * recursion.  To prevent that, we artificially lower the time     *
    * bound.  The time is restored afterwards, and we try again if    *
    * necessary.                                                      *
    *-----------------------------------------------------------------*/

   while(failure < 0) {
     LONG init_low_time;
     LONG low_time = *time_bound;
     if(low_time > TREE_EVAL_TIME) low_time = TREE_EVAL_TIME;
     init_low_time = low_time;

     if(weak) {
       IN_PLACE_WEAK_EVAL(f_head, &low_time);
     }
     else {
       IN_PLACE_EVAL(f_head, &low_time);
     }
     *time_bound -= (init_low_time - low_time);

     if(failure < 0) break;
     else {
       if(failure == TIME_OUT_EX) {
	 if(*time_bound > 0) failure = -1;
       }
       else goto return0;
     }
   } /* end while(failure < 0) */

   /*------------------------------------------------------------------*
    **** Case 2.1. If the leftmost child is a tree, then rebalance  ****
    **** the tree. This is even done if there was a time-out above, ****
    **** to bring the timed-out computation to the root for the     ****
    **** time.							    ****
    ****							    ****
    **** The idea is that the original tree e looks like this:      ****
    ****							    ****
    ****		 -					    ****
    ****	       /   \                                        ****
    **** 	      / ... \                                       ****
    **** 	     x   (...f_tail/r)                              ****
    **** 	    / \                                             ****
    **** 	   /...\                                            ****
    **** 	 (..bf/br)                                          ****
    ****                                                            ****
    **** where - indicates a vacant node, (...f_tail/r) indicates   ****
    **** a sequence of children given by forward list f_tail and    ****
    **** reverse list r, and bf/br is the queue pointed to by bp,   ****
    **** and where x might be NOTHING, indicating a vacant node.    ****
    **** (x can be NOTHING only if evaluation of f_head timed out.) ****
    **** The restructured tree looks like this                      ****
    ****                                                            ****
    ****		      x    				    ****
    ****		   /    \                                   ****
    ****		  /      \                                  ****
    ****		 /  ...   \                                 ****
    ****	    (...bf/br)     t                                ****
    ****                                                            ****
    **** where t is the sole member of f_tail/r if f_tail/r is      ****
    **** singleton, and is tree                                     ****
    ****							    ****
    ****		   -                                        ****
    ****		 /...\                                      ****
    ****	       (...f_tail/r)                                ****
    ****                                                            ****
    **** if f_tail/r is not singleton.                              ****
    *------------------------------------------------------------------*/

   if(TAG(f_head) == TREE_TAG) {
     ENTITY* bp = ENTVAL(f_head);
     LONG br_len = IVAL(bp[4]);
     ENTITY t;
     reg1(&t);
     reg1_ptrparam(&bp);

     /*-------------*
      * Get tree t. *
      *-------------*/

     if(f_len + r_len == 2) {
       t = get_singleton_queue_member(f_tail, ap[3]);
     }
     else {
       /*----------------------------------------------*
        * f_tail cannot be nil since f_len + r_len > 2 *
        * and f_len >= r_len due to balance condition. *
        *----------------------------------------------*/

       ENTITY* p = allocate_entity(6);
       t    = ENTP(INDIRECT_TAG, p);
       p[0] = ENTP(TREE_TAG, p+1);
       p[1] = NOTHING;
       p[2] = f_tail;      
       p[3] = ENTI(f_len - 1);
       p[4] = ap[3];
       p[5] = ap[4];
       rebalance_queue(p+1);
     }

     /*------------------------------*
      * Update the root of e (at ap) *
      *------------------------------*/

     if(br_len + 1 > SMALL_INT_MAX) failure = LIMIT_EX;
     else {
       ap[0] = bp[0];
       ap[1] = bp[1];
       ap[2] = bp[2];
       ap[3] = ast_pair(t, bp[3]);
       ap[4] = ENTI(br_len + 1);
       rebalance_queue(ap);
     }

     /*----------------------------------------------------------------*
      * The result is still the tree at ap, since we have altered the  *
      * memory to which ap points.  So e is unchanged. If bp[0] (x     *
      * in the comment above) is not NOTHING, then evaluation is done. *
      * If bp[0] is NOTHING, then evaluation of f_head timed-out.      *
      * Either way, we return 1.                                       *
      *----------------------------------------------------------------*/

     goto return1;

   } /* end if(TAG(f_head) == TREE_TAG) */

   /*------------------------------------------------------------------*
    **** Case 2.2. If evaluation of f_head timed out, and	    ****
    **** f_head is not a tree, then just leave things as they are   ****
    **** and return 1. 						    ****
    *------------------------------------------------------------------*/

   if(failure >= 0) goto return1;

   /*------------------------------------------------------------------*
    **** Case 2.3. If the leftmost child is nil, then just	    ****
    **** delete it and return 0 to call for more evaluation.        ****
    *------------------------------------------------------------------*/

   if(TAG(f_head) == NOREF_TAG) {

     /*---------------------------------------------------------*
      * If there becomes only one child, then that child is the *
      * whole tree. 						*
      *---------------------------------------------------------*/

     if(f_len + r_len == 2) {
       *loc = get_singleton_queue_member(f_tail, ap[3]);
     }

     /*-----------------------------------------------------------*
      * If there is more than one child, then just pop the queue. *
      *-----------------------------------------------------------*/

     else {
       /*-----------------------------------------------*
        * f_tail cannot be nil since f_len + r_len > 2  *
	* and f_len >= r_len.                           *
        *-----------------------------------------------*/

       ap[1] = f_tail;    
       ap[2] = ENTI(f_len - 1);
       rebalance_queue(ap);
     }
     goto return0;
   } /* end if(TAG(f_head) == NOREF_TAG) */

   /*------------------------------------------------------------------*
    **** Case 2.4. If f_head is a file, and weak is true, then      ****
    **** move the file to an APPEND_TAG node to make it visible.    ****
    *------------------------------------------------------------------*/

   if(weak && TAG(f_head) == FILE_TAG) {
     if(f_len + r_len == 2) {
       e = get_singleton_queue_member(f_tail, ap[3]);
     }
     else {
       ap[1] = f_tail;
       ap[2] = ENTI(f_len - 1);
       rebalance_queue(ap);
     }
     *loc = quick_append(f_head, e);
     goto return1;
   }

   /*-----------------------------------------------------------------*
    **** Case 2.5. If f_head is not a tree, and if there was no    ****
    **** timeout, then move the head of f_head to the root of      ****
    **** the tree and move the tail of f_head to where f was.      ****
    **** There is a special case for f_head being singleton        ****
    **** and there only one other tree: then, just build a pair    ****
    **** instead.  						   ****
    *-----------------------------------------------------------------*/

   {ENTITY tl;
    reg1(&tl);
    ast_split(f_head, ap, &tl);

    /*----------------------------------------------------------------*
     **** Case 2.5.1. If f_head is a singleton list, build a pair. ****
     *----------------------------------------------------------------*/

    if(TAG(tl) == NOREF_TAG) {
      if(f_len + r_len == 2) {

	/*-----------------------------------------*
	 * The tree is                             *
	 *                                         *
	 *	   -         or         -          *
	 *        / \                  / \         *
	 *	 /...\                /...\        *
	 *     [[h]]/[a]          [[h],a]/[]       *
	 *                                         *
	 *  Change it to [h & a].                  *
	 *-----------------------------------------*/

	tl = get_singleton_queue_member(f_tail, ap[3]);
	*loc = ast_pair(*ap, tl);
      }
      else {

	/*----------------------------------*
	 * The tree is                      *
	 *                                  *
	 *	       -                    *
	 *            / \                   *
	 *	     /...\                  *
	 *     [[h] & f_tail] / r           *
	 *                                  *
	 *   Change it to                   *
	 *                                  *
	 *	   h 		            *
	 *        / \           	    *
	 *	 /...\                      *
	 *     f_tail / r                   *
	 *----------------------------------*/

	ap[1] = f_tail;
	ap[2] = ENTI(f_len - 1);
	rebalance_queue(ap);
      }
    } /* end if(TAG(tl) == NOREF_TAG) */

    /*------------------------------------------------------------------*
     **** Case 2.5.2. If f_head is not a singleton list, then build  ****
     **** build a new tree                                           ****
     *------------------------------------------------------------------*/

    else { /* TAG(tl) != NOREF_TAG */

      /*--------------------------------------------*
       * The tree is                         	    *
       *		       -                    *
       *                      / \                   *
       *		     /...\                  *
       *	      [[h & tl] & f_tail] / r       *
       *                                            *
       *	 Change it to                       *
       *                                            *
       *		       h                    *
       *                      / \                   *
       *		     /...\                  *
       *		[[tl] & f_tail] / r         *
       *--------------------------------------------*/

      ap[1] = ast_pair(tl, f_tail);
    }

    goto return1;
   }
  }

return0:
  unreg(mark);
  unregptr(ptrmark);
  return 0;

return1:
  unreg(mark);
  unregptr(ptrmark);
  return 1;
}


