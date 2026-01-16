/*********************************************************************
 * File:    rts/product.c
 * Purpose: Implement functions for cartesian products and lists
 * Author:  Karl Abrahamson
 *********************************************************************/

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
 * This file provides functions for cartesian products and lists.	*
 * Both are stored with the same internal representation.  Multiple	*
 * representations are used.  The representations are as follows.	*
 *									*
 *  NOREF_TAG	A list with tag NOREF_TAG is nil.  There is only one	*
 *		representation of nil.					*
 *									*
 *  PAIR_TAG	A PAIR_TAG entity points to a pair of entities, which	*
 *		are the head and tail of a list, or are the left and	*
 *		right parts of a pair.					*
 *									*
 *  TRIPLE_TAG	A TRIPLE_TAG entity points to a triple of entities.  	*
 *		As a list, triple (A,B,C) represents the list [A,B & C] *
 *		consisting of A, then B, followed by suffix C.		*
 *		As a product, triple (A,B,C) is just (A,(B,C)).		*
 *									*
 *  QUAD_TAG	A QUAD_TAG entity points to a quadruple of entities.	*
 *		As a list, quad (A,B,C,D) represents the list		*
 *		[A,B,C & D].  As a product, it represents (A,(B,(C,D))) *
 *									*
 *  STRING_TAG	A STRING_TAG entity points to a binary chunk whose	*
 *		contents is the value of a string.			*
 *									*
 *  CSTR_TAG	A CSTR_TAG entity points to a null-terminated string.	*
 *              The string pointed to must be in the string hash	*
 *		table.							*
 *									*
 *  FILE_TAG	A FILE_TAG entity represents an unread file (a list	*
 *		of characters).  Using it entails reading part of the	*
 *		file.							*
 *									*
 *  APPEND_TAG	An APPEND_TAG entity has two children.  It represents	*
 *		the left child followed by the right child.  The	*
 *		left child is not allowed to have any unevaluated	*
 *		parts in its list structure, but can have unevaluated   *
 *		members.						*
 *									*
 *  TREE_TAG	A TREE_TAG entity stores a queue of children, and	*
 *		also possibly a value at the root.  It represents the	*
 *		list consisting of the value at the root (if there is	*
 *		one) followed by the concatenation of the children, in	*
 *		the order in which they occur in the queue.  These 	*
 *		trees are managed by Okasaki's algorithm.		*
 *									*
 *		Much of the administration of TREE_TAG entities is done	*
 *		when evaluation is called for.  It can be found in	*
 *		evalsup.c.						*
 *									*
 *  ARRAY_TAG	An ARRAY_TAG entity points to a header that describes	*
 *		an array.  See machdata/entity.doc for details.		*
 *									*
 * This file provides some basic operations.  Other files provide	*
 * more support.							*
 *									*
 *  append.c	Support for append, and for evaluating and taking	*
 *		apart entities made from APPEND_TAG and TREE_TAG tags.	*
 *									*
 *  array.c	Support for arrays and packed strings.  Here, ARRAY_TAG *
 *		entities are handled.					*
 ************************************************************************/


#include <string.h>
#include "../misc/misc.h"
#ifdef UNIX
# include <sys/types.h>
# include <sys/unistd.h>
# include <sys/fcntl.h>
#endif
#ifdef MSWIN
# include <io.h>
# include <fcntl.h>
#endif
#include <sys/stat.h>
#include "../utils/lists.h"
#include "../utils/filename.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../show/prtent.h"
#include "../tables/tables.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
#ifdef GCTEST
# include "../machdata/gc.h"
#endif


/***************************************************************
 * Some list functions need to keep track of how long they run *
 * so that they can time out if they run too long.  To let     *
 * these functions run reasonably long, the time counter is    *
 * not decremented at each iteration of those functions.       *
 * TIME_STEP_COUNT_INIT is the number of iterations done per   *
 * decrement of the time counter for list functions.           *
 ***************************************************************/

#define TIME_STEP_COUNT_INIT 10


/****************************************************************
 *			LAZY_LEFT_STDF, 			*
 *			LAZY_RIGHT_STDF,                        *
 *			LAZY_HEAD_STDF,                         *
 *			LAZY_TAIL_STDF				*
 ****************************************************************
 * These implement lazyLeft, etc, by creating lazy primitives.  *
 ****************************************************************/

ENTITY lazy_left_stdf(ENTITY x)
{return make_lazy_prim(LAZY_LEFT_TMO, x, nil);}

ENTITY lazy_right_stdf(ENTITY x)
{return make_lazy_prim(LAZY_RIGHT_TMO, x, nil);}

ENTITY lazy_head_stdf(ENTITY x)
{return make_lazy_prim(LAZY_HEAD_TMO, x, nil);}

ENTITY lazy_tail_stdf(ENTITY x)
{return make_lazy_prim(LAZY_TAIL_TMO, x, nil);}


/****************************************************************
 *			AST_SPLIT1				*
 ****************************************************************
 * This function sets *h to the head of ll and *t to the tail	*
 * of ll, except that the action is controlled by mode:         *
 *                                                              *
 *   mode	set                                             *
 *   ----	----                                            *
 *    1         *h only                                         *
 *    2         *t only                                         *
 *    3		*h and *t                                       *
 *                                                              *
 * This function MIGHT set *t when mode = 1 and *h when         *
 * mode = 2, but is not required to do so.			*
 *								*
 * Requires ll to be evaluated already, at least up to the head,*
 * unless it is an infile.  An infile can be unevaluated.       *
 ****************************************************************/

void ast_split1(ENTITY ll, ENTITY *h, ENTITY *t, int mode)
{
  ENTITY l;
  ENTITY *loc = NULL;

  l    = ll;

 top:         /* For tail recursion */
  switch(TAG(l)) {
      case NOREF_TAG:

	/*------------------*
	 * Can't split nil. *
	 *------------------*/

	failure = EMPTY_LIST_EX;
	*h = *t = zero;
	break;

      case INDIRECT_TAG:
      case GLOBAL_INDIRECT_TAG:

	/*--------------------------------------------------------*
	 * Tail recur on the value pointed to by the indirection. *
	 *--------------------------------------------------------*/

	loc = ENTVAL(l);
	l   = *loc;
	goto top;

      case FILE_TAG:

	/*------------------------------------------------------------*
	 * A file needs to be evaluated to split it.  Evaluating will *
	 * cause input to be done.  Note that this file must be an    *
	 * infile, since we cannot split an outfile.		      *
	 *------------------------------------------------------------*/

	{LONG l_time = LONG_MAX;
	 l = eval((loc == NULL) ? l : ENTP(INDIRECT_TAG, loc), &l_time);

	 /*---------------------------------------------*
	  * If evaluation failed, check for a time-out. *
	  *---------------------------------------------*/

	 if(failure >= 0) {
	   if(failure == TIME_OUT_EX) {

	     /*----------------------------------------------------*
	      * Build a lazy head and lazy tail of the file.  The  *
	      * assignment to l in the first line is needed to put *
	      * the result in a registered variable while the      *
	      * second line runs.				   *
	      *----------------------------------------------------*/

	     if(mode & 1) *h = l = make_lazy_prim(LAZY_HEAD_TMO, l, nil);
	     if(mode & 2) *t = make_lazy_prim(LAZY_TAIL_TMO, l, nil);
	   }
	   break;
	 }

	 /*------------------------------------------------------*
	  * If evaluation of the file succeeded, just tail recur *
	  * on the result of evaluation.                         *
	  *------------------------------------------------------*/

	 goto top;
	}

      case STRING_TAG:
	split_string(l, h, t, nil, mode);
	break;

      case CSTR_TAG:
        split_cstring(l, h, t, mode);
        break;

      case APPEND_TAG:
	split_append(l, h, t, nil, mode);
	break;

      case TREE_TAG:
	split_tree(l, h, t, mode);
	break;

      case ARRAY_TAG:
	split_array(l, h, t, nil, mode);
	break;

      case PAIR_TAG:
	{register ENTITY *p;
	 p  = ENTVAL(l);
	 *t = p[1];
	 *h = p[0];
	 break;
	}

      case TRIPLE_TAG:
      case QUAD_TAG:
	{*t = BLOCK_TAIL(l);
	 *h = HEAD(l);
	 break;
	}

      default:
	die(105, toint(TAG(l)));
  }
}


/*****************************************************************
 *			AST_HEAD				 *
 *****************************************************************
 * Return the head or left-part of ll.				 *
 * Requires ll to be evaluated already, at least up to the head. *
 * 								 *
 * NOTE: ast_head is inlined in evaluate.c.			 *
 *****************************************************************/

ENTITY ast_head(ENTITY ll)
{
  ENTITY h, t;

  ast_split1(ll, &h, &t, 1);
  return h;
}


/*****************************************************************
 *			AST_TAIL				 *
 *****************************************************************
 * Return the tail or right-part of ll.				 *
 * Requires ll to be evaluated already, at least up to the head. *
 * 								 *
 * NOTE: ast_tail is inlined in evaluate.c.			 *
 *****************************************************************/

ENTITY ast_tail(ENTITY ll)
{
  ENTITY h, t;

  ast_split1(ll, &h, &t, 2);
  return t;
}


/****************************************************************
 *			AST_PAIR				*
 ****************************************************************
 * Build a pair (h,t).						*
 ****************************************************************/

ENTITY ast_pair(ENTITY h, ENTITY t)
{
  register ENTITY *p;

  p 		= allocate_entity(2);
  p[0]          = h;
  p[1]          = t;
  return ENTP(PAIR_TAG, p);
}


/****************************************************************
 *			AST_TRIPLE				*
 ****************************************************************
 * Build a triple (a,b,c).					*
 ****************************************************************/

ENTITY ast_triple(ENTITY a, ENTITY b, ENTITY c)
{
  register ENTITY *p;

  p 		= allocate_entity(3);
  p[0]          = a;
  p[1]          = b;
  p[2]		= c;
  return ENTP(TRIPLE_TAG, p);
}


/****************************************************************
 *			AST_QUAD				*
 ****************************************************************
 * Build a quadruple (a,b,c,d).					*
 ****************************************************************/

ENTITY ast_quad(ENTITY a, ENTITY b, ENTITY c, ENTITY d)
{
  register ENTITY *p;

  p 	= allocate_entity(4);
  p[0]  = a;
  p[1]  = b;
  p[2]	= c;
  p[3]  = d;
  return ENTP(QUAD_TAG, p);
}


/****************************************************************
 *			MULTI_PAIR_PR				*
 ****************************************************************
 * Perform k PAIR_I instructions on the_stack.			*
 ****************************************************************/

void multi_pair_pr(int k)
{
  ENTITY *p, *r, e;
  int i, m;

  /*-----------------------------------------------------------*
   * Rather than just building pairs, we try to build slightly *
   * larger nodes, since they require less space for pointers. *
   *-----------------------------------------------------------*/

  e = POP_STACK();
  while(k > 0) {

    /*-------------------------------------------------------*
     * m is the number of entities to put in the next block. *
     *-------------------------------------------------------*/

    m = min(k + 1, MAX_ENTS_IN_PAIR_BLOCK);

    /*--------------------------------------------------------------*
     * Build the block, and install e at the end.  Note that PAIR_I *
     * instructions build structures from back to front, so that is *
     * what is done here.					    *
     *--------------------------------------------------------------*/

    r = allocate_entity(m);
    p = r + (m - 1);
    *(p--) = e;

    /*--------------------------------------------------------------*
     * Copy the remaining m-1 members of this block into the block. *
     *--------------------------------------------------------------*/

    for(i = m-2; i >= 0; i--) {
      *(p--) = POP_STACK();
    }

    /*----------------------------------------------------------------*
     * Set e to this block.  If we are done, then e will be returned. *
     * If not, then e will be installed as the last member of the     *
     * previous block.						      *
     *----------------------------------------------------------------*/

    e = ENTP(m + (PAIR_TAG - 2), r);
    k -= m-1;
  }

  *PUSH_STACK() = e;

}


/******************************************************************
 *			AST_LENGTH				  *
 ******************************************************************
 * ast_length(ll,offset,time_bound) returns the length of ll plus *
 * offset as an entity, but might time out if *time_bound is too  *
 * small.  ast_length is called by redo_lazy_prim.                *
 *								  *
 * ast_length_stdf(l) computes the length with a tightly limited  *
 * time.							  *
 ******************************************************************/

ENTITY ast_length_stdf(ENTITY l)
{
  LONG l_time = LIST_PRIM_TIME;
  return ast_length(l, 0, &l_time);
}

/*--------------------------------------------------------------*/

ENTITY ast_length(ENTITY ll, LONG offset, LONG *time_bound)
{
  ENTITY result, l;
  ENTITY* loc = NULL;
  REG_TYPE mark;
  REGPTR_TYPE ptrmark = reg1_ptr(&loc);
  int time_step_count = TIME_STEP_COUNT_INIT;

  l = ll;
  mark = reg1_param(&l);

  for(;;) {

    /*-----------------------------------------------------------*
     * Get the head and tail accessible.  But don't do anything  *
     * to files, since we might use the file system to get their *
     * lengths. 						 *
     *-----------------------------------------------------------*/

    IN_PLACE_WEAK_EVAL_FAILTO(l, time_bound, fail);
    goto got_l; /* Just below, after fail code. */

    /*------------------------------------------*
     * At a time-out, rebuild as a lazy entity. *
     *------------------------------------------*/

  fail:
    result = (failure == TIME_OUT_EX)
		? make_lazy_prim(LENGTH_TMO, l, ast_make_int(offset))
		: zero;
    goto out;

  got_l:

    switch(TAG(l)) {
      case NOREF_TAG:

	/*-----------------------------------------------------------*
	 * An empty list has length 0.  Since we are supposed to add *
	 * offset to the length, the desired result is offset.       *
	 *-----------------------------------------------------------*/

	result = ast_make_int(offset);
	goto out;

      case INDIRECT_TAG:
      case GLOBAL_INDIRECT_TAG:
	loc = ENTVAL(l);
	l   = *loc;
	break;

      case STRING_TAG:
	result = ast_make_int(STRING_SIZE(BIVAL(l)) + offset);
	goto out;

      case CSTR_TAG:
        result = ast_make_int(strlen(CSTRVAL(l)) + offset);
        goto out;

      case APPEND_TAG:

	/*-------------------------------------------------------*
	 * To handle an append node, add the length of the left  *
	 * child to the offset, and tail recur on the right      *
	 * child.  But be careful to check for integer overflow. *
	 *-------------------------------------------------------*/

	{ENTITY left_length = ast_length(*ENTVAL(l), 0, time_bound);
	 offset += get_ival(left_length, LIMIT_EX);
	 if(offset < 0) { /* overflow check */
	   failure = LIMIT_EX;
	 }
	 if(failure >= 0) {
	   result  = zero;
	   goto out;
	 }
	 l = ENTVAL(l)[1];
	 break;
	}

      case ARRAY_TAG:
        l = array_length(l, &offset);
        MAYBE_TIME_STEP(time_bound);
	if(failure >= 0) goto fail;
        break;

      case PAIR_TAG:
      case TRIPLE_TAG:
      case QUAD_TAG:
	{register int n;
	 register ENTITY *ap;

	 /*------------------------------------------------------*
	  * To get the length, add one less than the number of   *
	  * parts that are stored here to offset, and tail recur *
	  * on the last part.					 *
	  *------------------------------------------------------*/

	 n       = TAG(l) - (PAIR_TAG - 1);
	 offset += n;
	 ap      = ENTVAL(l);
	 l       = ap[n];
	 MAYBE_TIME_STEP(time_bound);
	 if(failure >= 0) goto fail;
	 break;
       }

      case TREE_TAG:

	 /*---------------------------------------------------*
	  * To get the length of a list represented by a tree *
	  * node, get the head and tail, add 1 to the offset, *
	  * and recur on the tail. 			      *
	  *---------------------------------------------------*/

	 {ENTITY h;
	  ast_split1(l, &h, &l, 2);
	  offset++;
          MAYBE_TIME_STEP(time_bound);
	  if(failure >= 0) goto fail;
	  break;
	 }

      case FILE_TAG:
	{struct file_entity* fent = FILEENT_VAL(l);
	 struct stat stat_buf;

	 /*-------------------------------------------------------*
	  * Handle volatile files as ordinary lists.  Also handle *
	  * nonvolatile text files as ordinary lists in the 	  *
	  * DOS/Windows file system, since a newline is 	  *
	  * represented by two characters, making file lengths	  *
	  * unreliable. 					  *
	  *-------------------------------------------------------*/

	 if(fent->kind != INFILE_FK || IS_VOLATILE_OR_DOSTEXT_FM(fent->mode)) {
	   l = eval((loc == NULL) ? l : ENTP(INDIRECT_TAG, loc), time_bound);
	   loc = NULL;
	   break;      /* Tail recur on evaled entity. */
	 }

	 /*------------------------------------------*
	  * For a file, do a stat to get the length. *
	  *------------------------------------------*/

	 stat_file(fent->u.file_data.name, &stat_buf);
	 result = ast_make_int(stat_buf.st_size - fent->u.file_data.pos
			       + offset);
	 goto out;
	}

     default:
	die(108, toint(TAG(l)));
    }
  }

 out:
  unreg(mark);
  unregptr(ptrmark);
  return result;
}


/****************************************************************
 *			AST_SUBSCRIPT1				*
 ****************************************************************
 * ast_subscript1(l,nn,time) returns      			*
 * l # nn, possibly timing out.                               	*
 ****************************************************************/

ENTITY ast_subscript1(ENTITY l, ENTITY nn, LONG *l_time)
{
  LONG n = get_ival(nn, LIMIT_EX);
  if(failure >= 0) return nil;
  else {
    return ast_sublist1(l, n, 1, l_time, TRUE, TRUE);
  }
}


/****************************************************************
 *			INDEX_VAL				*
 ****************************************************************
 * Return							*
 *   0   	if x << 0					*
 *   LONG_MAX	if x >> 0					*
 *   x          otherwise.					*
 *								*
 * where x << 0 means x is a negative number with tag 		*
 * BIGNEGINT_TAG, and x >> 0 means x is a positive number with  *
 * tag BIGPOSINT_TAG.						*
 ****************************************************************/

LONG index_val(ENTITY x)
{
  int tag = TAG(x);
  while(tag == INDIRECT_TAG || tag == GLOBAL_INDIRECT_TAG) {
    x   = *ENTVAL(x);
    tag = TAG(x);
  }
  return (tag == BIGNEGINT_TAG) ? 0 :
	 (tag == BIGPOSINT_TAG) ? LONG_MAX : IVAL(x);
}


/****************************************************************
 *			AST_SUBLIST, AST_SUBLIST1		*
 ****************************************************************
 * If subscript is FALSE, then ast_sublist1 returns the 	*
 * sublist of ll starting at index i, of length at most n.	*
 *								*
 * If subscript is TRUE, then ast_sublist1 returns ll#i. In	*
 * this case, n should be 1.					*
 *								*
 * Each of i and n must be fully evaluated.  			*
 *								*
 * If force is true, then begin evaluation even if ll is lazy.	*
 * If force is false, it is possible to return a lazy value	*
 * without doing any calculation when ll is lazy.		*
 *								*
 * Ast_sublist returns the sublist of ll starting at left(ij), 	*
 * of length right(ij). Object ij must be fully evaluated.	*
 ****************************************************************/

#ifdef DEBUG
PRIVATE int sublist_ident = 0;
#endif

ENTITY ast_sublist1(ENTITY ll, LONG i, LONG n,
		    LONG *time_bound, Boolean force, Boolean subscript)
{
  int tag;
  REG_TYPE mark;
  REGPTR_TYPE ptrmark;
  ENTITY result, l;
  ENTITY* loc = NULL;
  int time_step_count = TIME_STEP_COUNT_INIT;
# ifdef DEBUG
    int myid = 0;
# endif

# ifdef DEBUG
    if(trace_extra) {
      myid = sublist_ident++;
      trace_i(242, myid, i, n, subscript);
      long_print_entity(ll, 1, 0);
      tracenl();
    }
# endif

  mark    = reg2(&result, &l);
  ptrmark = reg1_ptr(&loc);

  /*--------------------------------------------------------------*
   * The default result is ll itself.  Variable l is used to scan *
   * the list.							  *
   *--------------------------------------------------------------*/

  result = l = ll;

  /*--------------------------------------------------------------*
   * Adjust for i negative or very large.  For taking a sublist,  *
   * if i < 0, then force to 0.  If i is very large, then we	  *
   * presume that the sublist is nil.				  *
   *								  *
   * For subscripting, i < 0 or i very large yield subscript out  *
   * of bounds.							  *
   *--------------------------------------------------------------*/

  if(i < 0) {
    if(subscript) goto do_nil;   /* Below: under switch: fails */
    else i = 0;
  }
  if(i == LONG_MAX) goto do_nil;  /* Below: returns nil or fails. */

  for(;;) {  /* for tail recursion simulation */

    /*-------------------------------------------------------*
     * If a failure has occurred, then check for a time-out. *
     * At a time-out, build a lazy sublist entity that will  *
     * compute the desired result, l##(i,n) or l#i.	     *
     *-------------------------------------------------------*/

    if(failure >= 0) {
   fail:
      if(failure == TIME_OUT_EX) {
   timeout:
	{ENTITY nn = (n == LONG_MAX) ? a_large_int : ENTI(n);
	 result = ast_triple(ENTI(subscript), ENTI(i), nn);
	 result = make_lazy_prim(SUBLIST_TMO, l, result);
	}
      }
      goto out;
    }

    /*------------------------------------------------------------*
     * If n <= 0, then we are doing a sublist (not a subscript)	  *
     * and we are asked to take nothing.  Return nil. 		  *
     *------------------------------------------------------------*/

    if(n <= 0) {result = nil; goto out;}

    /*-----------------------------------------------------*
     * If i = 0 and n is large, then we must be doing a    *
     * sublist (not a subscript) and the result must be l. *
     *-----------------------------------------------------*/

    if(n == LONG_MAX && i == 0) {result = l; goto out;}

#   ifdef DEBUG
      if(trace_extra) {
	trace_i(243, myid, i, n, force, *time_bound, failure, TAG(l));
	trace_print_entity(l);
	tracenl();
	if(TAG(l) == INDIRECT_TAG) {
	  trace_i(244, TAG(*ENTVAL(l)));
	}
      }
#   endif

    /*-------------------------------------------------------*
     * Be lazy on lazy values, unless forced not to be lazy. *
     *-------------------------------------------------------*/

    if(!force && is_lazy(l)) goto timeout;

    /*----------------------------------------------------------*
     * Get the head and tail accessible, but don't do anything  *
     * to files, since we will use a seek on them. 		*
     *----------------------------------------------------------*/

    IN_PLACE_WEAK_EVAL_FAILTO(l, time_bound, fail);

#   ifdef DEBUG
      if(trace_extra) {
	trace_i(245, myid, TAG(l));
	trace_print_entity(l);
	tracenl();
      }
#   endif

    /*---------------------------*
     * Process according to tag. *
     *---------------------------*/

    tag = TAG(l);
    switch(tag) {
      case NOREF_TAG: /* nil */
      do_nil:

	/*--------------------------------------*
	 * A sublist of nil is always nil. 	*
	 * Subscripting into nil always fails.  *
	 *--------------------------------------*/

	if(!subscript) result = nil;
	else failure = SUBSCRIPT_EX;
	goto out;

      case INDIRECT_TAG:
      case GLOBAL_INDIRECT_TAG:

	/*---------------------------------------------------------*
	 * Set loc, and tail recur on what the INDIRECT_TAG entity *
	 * points to.						   *
	 *---------------------------------------------------------*/

	loc = ENTVAL(l);
	l   = *loc;
	break;

      case STRING_TAG:

	/*------------------------------------------------------*
	 * sp points to the chunk that holds the string, and k  *
	 * is the length of the string.				*
	 *------------------------------------------------------*/

	{CHUNKPTR sp = BIVAL(l);
	 LONG     k  = STRING_SIZE(sp);
	 if(i >= k) goto do_nil; /* above (NOREF_TAG) */
	 else {
	   if(subscript) result = ENTCH(STRING_BUFF(sp)[i]);
	   else {

	     /*--------------------------------------------------*
	      * To take a sublist of a string, we build an array *
	      * header that describes how much is taken.	 *
	      *--------------------------------------------------*/

	     ENTITY* ap = allocate_entity(4);
	     ap[0] = ENTI(min(k-i,n));
	     ap[1] = l;
	     ap[2] = nil;
	     ap[3] = ENTI(i);
	     result = ENTP(ARRAY_TAG, ap);
	   }
	   goto out;
	 }
	}

      case CSTR_TAG:

        /*----------------------------------------------------------*
         * To take a sublist of a constant string, just convert the *
         * substring to STRING_TAG form.  To subscript into it, just*
         * index into its buffer.				    *
         *----------------------------------------------------------*/

        {charptr s = CSTRVAL(l);
         LONG slen = strlen(s);
         if(i >= slen) goto do_nil;
	 if(subscript) result = ENTCH(s[i]);
         else {
           LONG len = slen - i;
           if(len > n) len = n;
           result = make_strn(s + i, len);
         }
         goto out;
        }

      case ARRAY_TAG:
	result = array_sublist(l, i, n, time_bound, subscript);
	goto out;

      /*---------------------------------------------------------*
       * TREE_TAG, PAIR_TAG, TRIPLE_TAG and QUAD_TAG are handled *
       * similarly, but we take the tail in special ways for	 *
       * efficiency.  Just take the tail of l, decrement i, and  *
       * tail recur.  The case of i = 0 is discussed below.	 *
       *---------------------------------------------------------*/

      case APPEND_TAG:
      case TREE_TAG:
	if(i > 0) {
	  l = ast_tail(l);
	  goto is_not_zero;
	}
	else goto iszero;

      case PAIR_TAG:
	if(i > 0) {
	  l = PAIR_TAIL(l);
	  goto is_not_zero;
	}
	else goto iszero;

      case TRIPLE_TAG:
      case QUAD_TAG:
	if(i > 0) {
	  l = BLOCK_TAIL(l);
      is_not_zero:
	  loc = NULL;
	  i--;
	  MAYBE_TIME_STEP(time_bound);
	  break;  /* tail recur to top of loop. */
	}
	else {
      iszero:

	   /*----------------------------------------------------*
	    * We get here when i = 0.  If we are taking a suffix *
	    * (n = LONG_MAX) then just return l.  Otherwise,     *
	    * get the head and tail of l, build a lazy sublist   *
	    * for the tail, and build the sublist of l.		 *
	    * 							 *
	    * If we are doing a subscript, just get the head of	 *
	    * l.						 *
	    *----------------------------------------------------*/

	   {ENTITY tl, hd;
	    if(n == LONG_MAX) result = l;
	    else if(subscript) result = ast_head(l);
	    else {
	      reg2(&hd, &tl);
	      ast_split(l, &hd, &tl);
	      tl = ast_sublist1(tl, 0, n - 1, time_bound, FALSE, FALSE);
	      result = ast_pair(hd, tl);
	    }
	    goto out;
	   }
	}

      case FILE_TAG:
	if(file_sublist(&l, &loc, &i, time_bound)) break;
	else {
	  result = nil;
	  goto out;
	}

      default:
	die(109, toint(tag));
    }
  }

  out:

# ifdef DEBUG
    if(trace_extra) {
      trace_i(248, myid, failure);
      trace_print_entity(result);
      tracenl();
    }
# endif

  unreg(mark);
  unregptr(ptrmark);
  return result;
}

/*---------------------------------------------------------*/

ENTITY ast_sublist(ENTITY ll, ENTITY ij)
{
  ENTITY prim_arg = ast_pair(false_ent, ij);
  return make_lazy_prim(SUBLIST_TMO, ll, prim_arg);
}


/****************************************************************
 *			AST_UPTO				*
 ****************************************************************
 * Return the list a upto b, in lazy form.			*
 ****************************************************************/

ENTITY ast_upto(ENTITY a, ENTITY b)
{
  return make_lazy_prim(UPTO_TMO, a, b);
}


/****************************************************************
 *			AST_DOWNTO				*
 ****************************************************************
 * Return a downto b, in lazy form.                             *
 ****************************************************************/

ENTITY ast_downto(ENTITY a, ENTITY b)
{
  return make_lazy_prim(DOWNTO_TMO, a, b);
}


/****************************************************************
 *			LIST_TO_ENTLIST				*
 ****************************************************************
 * Return list-of-strings l as an entity list.			*
 ****************************************************************/

ENTITY list_to_entlist(STR_LIST *l)
{
  if(l == NIL) return nil;
  else {
    ENTITY h = make_str(l->head.str);
    ENTITY t = list_to_entlist(l->tail);
    return ast_pair(h,t);
  }
}


/****************************************************************
 *			AST_REVERSE, REVERSE_STDF		*
 ****************************************************************
 * ast_reverse(a,b,time_bound) returns the reversal of a 	*
 * followed by b, under given time bound.			*
 *								*
 * ast_reverse_stdf(x) builds a lazy entity that computes the   *
 * reversal of x.						*
 ****************************************************************/

ENTITY reverse_stdf(ENTITY x)
{
  return make_lazy_prim(REVERSE_TMO, x, nil);
}

/*------------------------------------------------------------*/

ENTITY ast_reverse(ENTITY a, ENTITY b, LONG *time_bound)
{
  ENTITY aa, bb, h;
  REG_TYPE mark = reg3(&aa, &bb, &h);

  aa = a;
  bb = b;
  for(;;) {
    IN_PLACE_EVAL_FAILTO(aa, time_bound, fail);
    goto got_aa;

   fail:
    {ENTITY result;
     if(failure != TIME_OUT_EX) result = nil;
     else result = make_lazy_prim(REVERSE_TMO, aa, bb);
     unreg(mark);
     return result;
    }

   got_aa:
    if(TAG(aa) == NOREF_TAG) {
      unreg(mark);
      return bb;
    }
    ast_split(aa, &h, &aa);
    bb = ast_pair(h, bb);
  }
}

