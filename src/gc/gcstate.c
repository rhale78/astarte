/**********************************************************************
 * File:    gc/gcstate.c
 * Purpose: Garbage collector for states
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
 * This file is part of the garbage collector.  It manages collection   *
 * of STATE structures, which hold the bindings of nonshared boxes.     *
 *									*
 * See machstrc/state.c for a description of STATE structures.		*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/gc.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../tables/tables.h"
#include "../gc/gc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			marked_boxes				*
 ****************************************************************
 * marked_boxes is the set of boxes that have been marked by	*
 * the garbage collector.  					*
 ****************************************************************/

BOXSET *marked_boxes;

/****************************************************************
 *			free_boxsets				*
 ****************************************************************
 * free_boxsets is a list of available boxset nodes.		*
 ****************************************************************/

BOXSET *free_boxsets;  

/****************************************************************
 *			seen_states				*
 ****************************************************************
 * seen_states is a chain of nodes, each containing a pointer	*
 * to a state node and a list of pointers to pointers to state	*
 * nodes.  The idea is that node (s, [v1,...,vn]) indicates	*
 * that state node s is accessible, and that it is in each	*
 * of variables v1,...,vn.  Chain seen_states is used in	*
 * the mark phase, to mark the contents of boxes in visible	*
 * states, and at the end of the mark phase to rebuild each	*
 * visible state, and to relocate the variables to the		*
 * rebuilt states.						*
 ****************************************************************/

struct seen_state_struct *seen_states;     

/********************************************************
 *			MARK_BOX_GC		        *
 ********************************************************
 * Place box b in marked_boxes, and mark the content of *
 * b in each state in seen_states.                      *
 ********************************************************/

void mark_box_gc(LONG b)
{
  struct seen_state_struct *p;

# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(172, b);
      print_boxset(marked_boxes);
      fprintf(TRACE_FILE,")\n");
    }
# endif

  if(box_member(b, marked_boxes)) return;

# ifdef DEBUG
    if(gctrace > 1) trace_i(173, b);
# endif

  marked_boxes = box_insert(b, marked_boxes);
  for(p = seen_states; p != NULL; p = p->next) {
#   ifdef DEBUG
      if(gctrace > 1) trace_i(174, p->state);
#   endif
    mark_range_gc(b, b, p->state, 0, LONG_MAX);
  }
}


/********************************************************
 *		       ADD_TO_SEEN_STATES	        *
 ********************************************************
 * Add state *s, with variable s, to seen_states.	*
 ********************************************************/

PRIVATE void add_to_seen_states(STATE **s)
{
  register struct seen_state_struct *p;
  STATE* star_s;

  /*----------------------------------------------------*
   * If *s is already in seen_states, then just add the *
   * variable s to its variable list.			*
   *----------------------------------------------------*/

  star_s = *s;
  for(p = seen_states; p != NULL; p = p->next) {
    if(p->state == star_s) {
      p->variables = gc_cons((char*) s, p->variables);
      return;
    }
  }

  /*----------------------------------------------*
   * If *s is not present yet, add a node for it. *
   *----------------------------------------------*/

  p = (struct seen_state_struct*) gc_alloc(sizeof(struct seen_state_struct));
  bump_state(p->state = star_s);
  p->variables = gc_cons((char*) s, NIL);
  p->next = seen_states;
  seen_states = p;
}


/********************************************************
 *			MARK_STATE_GC		        *
 ********************************************************
 * Mark the contents in state *s of all boxes in 	*
 * marked_boxes, and add state *s, with variable s, to	*
 * seen_states.                            		*
 ********************************************************/

void mark_state_gc(STATE **s)
{
  STATE* star_s = *s;

# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(175, star_s);
      if(*s != NULL) trace_i(176, toint(star_s->mark));
      else tracenl();
      trace_i(177);
      print_boxset(marked_boxes);
      fprintf(TRACE_FILE, ")\n");
    }
# endif

  if(star_s == NULL) return;

  /*--------------------------------------------------------------------*
   * Add s to seen_states.  This must precede marking contents of boxes *
   * in *s, since more boxes might be marked there. 			*
   *--------------------------------------------------------------------*/

  add_to_seen_states(s);

# ifdef DEBUG
    if(gctrace > 1) {
      struct seen_state_struct *p;
      trace_i(178);
      for(p = seen_states; p != NULL; p = p->next) {
	fprintf(TRACE_FILE,"%p ", p->state);
      }
      fprintf(TRACE_FILE,")\n");
    }
# endif

  /*----------------------------------------------------------*
   * Mark the contents of the currently marked boxes in       *
   * star_s.  If star_s->mark is set, then star_s is already  *
   * in seen_states, so no need to mark contents of boxes in  *
   * star_s.		      				      *
   *----------------------------------------------------------*/

  if(!(star_s->mark)) {
    mark_state_with_gc(star_s, 0, LONG_MAX, marked_boxes, 0, LONG_MAX);
  }
}


/********************************************************
 *		       UNMARK_STATE_GC		        *
 ********************************************************
 * Unmark state s and everything below it.  Also 	*
 * relocate all boxes in state s and everything below	*
 * it.							*
 ********************************************************/

void unmark_state_gc(STATE *s)
{
  while(s != NULL && s->mark) {

#   ifdef DEBUG
      if(gctrace > 1) trace_i(181, s);
#   endif

    s->mark = 0;
    switch(s->kind) {
      case INTERNAL_STT:
	GCUNMARK(&(s->ST_CONTENT));
	if(compactifying) {

#         ifdef DEBUG
	    if(gctrace > 1) trace_i(284, s->key);
#         endif

	  relocate_gc(&(s->ST_CONTENT));
	  s->key = box_relocation(s->key, marked_boxes);
	}
	unmark_state_gc(s->ST_LEFT);
	s = s->ST_RIGHT;
	break;

      case SMALL_LEAF_STT:
	GCUNMARK(&(s->ST_SMCONTENT));
	if(compactifying) {

#         ifdef DEBUG
	    if(gctrace > 1) trace_i(285, s->key);
#         endif

	  relocate_gc(&(s->ST_SMCONTENT));
	  s->key = box_relocation(s->key, marked_boxes);
	}
	return;

      default: /* LEAF_STT */
	if(compactifying) {

#         ifdef DEBUG
	    if(gctrace > 1) trace_i(286, s->key, s->ST_MAX);
#         endif

	  relocate_ptr_gc(&(s->ST_CONTENTS));
	  s->key    = box_relocation(s->key, marked_boxes);
	  s->ST_MAX = box_relocation(s->ST_MAX, marked_boxes);
	}
	return;
    }
  }
}


/********************************************************
 *		       UNMARK_SEEN_STATES_GC	        *
 ********************************************************
 * Unmark the states in list seen_states, and the trees *
 * beneath them.  Also relocate the boxes in all of	*
 * those states.                                        *
 ********************************************************/

void unmark_seen_states_gc()
{
  struct seen_state_struct *p;

  for(p = seen_states; p != NULL; p = p->next) {
    unmark_state_gc(p->state);
    SET_STATE(p->state, NULL);
  }
}


/********************************************************
 *			MARK_STATE_NODES_GC	        *
 ********************************************************
 * Set the mark bit in each node in the subtree rooted  *
 * at s.  An already marked node is assumed to be the   *
 * root of a completely marked tree.                    *
 ********************************************************/

PRIVATE void mark_state_nodes_gc(STATE *s)
{
  if(s == NULL || s->mark) return;

# ifdef DEBUG
    if(gctrace > 1) trace_i(179, s);
# endif

  s->mark = 1;
  if(s->kind == INTERNAL_STT) {
     mark_state_nodes_gc(s->ST_LEFT);
     mark_state_nodes_gc(s->ST_RIGHT);
  }
}


/************************************************************
 *			MARK_STATE_WITH_GC	            *
 ************************************************************
 * Mark the contents in s of all boxes in box set bs.       *
 * All boxes in s must be in the range min_s...max_s, and   *
 * all boxes in bs must be in the range min_bs...max_bs.    *
 * Also set the mark in each state node in tree s, assuming *
 * that a node that is already marked has all nodes         *
 * below it marked already as well.                         *
 ************************************************************/

void mark_state_with_gc(STATE *s, LONG min_s, LONG max_s,
			BOXSET *bs, LONG min_bs, LONG max_bs)
{
  LONG key, bsmin, bsmax;

  if(s == NULL) return;

  while(bs != NULL) {
#   ifdef DEBUG
      if(gctrace > 1) trace_i(179, s);
#   endif
    s->mark = 1;

#   ifdef DEBUG
      if(gctrace > 1) {
	trace_i(182, s, min_s, max_s, min_bs, max_bs);
	print_boxset(bs);
	fprintf(TRACE_FILE,")\n");
      }
#   endif

    /*----------------------------------------------*
     * Skip over parts of state that can't overlap. *
     *----------------------------------------------*/

    while(s != NULL && s->kind == INTERNAL_STT) {
      key = s->key;
      if(key > max_bs) {
	mark_state_nodes_gc(s->ST_RIGHT);
	s = s->ST_LEFT;

#       ifdef DEBUG
	  if(gctrace > 1) trace_i(179, s);
#       endif

	if(s != NULL) s->mark = 1;
	max_s = key - 1;
      }
      else if(key < min_bs) {
	mark_state_nodes_gc(s->ST_LEFT);
	s = s->ST_RIGHT;

#       ifdef DEBUG
	  if(gctrace > 1) trace_i(179, s);
#       endif

	if(s != NULL) s->mark = 1;
	min_s = key + 1;
      }
      else break;
    }

    if(s == NULL) return;

    /*---------------------------------------*
     * Mark the all boxes in the root of bs. *
     *---------------------------------------*/

    bsmin = bs->minb;
    bsmax = bs->maxb;
    mark_range_gc(bsmin, bsmax, s, min_s, max_s);

    /*------------------------------*
     * Mark the left subtree of bs. *
     *------------------------------*/

    mark_state_with_gc(s, min_s, max_s, bs->left, min_bs, bsmin - 1);

    /*-------------------------------*
     * Mark the right subtree of bs. *
     *-------------------------------*/

    min_bs = bsmax + 1;
    bs = bs->right;
  }

  mark_state_nodes_gc(s);
}


/********************************************************
 *			MARK_BOX_RANGE_GC	        *
 ********************************************************
 * mark_box_range_gc(a,b,s) marks the content of box x  *
 * in each state in seen_states, for each x             *
 * in {a,...,b} that is not in set s.  It returns the   *
 * set s union {a,...,b}.                               *
 ********************************************************/

BOXSET* mark_box_range_gc(LONG min_b, LONG max_b, BOXSET *s)
{
  struct seen_state_struct *p;
  BOXSET *s1, *s2;

# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(183, min_b, max_b);
      print_boxset(s);
      fprintf(TRACE_FILE,")\n");
    }
# endif

  if(min_b > max_b) return s;

  if(s == NULL) {
    for(p = seen_states; p != NULL; p = p->next) {
      mark_range_gc(min_b, max_b, p->state, 0, LONG_MAX);
    }
    return box_range_insert(min_b, max_b, s);
  }

  s1 = mark_box_range_gc(min_b, min(max_b, s->minb - 1), s->left);
  s2 = mark_box_range_gc(max(min_b, s->maxb + 1), max_b, s->right);

  if(s1 != s->left || s2 != s->right) {
    return collapse_boxset(new_boxset_node(s->minb, s->maxb, s1, s2));
  }
  return s;
}


/********************************************************
 *			MARK_RANGE_GC		        *
 ********************************************************
 * Mark the content of boxes minb...maxb in state s,    *
 * where all boxes in s are in the range min_s...max_s. *
 ********************************************************/

void mark_range_gc(LONG min_b, LONG max_b, STATE *s, LONG min_s, LONG max_s)
{
  LONG b;
  ENTITY *a;

  for(;;) {

    /*--------------------------------------------------*
     * If null intersection of ranges, nothing to mark. *
     *--------------------------------------------------*/

    if(min_s > max_b || max_s < min_b) return;

    /*--------------------------------------------*
     * Skip over parts of s that can't be marked. *
     *--------------------------------------------*/

    while(s != NULL && s->kind == INTERNAL_STT) {
      b = s->key;
      if(b > max_b) {
	s = s->ST_LEFT;
	max_s = b - 1;
      }
      else if(b < min_b) {
	s = s->ST_RIGHT;
	min_s = b + 1;
      }
      else break;
    }

    if(s == NULL) return;

#   ifdef DEBUG
      if(gctrace > 1) trace_i(184, min_b, max_b, s);
#   endif

    switch(s->kind) {
      case INTERNAL_STT:
	b = s->key;
	if(min_b <= b && b <= max_b) {
	  a = &(s->ST_CONTENT);

#         ifdef DEBUG
	    if(gctrace > 1) trace_i(185, a, b);
#         endif

	  mark_entity_parts_gc(a);
	}
	mark_range_gc(min_b, max_b, s->ST_LEFT, min_s, b - 1);
	s     = s->ST_RIGHT;
	min_s = b + 1;
	break;

      case SMALL_LEAF_STT:
	b = s->key;
	if(min_b <= b && b <= max_b) {
	  a = &(s->ST_SMCONTENT);

#         ifdef DEBUG
	    if(gctrace > 1) trace_i(186, a, b);
#         endif

	  mark_entity_parts_gc(a);
	}
	return;

      case LEAF_STT:
	{LONG aa, i;
	 ENTITY *ee;
	 aa = s->key;
	 b = min(s->ST_MAX, max_b);
	 for(i = max(aa, min_b); i <= b; i++) {
	   ee = s->ST_CONTENTS + toint(i - aa);

#          ifdef DEBUG
	     if(gctrace > 1) trace_i(187, ee, i);
#          endif

	   mark_entity_gc(ee);
	 }
	 return;
	}
    }
  }
}


/****************************************************************
 *			PRINT_BOXSET				*
 ****************************************************************
 * Print the boxes in box set s.				*
 ****************************************************************/

#ifdef DEBUG
void print_boxset(BOXSET *s)
{
  LONG minb, maxb;

  if(s == NULL) return;
  print_boxset(s->left);
  minb = s->minb;
  maxb = s->maxb;
  if(minb == maxb) fprintf(TRACE_FILE," %ld", minb);
  else fprintf(TRACE_FILE," %ld..%ld", minb, maxb);
  if(s->new_minb >= 0) fprintf(TRACE_FILE, "/%ld", s->new_minb);
  print_boxset(s->right);
}
#endif


/****************************************************************
 *			BOX_MEMBER				*
 ****************************************************************
 * Return true just when b is a member of set s.		*
 ****************************************************************/

Boolean box_member(LONG b, BOXSET *s)
{
  register LONG minb, maxb;

  for(;;) {
    if(s == NULL) return FALSE;
    minb = s->minb;
    maxb = s->maxb;
    if(minb <= b && b <= maxb) return TRUE;
    if(b < minb) s = s->left;
    else         s = s->right;
  }
}


/*****************************************************************
 *			NEW_BOXSET_NODE				 *
 *****************************************************************
 * Return a new boxset node representing range minb..maxb, with  *
 * left subtree left and right subtree right.  In terms of sets, *
 * this node represents left union right union {minb,...,maxb}.  *
 * It is required that all members of left be less than minb,    *
 * and all members of right be greater than maxb.                *
 *****************************************************************/

BOXSET* new_boxset_node(LONG minb, LONG maxb, BOXSET *left, BOXSET *right)
{
  BOXSET *t;
  int left_height, right_height;

  /*-----------------------------*
   * Allocate a new boxset node. *
   *-----------------------------*/

  t = free_boxsets;
  if(t == NULL) t = (BOXSET *) gc_alloc(sizeof(BOXSET));
  else free_boxsets = free_boxsets->left;

  /*----------------------*
   * Set up the new node. *
   *----------------------*/

  t->left      = left;
  t->right     = right;
  t->minb      = minb;
  t->maxb      = maxb;
  t->new_minb  = -1;
  left_height  = (left == NULL) ? 0 : left->height;
  right_height = (right == NULL) ? 0 : right->height;
  t->height    = max(left_height, right_height) + 1;

  return t;
}


#ifdef NEVER
/****************************************************************
 *			FREE_BOXSET				*
 ****************************************************************
 * Return node s to the free space list.			*
 * 								*
 * Note: this function is currrently not used.  Boxsets are	*
 * not freed.							*
 ****************************************************************/

void free_boxset(BOXSET *s)
{
  if(s != NULL) {
    s->left = free_boxsets;
    free_boxsets = s;
  }
}
#endif


/****************************************************************
 *			COLLAPSE_BOXSET				*
 ****************************************************************
 * Attempt to make set s use less space by collapsing adjacent  *
 * box ranges to a single node.  Only the children of s are     *
 * examined, not their children.                                *
 ****************************************************************/

BOXSET* collapse_boxset(BOXSET *s)
{
  LONG minb, maxb;
  int lh, rh;
  BOXSET *left, *right;

  if(s == NULL) return s;

  minb  = s->minb;
  maxb  = s->maxb;
  left  = s->left;
  right = s->right;

  if(left != NULL && left->maxb == minb - 1) {
    s->minb = left->minb;
    s->left = left->left;
#   ifdef NEVER
       free_boxset(left);
#   endif
    left = s->left;
    lh = (left == NULL) ? 0 : left->height;
    rh = (right == NULL) ? 0 : right->height;
    s->height = max(lh, rh) + 1;
  }

  if(right != NULL && right->minb == maxb + 1) {
    s->maxb = right->maxb;
    s->right = right->right;
#    ifdef NEVER
       free_boxset(right);
#    endif
    right = s->right;
    lh = (left == NULL) ? 0 : left->height;
    rh = (right == NULL) ? 0 : right->height;
    s->height = max(lh, rh) + 1;
  }

  return s;
}


/****************************************************************
 *			REBALANCE_BOXSET			*
 ****************************************************************
 * Perform a binary-tree rebalance at the root of tree s.       *
 * Box sets are height balanced.				*
 ****************************************************************/

BOXSET* rebalance_boxset(BOXSET *s)
{
  BOXSET *left, *right;
  int left_h, right_h;

  if(s == NULL) return NULL;
  left    = s->left;
  left_h  = (left == NULL) ? 0 : left->height;
  right   = s->right;
  right_h = (right == NULL) ? 0 : right->height;

  if(left_h > right_h + 1) {
    s->left     = left->right;
    left->right = s;
    return left;
  }

  else if(right_h > left_h + 1) {
    s->right    = right->left;
    right->left = s;
    return right;
  }

  return s;
}


/****************************************************************
 *			BOX_INSERT				*
 ****************************************************************
 * Insert box b into set s, and return the new set.  This is a  *
 * destructive insertion, and destroys s.                       *
 ****************************************************************/

BOXSET* box_insert(LONG b, BOXSET *s)
{
  LONG minb, maxb;

  if(s == NULL) {
    return new_boxset_node(b, b, NULL, NULL);
  }

  minb = s->minb;
  maxb = s->maxb;
  if(b >= minb && b <= maxb) return s;

  if(b == minb - 1) {
    if(box_member(b, s->left)) return s;
    s->minb = b;
    return collapse_boxset(s);
  }

  if(b == maxb + 1) {
    if(box_member(b, s->right)) return s;
    s->maxb = b;
    return collapse_boxset(s);
  }

  if(b < minb) {
    s->left = box_insert(b, s->left);
    return collapse_boxset(rebalance_boxset(s));
  }

  else if(b > maxb) {
    s->right = box_insert(b, s->right);
    return collapse_boxset(rebalance_boxset(s));
  }

  else return s;
}


/****************************************************************
 *			BOX_RANGE_INSERT			*
 ****************************************************************
 * Insert boxes a,a+1,...,b into set s, and return the new set. *
 * This is destructive, destroying s.                           *
 ****************************************************************/

BOXSET* box_range_insert(LONG a, LONG b, BOXSET *s)
{
  LONG minb, maxb;

  if(a > b) return s;
  if(s == NULL) {
    return new_boxset_node(a, b, NULL, NULL);
  }

  minb = s->minb;
  maxb = s->maxb;
  s->left = box_range_insert(a, min(b, minb-1), s->left);
  s->right = box_range_insert(max(a, maxb+1), b, s->right);
  return collapse_boxset(rebalance_boxset(s));
}

/****************************************************************
 *			COMPACTIFY_BOXSET			*
 ****************************************************************
 * Compactifying a box set consists of selecting new numbers    *
 * for boxes so that the range of new numbers is contiguous and *
 * as small as possible.                                        *
 *								*
 * compactify_boxset(s) compactifies the boxes in set s,	*
 * by placing values in the new_minb fields to indicate         *
 * the new location of minb.  It returns the smallest box       *
 * number that is not used in the compactified box set.         *
 *                                                              *
 * compactify_boxset1(s,n) compactifies s, with its smallest    *
 * box number being n.  It returns the box number that should   *
 * follow s.  So if s has k boxes, then compactify_boxset1(s,n) *
 * numbers those boxes n to n+k-1, and returns n+k.	 	*
 ****************************************************************/

PRIVATE LONG compactify_boxset1(BOXSET *s, LONG next_box)
{
  LONG n;
  if(s == NULL) return next_box;

  /*------------------------------*
   * Compactify the left subtree. *
   *------------------------------*/

  n = compactify_boxset1(s->left, next_box);

  /*------------------------------------------------------------*
   * Assign numbers to the boxes at the root of s, and then 	*
   * compactify the right subtree.  If the root includes any	*
   * of the standard boxes, don't compactify: just leave those	*
   * boxes where they are. 					*
   *------------------------------------------------------------*/

  if(s->minb < FIRST_FREE_BOX_NUM) {
    s->new_minb = s->minb;
    return compactify_boxset1(s->right, s->maxb + 1);
  }

  else {
#   ifdef DEBUG
      if(gctrace > 1) {
	trace_i(180, s->minb, s->maxb, n, n + s->maxb - s->minb);
      }
#   endif
    s->new_minb = n;
    return compactify_boxset1(s->right, n + 1 + s->maxb - s->minb);
  }
}

/*---------------------------------------------------*/

LONG compactify_boxset(BOXSET *s)
{
# ifdef DEBUG
    if(gctrace > 1) {
      trace_i(300);
      print_boxset(s);
    }
# endif

  {LONG n = compactify_boxset1(s, FIRST_FREE_BOX_NUM);
   if(n < FIRST_FREE_BOX_NUM) n = FIRST_FREE_BOX_NUM;
   return n;
  }
}


/****************************************************************
 *			BOX_RELOCATION				*
 ****************************************************************
 * Return the new location of b, as indicated by s, or -1 if b  *
 * does not exist in s.                                         *
 ****************************************************************/

LONG box_relocation(LONG b, BOXSET *s)
{
  register LONG minb, maxb;

  for(;;) {
    if(s == NULL) return -1;
    minb = s->minb;
    maxb = s->maxb;
    if(minb <= b && b <= maxb) {
      register LONG new_b = s->new_minb + (b - minb);
#     ifdef DEBUG
	if(gctrace > 1) trace_i(287, b, new_b);
#     endif
      return new_b;
    }
    if(b < minb) s = s->left;
    else         s = s->right;
  }
}


/****************************************************************
 *			RELOCATE_BOX_GC				*
 ****************************************************************
 * Replace the box in p by its new value.			*
 ****************************************************************/

void relocate_box_gc(ENTITY *p)
{
  *p = ENTB(box_relocation(VAL(*p),marked_boxes));
}


/****************************************************************
 *			REBUILD_STATES_GC			*
 ****************************************************************
 * rebuild_states_gc reconstructs all states in seen_states,	*
 * deleting empty boxes and boxes that are no longer 		*
 * accessible.  All of the mark bits are cleared.		*
 * It then drops references in seen_states.			*
 ****************************************************************/

/*--------------------------------------------------------------*
 * If b is a box number before compactification, let b' be the  *
 * corresponding box number after compactification.  Let val'	*
 * be the relocated entity val.					*
 *								*
 * copy_assign_s(s, b, val) does assignment b' := val' in state *
 * s,  but does nothing if val is NOTHING, or if box b' is not	*
 * accessible.  It returns the resulting state.			*
 *--------------------------------------------------------------*/

PRIVATE STATE* copy_assign_s(STATE* s, LONG b, ENTITY val)
{
  LONG bprime;

  /*-----------------------------------------*
   * Don't do an assignment to an empty box. *
   *-----------------------------------------*/

  if(ENT_FEQ(val, NOTHING)) return s;

  /*--------------------------------------------------------*
   * Get b'.  Don't assign to a box that is not accessible. *
   *--------------------------------------------------------*/
  
  bprime = box_relocation(b, marked_boxes);
  if(bprime < 0) return s;

  /*--------------------------------------------*
   * Do the assignment, but relocate val first. *
   *--------------------------------------------*/

  {ENTITY oldcontent, demon;
   relocate_gc(&val);
   return ast_assign_s(bprime, val, s, 0, &demon, &oldcontent);
  }
}

/*---------------------------------------------------------------*
 * copy_state(dest,source) copies all visible, nonempty bindings *
 * from state source to state dest.  The bindings in state	 *
 * source use the old box numbers (before compactification), but *
 * the bindings in dest must use the new box numbers (after	 *
 * compactification).						 *
 *								 *
 * Variable dest is presumed to have a reference count.  	 *
 * set_state is used to change it, so that the reference count   *
 * is maintained.						 *
 *								 * 
 * Note: demons are copied as well as content, since the entire	 *
 * value field is copied, and the value field is just an	 *
 * entity with tag DEMON_TAG if there is a demon present.	 *
 *---------------------------------------------------------------*/

PRIVATE void copy_state_to(STATE **dest, STATE *source)
{
 tail_recur:
  if(source == NULL) return;

  switch(source->kind) {
    case INTERNAL_STT:
      copy_state_to(dest, source->ST_LEFT);
      set_state(dest, copy_assign_s(*dest, source->key, source->ST_CONTENT));

      /*----------------------------*
       * Tail recur on s->ST_RIGHT. *
       *----------------------------*/

      source = source->ST_RIGHT;
      goto tail_recur;

    case SMALL_LEAF_STT:
      set_state(dest, copy_assign_s(*dest, source->key, source->ST_SMCONTENT));
      return;

    case LEAF_STT:
      {ENTITY* avalp = source->ST_CONTENTS;
       LONG a        = source->key;
       LONG maxa     = source->ST_MAX;
       ENTITY val;
       while(a <= maxa) {
	 val = *avalp;
	 if(GCTAG(val) == RELOCATE_TAG)  val = *ENTVAL(val);
	 set_state(dest, copy_assign_s(*dest, a, val));
	 a++;
	 avalp++;
       }
       return;
      }
  }
}
 
/*---------------------------------------------------------------*/

void rebuild_states_gc(void)
{
  struct seen_state_struct *p;
  LIST  *q;
  STATE *this_state, *this_new_state;

# ifdef DEBUG
    if(gctrace) {
      trace_i(303);
      print_boxset(marked_boxes);
      tracenl();
    }
# endif

  /*-------------------------------------------------------------*
   * This function needs to allocate from the entity heap.  Set  *
   * get_before_gc high, to suppress attempts to garbage collect.*
   *-------------------------------------------------------------*/

  get_before_gc = get_before_gc_reset = LONG_MAX;

  /*-------------------------------------*
   * Now copy each state in seen_states. *
   *-------------------------------------*/

  for(p = seen_states; p != NULL; p = p->next) {
    bump_state(this_state = p->state);
    this_new_state = NULL;  /* implicit bump */

#   ifdef DEBUG
      if(gctrace) {
        trace_i(301);
	print_state(this_state, 1);
        tracenl();
      }
#   endif

    copy_state_to(&this_new_state, this_state);
    SET_STATE(p->state, NULL);

    /*------------------------------------------------*
     * Update each of the variables that used to hold *
     * this_state, making them hold this_new_state.   *
     *------------------------------------------------*/

    for(q = p->variables; q != NIL; q = q->tail) {
      set_state(q->head.states, this_new_state);
    }

#   ifdef DEBUG
      if(gctrace) {
        trace_i(302);
	print_state(this_new_state, 1);
	tracenl();
      }
#   endif

    drop_state(this_new_state);
    drop_state(this_state);
  }
}
