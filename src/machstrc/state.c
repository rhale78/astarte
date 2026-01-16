/****************************************************************
 * File:    machstrc/state.c
 * Purpose: Implement states as binary search trees, and implement
 *          nonshared and shared box operations.
 * Author:  Karl Abrahamson
 ****************************************************************/

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
 * The state holds contents of nonshared boxes. It is represented as a  *
 * binary search tree with three kind of nodes: 			*
 *    internal nodes, 							*
 *    small leaves and 							*
 *    large leaves.  							*
 * A small leaf stores just one box, while a large leaf stores many 	*
 * boxes.								*
 ************************************************************************/


#include <memory.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../unify/unify.h"
#include "../gc/gc.h"
#include "../classes/classes.h"
#ifdef DEBUG
# include "../debug/debug.h"
#include "../show/prtent.h"
#endif

PRIVATE void assign_store(ENTITY *loc, ENTITY val, 
			  ENTITY *demon, ENTITY *oldcontent);
PRIVATE STATE* new_internal_or_leaf_s(LONG b, ENTITY e, 
				      STATE *left, STATE *right);
PRIVATE STATE* new_internal_s(LONG b, ENTITY e, STATE *left, STATE *right);
PRIVATE STATE* new_leaf_s(LONG minb, LONG maxb, ENTITY *contents);
PRIVATE STATE* new_small_leaf_s(LONG b, ENTITY content);

PRIVATE void collapse_s(STATE *s);
PRIVATE void rebalance_s(STATE *s);


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			next_box_number				*
 ****************************************************************
 * next_box_number is the number that will be assigned to the   *
 * next nonshared box that is allocated.  It is used here and   *
 * in gc.c, and is initialized in stdfuns.c.			*
 ****************************************************************/

ULONG next_box_number;

/****************************************************************
 *			initial_state				*
 ****************************************************************
 * initial_state holds the initial bindings of nonshared boxes. *
 ****************************************************************/

STATE* initial_state = NULL;


/****************************************************************
 *			AST_BOX_FLAVOR				*
 ****************************************************************
 * Return the flavor (type BoxFlavor) of box b.			*
 ****************************************************************/

ENTITY ast_box_flavor(ENTITY b)
{
  return ENTU(TAG(b) == PLACE_TAG);
}

/****************************************************************
 *			AST_NEW_BOX				*
 ****************************************************************
 * Return a new nonshared box.					*
 ****************************************************************/

ENTITY ast_new_box(void)
{
  if(next_box_number > MAX_BOX_NUMBER) die(38);
  return ENTB(next_box_number++);
}


/****************************************************************
 *			AST_NEW_PLACE				*
 ****************************************************************
 * Return a new shared box.					*
 ****************************************************************/

ENTITY ast_new_place(void)
{
  register ENTITY* p = allocate_entity(1);
  *p = NOTHING; 
  return ENTP(PLACE_TAG, p);
}


/*****************************************************************
 *			NEW_BOXES				 *
 *****************************************************************
 * Return the first box number of a range of k contiguous 	 *
 * nonshared boxes.  Allocate those box numbers.		 *
 *****************************************************************/

LONG new_boxes(LONG k)
{
  LONG result;

  if(next_box_number + k > MAX_BOX_NUMBER) die(38);
  result = next_box_number;
  next_box_number += k;
  return result;
}


/****************************************************************
 *			MAKE_BXPL_EMPTY				*
 ****************************************************************
 * Empty out box or place a.  Set *demon to the demon that was  *
 * stored at this box, and oldcontent to the former contents.	*
 ****************************************************************/

void make_bxpl_empty(ENTITY a, ENTITY *demon, ENTITY *oldcontent)
{
  if(TAG(a) == WRAP_TAG) a = ENTVAL(a)[1];

  if(TAG(a) == PLACE_TAG) {

    /*------------------------------------------------------------------*
     * For a place, get a pointer to the cell to be made empty.  If	*
     * the place has tag DEMON_TAG, then must go to where the DEMON_TAG *
     * entity points. 						        *
     *------------------------------------------------------------------*/

    register ENTITY* p = ENTVAL(a);
    assign_store(p, NOTHING, demon, oldcontent);
  }

  else {
    ast_assign(a, NOTHING, demon, oldcontent);
  }
}


/*****************************************************************
 *			IS_EMPTY_BXPL				 *
 *****************************************************************
 * Return true if box or place bxpl is empty, and false if it is *
 * not empty.							 *
 *****************************************************************/

Boolean is_empty_bxpl(ENTITY bxpl)
{
  if(TAG(bxpl) == WRAP_TAG) bxpl = ENTVAL(bxpl)[1];

  if(TAG(bxpl) == BOX_TAG) {
    ast_content(bxpl);
    if(failure >= 0) {
      failure = -1;
      return TRUE;
    }
    return FALSE;
  }
  else {

    /*------------------------------------------------------------------*
     * For a place, get a pointer to the cell to be checked.  If	*
     * the place has tag DEMON_TAG, then must go to where the DEMON_TAG *
     * entity points. 						        *
     *------------------------------------------------------------------*/

    register ENTITY* p = ENTVAL(bxpl);
    if(TAG(*p) == DEMON_TAG) p = ENTVAL(*p);
    if(ENT_EQ(*p, NOTHING)) return TRUE;
    return FALSE;
  }
}


/****************************************************************
 *			AST_RANK_BOX				*
 ****************************************************************
 * Return the number of box or place a.				*
 * Note that this number can be changed by the garbage collector*
 * during compactification.					*
 *								*
 * Note: This function should do no storage allocation. 	*
 * Otherwise, change product.c:array_pre_range_stdf.		*
 ****************************************************************/

LONG box_rank(ENTITY a)
{
  if(TAG(a) == WRAP_TAG) a = ENTVAL(a)[1];

#ifdef SMALL_ENTITIES
  return VAL(a);
#else
  if(TAG(a) == BOX_TAG) return VAL(a);
  else return VAL(a) / sizeof(ENTITY);
#endif
}

ENTITY ast_rank_box(ENTITY a)
{
  return ENTI(box_rank(a));
}


/****************************************************************
 *			AST_CONTENT_S				*
 ****************************************************************
 * Return a pointer to the content of nonshared box b in state  *
 * s. Return NULL if b is not present in state s.  If mode is 0,*
 * return a pointer to the location where the true content of b *
 * is stored.  If mode is 1, return the location where the      *
 * content or DEMON_TAG entity for b is stored.			*
 ****************************************************************/

ENTITY* ast_content_s(LONG b, STATE *s, Boolean mode)
{
  LONG key;
  ENTITY *result;

  for(;;) {

    /*------------------------------------------------------------------*
     * If the state is NULL, then it contains nothing, so the box	*
     * is certainly empty.						*
     *------------------------------------------------------------------*/

    if(s == NULL)  return NULL;

    key = s->key;
    switch(STKIND(s)) {

      /*----------------------------------------------------------------*
       * At a leaf, get the content from the ST_CONTENTS array.  The	*
       * keys at the leaf are from s->key to s->ST_MAX inclusive, and   *
       * contents are stored in order in the ST_CONTENTS array.         *
       *----------------------------------------------------------------*/

      case LEAF_STT:
        if(b < key || b > s->ST_MAX) return NULL;
	result = s->ST_CONTENTS + toint(b - key);
	goto out;

      /*------------------------------------------------------------------*
       * At a small leaf, the only key is s->key, and the content of that *
       * box is stored in the ST_SMCONTENT field.			  *
       *------------------------------------------------------------------*/

      case SMALL_LEAF_STT:
	if(b != key) return NULL;
	result = &(s->ST_SMCONTENT);
	goto out;

      /*------------------------------------------------------------------*
       * At an internal node, there one key stored, with its content in   *
       * the ST_CONTENT field.  Go left if b < key, and right if b > key. *
       *------------------------------------------------------------------*/

      case INTERNAL_STT:
	if(b == key) {result = &(s->ST_CONTENT); goto out;}
	if(b < key) s = s->ST_LEFT;
 	else        s = s->ST_RIGHT;

    } /* end switch */

  } /* end for */

out:

# ifdef DEBUG
    if(statetrace > 1) {
      trace_i(306, b);
      trace_print_entity(*result);
      tracenl();
    }
# endif

  /*--------------------------------------------------------------------*
   * Here, we have the nominal content, which might be a DEMON_TAG	*
   * entity.  If so, then the content is actually in the cell pointed   *
   * to by the DEMON_TAG entity. 					*
   *--------------------------------------------------------------------*/

  if(mode && TAG(*result) == DEMON_TAG) result = ENTVAL(*result);
  return result;
}


/****************************************************************
 *			AST_CONTENT_STDF			*
 ****************************************************************
 * Return the content of bxpl in state the_act.state_a.		*
 ****************************************************************/

ENTITY ast_content_stdf(ENTITY bxpl)
{
  if(TAG(bxpl) == WRAP_TAG) bxpl = ENTVAL(bxpl)[1];

  /*----------------------------------------------------------*
   * For a nonshared box, use ast_content to get the content. *
   *----------------------------------------------------------*/

  if(TAG(bxpl) == BOX_TAG) return ast_content(bxpl);

  /*-------------------------------------------------------------*
   * For a shared box, get the content from the cell pointed to  *
   * by this entity, being sure to skip over a DEMON_TAG entity. *
   *-------------------------------------------------------------*/

  else {
    register ENTITY* p = ENTVAL(bxpl);
    if(TAG(*p) == DEMON_TAG) p = ENTVAL(*p);
    if(ENT_EQ(*p, NOTHING)) failure = EMPTY_BOX_EX;
    return *p;
  }
}


/****************************************************************
 *			PRCONTENT_STDF				*
 ****************************************************************
 * Return the content of box or place bxpl, but return NOTHING  *
 * if bxpl is empty.						*
 ****************************************************************/

ENTITY prcontent_stdf(ENTITY bxpl)
{
  if(TAG(bxpl) == WRAP_TAG) bxpl = ENTVAL(bxpl)[1];

  if(TAG(bxpl) == BOX_TAG) {
    ENTITY cont = ast_content(bxpl);
    if(failure == EMPTY_BOX_EX) {
      failure = -1;
      return NOTHING;
    }
    return cont;
  }
  else {
    register ENTITY* p = ENTVAL(bxpl);
    if(TAG(*p) == DEMON_TAG) p = ENTVAL(*p);
    return *p;
  }
}


/****************************************************************
 *			AST_CONTENT_TEST_STDF			*
 ****************************************************************
 * Return the content of box or place bxpl, but fail with       *
 * exception TEST_EX if it is empty.				*
 ****************************************************************/

ENTITY ast_content_test_stdf(ENTITY bxpl)
{
  ENTITY result;

  if(TAG(bxpl) == WRAP_TAG) bxpl = ENTVAL(bxpl)[1];

  if(TAG(bxpl) == BOX_TAG) {
    result = ast_content(bxpl);
  }
  else {
    register ENTITY* p = ENTVAL(bxpl);
    if(TAG(*p) == DEMON_TAG) p = ENTVAL(*p);
    result = *p;
  }

  if(ENT_EQ(result, NOTHING) || failure == EMPTY_BOX_EX) failure = TEST_EX;
  return result;
}


/****************************************************************
 *			AST_CONTENT				*
 ****************************************************************
 * Return the content of box bx, either by looking in state	*
 * the_act.state_a or by looking in the initial state.  bx must *
 * be a nonshared box.						*
 ****************************************************************/

ENTITY ast_content(ENTITY box)
{
  /*---------------------*
   * Try the_act.state.a *
   *---------------------*/

  ENTITY* loc = ast_content_s(VAL(box), the_act.state_a, 0);

  /*----------------------------------------------------------*
   * If can't find in the_act.state_a, try the initial state. *
   *----------------------------------------------------------*/

  if(loc == NULL) {
    loc = ast_content_s(VAL(box), initial_state, 0);
  }

  /*-----------------------*
   * If still empty, fail. *
   *-----------------------*/

  if(loc == NULL || ENT_EQ(*loc, NOTHING)) {
    failure = EMPTY_BOX_EX;
    return bad_ent;
  }
  else return *loc;
}


/******************************************************************
 *			UPDATE_HEIGHT				  *
 ******************************************************************
 * Set the height field of s.					  *
 ******************************************************************/

PRIVATE void update_height(STATE *s)
{
  if(STKIND(s) ==  INTERNAL_STT) {
    register STATE* t = s->ST_LEFT;
    register int    h = (t == NULL) ? 0 : t->height;
    register int    h2;
    t = s->ST_RIGHT;
    if(t != NULL) {
      h2 = t->height;
      if(h2 > h) h = h2;
    }
    s->height = h + 1;
  }
  else s->height = 1;
}


/****************************************************************
 *			NEW_INTERNAL_S				*
 ****************************************************************
 * Return an internal node with key b, content e, left subtree  *
 * left and right subtree right.				*
 ****************************************************************/

PRIVATE STATE* new_internal_s(LONG b, ENTITY e, STATE *left, STATE *right)
{
  register STATE *t;
  register int left_height, right_height;

  /*-------------------------------*
   * Install the root information. *
   *-------------------------------*/

  t             = allocate_state(INTERNAL_STT);
  t->key        = b;
  t->ST_CONTENT = e;

  /*---------------------------*
   * Install the left subtree. *
   *---------------------------*/

  if(left == NULL) {
    left_height = 0;
    t->ST_LEFT  = NULL;
  }
  else {
    left_height          = left->height;
    bmp_state(t->ST_LEFT = left);
  }

  /*----------------------------*
   * Install the right subtree. *
   *----------------------------*/

  if(right == NULL) {
    right_height = 0;
    t->ST_RIGHT  = NULL;
  }
  else {
    right_height          = right->height;
    bmp_state(t->ST_RIGHT = right);
  }

  /*--------------------------------*
   * Install the height and return. *
   *--------------------------------*/

  t->height = max(left_height, right_height) + 1;
  return t;
}


/****************************************************************
 *			NEW_SMALL_LEAF_S			*
 ****************************************************************
 * Return a new small leaf holding box b and given content.	*
 ****************************************************************/

PRIVATE STATE* new_small_leaf_s(LONG b, ENTITY content)
{
  register STATE* result = allocate_state(SMALL_LEAF_STT);
  result->key          = b;
  result->ST_SMCONTENT = content;
  result->height       = 1;
  return result;
}


/****************************************************************
 *			NEW_LEAF_S				*
 ****************************************************************
 * Return a new leaf state with cells for minb .. maxb		*
 * and content array contents.					*
 ****************************************************************/

PRIVATE STATE* new_leaf_s(LONG minb, LONG maxb, ENTITY *contents)
{
  register STATE *t;

  t              = allocate_state(LEAF_STT);
  t->height      = 1;
  t->key         = minb;
  t->ST_MAX      = maxb;
  t->ST_CONTENTS = contents;
  return t;
}


/****************************************************************
 *			NEW_INTERNAL_OR_LEAF_S			*
 ****************************************************************
 * Return a new internal node with key b, content e, left	*
 * subtree left and right subtree right.  However, if left and	*
 * right are both NULL, then return a leaf instead.		*
 ****************************************************************/

PRIVATE STATE* 
new_internal_or_leaf_s(LONG b, ENTITY e, STATE *left, STATE *right)
{
  if(left == NULL && right == NULL) {
    return new_small_leaf_s(b, e);
  }
  else {
    return new_internal_s(b, e, left, right);
  }
}


/******************************************************************
 *			COLLAPSE_S				  *
 ******************************************************************
 * s must be an internal node.  If both chidren of s are leaves,  *
 * the boxes in the leaves are contiguous, and the sizes of the	  *
 * leaves are sufficiently close, then s is set to a new	  *
 * large leaf holding the entire contents of both leaves and	  *
 * the root s.	  						  *
 ******************************************************************/

PRIVATE void collapse_s(STATE *s)
{
  LONG left_min, left_max, left_weight;
  LONG right_min, right_max, right_weight;
  LONG key, new_size;
  int left_kind, right_kind;
  STATE *left_subtree, *right_subtree;
  ENTITY *p, root_content;

  /*---------------------------*
   * Check for collapsibility. *
   *---------------------------*/

  /*---------------------------------*
   * Only collapse an internal node. *
   *---------------------------------*/

  if(STKIND(s) != INTERNAL_STT) return;

  /*----------------------------------------------------------------*
   * Only collapse when the the children of s are both leaves with  *
   * reference count 1.  (Don't collapse if there are outside refs  *
   * to avoid having lots of copies of things.) 		    *
   *----------------------------------------------------------------*/

  left_subtree  = s->ST_LEFT;
  if (left_subtree == NULL || left_subtree->ref_cnt > 1) return;
  left_kind = STKIND(left_subtree);
  if(left_kind == INTERNAL_STT) return;

  right_subtree = s->ST_RIGHT;
  if(right_subtree == NULL || right_subtree->ref_cnt > 1) return;
  right_kind = STKIND(right_subtree);
  if(right_kind == INTERNAL_STT) return;

  /*----------------------------------------------------------------*
   * If the largest key in the right subtree is greater than key+1, *
   * then don't collapse.					    *
   *----------------------------------------------------------------*/

  key       = s->key;
  right_min = right_subtree->key;
  if(right_min > key + 1) return;

  /*------------------------------------------------------------*
   * Only collapse if the range of boxes is contiguous.         *
   * If the largest key in the left subtree is less than key-1, *
   * then don't collapse.					*
   *------------------------------------------------------------*/

  left_min = left_subtree->key;
  if(left_kind == LEAF_STT) {
    left_max = left_subtree->ST_MAX;
    left_weight = left_max - left_min;
  }
  else {
    left_max = left_subtree->key;
    left_weight = 0;
  }
  if(left_max < key - 1) return;

  /*-------------------------------------------------------*
   * Only collapse if the weights of the leaves is close.  *
   * We consider the weights to be close if they differ by *
   * at most just a bit more than a factor of 2.	   *
   *							   *
   * Note: left_weight is one less than the number of      *
   * keys in the left leaf, and right_weight is one less   *
   * than the number of keys in the right leaf.		   *
   *-------------------------------------------------------*/

  if(right_kind == LEAF_STT) {
    right_max    = right_subtree->ST_MAX;
    right_weight = right_max - right_min;
  }
  else {
    right_max    = right_subtree->key;
    right_weight = 0; 
  }
  if( ((left_weight  + 1) << 1) < right_weight ||
      ((right_weight + 1) << 1) < left_weight) {
    return;
  }   

  /*---------------------------------------------------------------------*
   * The left subtree, node s and the right subtree make a contiguous    *
   * range of boxes, and the left and right subtrees are leaves with     *
   * about the same weight.  Try to merge into a single leaf.  This can  *
   * only be done if the resulting range is small enough to store as an  *
   * array.								 *
   *---------------------------------------------------------------------*/

  new_size = right_max - left_min + 1;
  if(new_size > ENT_BLOCK_SIZE) return;

  /*-----------------------------*
   * We are going to collapse s. *
   *-----------------------------*/

# ifdef DEBUG
    if(statetrace) {
      trace_i(311);
      print_state(s, 1);
    }
# endif

  /*------------------------------------------------------------*
   * We are going to destroy what was in node s.  Hold onto the *
   * current content.						*
   *------------------------------------------------------------*/

  root_content = s->ST_CONTENT;

  /*------------*
   * Rebuild s. *
   *------------*/

  s->kind            = LEAF_STT;
  s->key             = left_min;
  s->ST_MAX          = right_max;
  s->height	     = 1;
  s->ST_CONTENTS = p = allocate_entity(toint(new_size));

  /*------------------------------------------*
   * Copy the left subtree into the new leaf. *
   *------------------------------------------*/

  if(left_kind == LEAF_STT) {
    register LONG i;
    register ENTITY* q = left_subtree->ST_CONTENTS;
    for(i = left_min; i <= left_max; i++) *(p++) = *(q++);
  }
  else { /*left_kind == SMALL_LEAF_STT */
    *(p++) = left_subtree->ST_SMCONTENT;
  }

  /*------------------------------------------*
   * Copy the root content into the new leaf. *
   *------------------------------------------*/

  *(p++) = root_content;

  /*-------------------------------------------*
   * Copy the right subtree into the new leaf. *
   *-------------------------------------------*/

  if(right_kind == LEAF_STT) {
    register LONG i;
    register ENTITY* q = right_subtree->ST_CONTENTS;
    for(i = right_min; i <= right_max; i++) *(p++) = *(q++);
  }
  else { /* right_kind == SMALL_LEAF_STT */
    *p = right_subtree->ST_SMCONTENT;
  }

  /*------------------------------------------------------------*
   * Drop the references to left_subtree and right_subtree that *
   * were in s.							*
   *------------------------------------------------------------*/

  drop_state(left_subtree);
  drop_state(right_subtree);

# ifdef DEBUG
    if(statetrace) {
      trace_i(312);
      print_state(s, 1);
    }
# endif
}


/************************************************************************
 *			REBALANCE_S					*
 ************************************************************************
 * Rebalance the tree rooted at s if necessary.  The subtrees are 	*
 * presumed already rebalanced.						*
 ************************************************************************/

PRIVATE void rebalance_s(STATE *s)
{
  STATE *left_subtree, *right_subtree, *t, *r;
  int left_height, right_height, diff;

  /*-------------------------------------*
   * Only rebalance at an internal node. *
   *-------------------------------------*/

  if(s == NULL || STKIND(s) != INTERNAL_STT) return;

  left_subtree  = s->ST_LEFT;
  left_height   = (left_subtree == NULL) ? 0 : left_subtree->height;
  right_subtree = s->ST_RIGHT;
  right_height  = (right_subtree == NULL) ? 0 : right_subtree->height;
  diff          = left_height - right_height;

  /*-----------------------------------------------------------*
   * Rebalance if the left and right subtrees differ in height *
   * by more than 1.					       *
   *-----------------------------------------------------------*/

  if(diff >= -1 && diff <= 1) return;
  
# ifdef DEBUG
    if(statetrace) {
      trace_i(304);
      print_state(s, 1);
    }
# endif

  /*------------------------------------------*
   * Cases where the right subtree is higher. *
   *------------------------------------------*/

  if(diff < 0) {

    STATE* left_of_right      = right_subtree->ST_LEFT;
    int left_of_right_height  = (left_of_right == NULL) 
      				  ? 0 : left_of_right->height;
    STATE* right_of_right     = right_subtree->ST_RIGHT;
    int right_of_right_height = (right_of_right == NULL)
				  ? 0 : right_of_right->height;

    /*------------------------------------------------------------------*
     * We have								*
     *                            x <----s				*
     *                           / \					*
     *      left_subtree------> A   z <-------right_subtree		*
     *                             / \					*
     *        left_of_right---->  B   C	<---right_of_right		*
     *									*
     *									*
     * Do a single rotation if tree C is at least as high as tree B.    *
     * The rotation yields						*
     *									*
     *                    z  <--- s					*
     *                   / \						*
     *           t--->  x   C						*
     *                 / \						*
     *                A   B						*
     *									*
     *------------------------------------------------------------------*/

     if(right_of_right_height >= left_of_right_height) {

	/*---------------*
	 * Build node t. *
	 *---------------*/

	t = new_internal_or_leaf_s(s->key, s->ST_CONTENT, 
				   left_subtree, left_of_right);

	/*--------------------------------------------------------*
	 * Rebuild s. Note that right_subtree must be an internal *
         * node, or this branch would not have been taken.        *
	 *--------------------------------------------------------*/

        s->key        = right_subtree->key;
        s->ST_CONTENT = right_subtree->ST_CONTENT;
        SET_STATE(s->ST_LEFT, t);
        SET_STATE(s->ST_RIGHT, right_of_right);

	/*------------------------------------------------------*
	 * Now collapse t if called for.  Don't do that before  *
         * now because the reference counts of A and B might    *
	 * be dropped by the above SET_STATE's.			*
	 *------------------------------------------------------*/

        collapse_s(t);

	/*------------------------------*
	 * Install the new height of s. *
	 *------------------------------*/

	{register int s_height = t->height;
	 if(s_height < right_of_right_height) {
	   s_height = right_of_right_height;
	 }
         s->height = s_height + 1;
        }

     }

    /*------------------------------------------------------------------*
     * If a single rotation is not called for, then we must have	*
     *									*
     *                            x <----s				*
     *                           / \					*
     *      left_subtree------> A   z <-------right_subtree		*
     *                             / \					*
     *        left_of_right---->  w   C	<---right_of_right		*
     *                           / \ 					*
     *                          D   E 					*
     *									*
     * where the tree rooted at w is higher than tree C.		*
     * Do a double rotation.  The rotation yields			*
     *									*
     *                      w  <--- s					*
     *                   /    \						*
     *           r--->  x      z <--- t					*
     *                 / \    / \					*
     *                A   D  E   C					*
     *									*
     * Note: we do not want to deal with the case where w is a leaf,    *
     * since there are two different kinds of leaves.  So only          *
     * do the rotation if w is not a leaf. 				*
     *------------------------------------------------------------------*/

     else if(STKIND(left_of_right) == INTERNAL_STT) {
	STATE* D = left_of_right->ST_LEFT;
	STATE* E = left_of_right->ST_RIGHT;

        /*---------------*
         * Build node r. *
         *---------------*/

	r = new_internal_or_leaf_s(s->key, s->ST_CONTENT, left_subtree, D);

        /*---------------*
         * Build node t. *
         *---------------*/

        t = new_internal_or_leaf_s(right_subtree->key, 
				   right_subtree->ST_CONTENT, 
				   E, right_of_right);

        /*-----------------*
         * Rebuild node s. *
         *-----------------*/

	s->key        = left_of_right->key;
	s->ST_CONTENT = left_of_right->ST_CONTENT;
	SET_STATE(s->ST_LEFT, r);
	SET_STATE(s->ST_RIGHT, t);

	/*------------------------------------------------------*
	 * Now collapse r and t if called for.  Don't do that 	*
	 * before  now because the reference counts of A and B  *
	 * might be dropped by the above SET_STATE's.		*
	 *------------------------------------------------------*/

	collapse_s(r);
        collapse_s(t);

	/*------------------------------*
	 * Install the new height of s. *
	 *------------------------------*/

	{register int s_height = t->height;
	 register int r_height = r->height;
	 if(s_height < r_height) s_height = r_height;
         s->height = s_height + 1;
        }
     }

     /*-------------------------------------------------------------*
      * If no rotation is to be done, just return without a change. *
      *-------------------------------------------------------------*/

  }

  /*------------------------------------------*
   * Cases where the left subtree is heavier. *
   *------------------------------------------*/

  else { /* diff > 0 */

    STATE* left_of_left      = left_subtree->ST_LEFT;
    int left_of_left_height  = (left_of_left == NULL) 
      				  ? 0 : left_of_left->height;
    STATE* right_of_left     = left_subtree->ST_RIGHT;
    int right_of_left_height = (right_of_left == NULL)
				  ? 0 : right_of_left->height;

    /*------------------------------------------------------------------*
     * We have								*
     *                            x <----s				*
     *                           / \					*
     *      left_subtree------> z   A <-------right_subtree		*
     *                         / \					*
     *      left_of_left--->  C   B  <---right_of_left			*
     *									*
     *									*
     * Do a single rotation if tree C is at least as high as tree B. 	*
     * The rotation yields						*
     *									*
     *                    z  <--- s					*
     *                   / \						*
     *                  C   x <---- t					*
     *                     / \						*
     *                    B   A						*
     *									*
     *------------------------------------------------------------------*/

     if(left_of_left_height >= right_of_left_height) {

       /*---------------*
        * Build node t. *
        *---------------*/

       t = new_internal_or_leaf_s(s->key, s->ST_CONTENT, 
				  right_of_left, right_subtree);

       /*-----------------*
        * Rebuild node s. *
        *-----------------*/

       s->key        = left_subtree->key;
       s->ST_CONTENT = left_subtree->ST_CONTENT;
       SET_STATE(s->ST_LEFT, left_of_left);
       SET_STATE(s->ST_RIGHT, t);

       /*------------------------------------------------------*
        * Now collapse t if called for.  Don't do that before  *
	* now because the reference counts of A and B might    *
	* be dropped by the above SET_STATE's.		       *
        *------------------------------------------------------*/

       collapse_s(t);

	/*------------------------------*
	 * Install the new height of s. *
	 *------------------------------*/

       {register int s_height = t->height;
	if(s_height < left_of_left_height) {
	  s_height = left_of_left_height;
	}
        s->height = s_height + 1;
       }
     }

    /*------------------------------------------------------------------*
     * If a single rotation is not called for, then we must have	*
     *									*
     *                            x <----s				*
     *                           / \					*
     *      left_subtree------> z   A <-------right_subtree		*
     *                         / \					*
     *     left_of_left---->  C   w <---right_of_left			*
     *                           / \ 					*
     *                          E   D					*
     *									*
     * where the tree rooted at w is higher than tree C.		*
     * Do a double rotation.  The rotation yields			*
     *									*
     *                       w  <--- s					*
     *                    /    \					*
     *            r--->  z      x <--- t				*
     *                  / \    / \					*
     *                 C   E  D   A					*
     *									*
     * Note: we do not want to deal with the case where w is a leaf,    *
     * since there are two different kinds of leaves.  So only          *
     * do the rotation if w is not a leaf. 				*
     *------------------------------------------------------------------*/

     else if(STKIND(right_of_left) == INTERNAL_STT) {
       STATE* D = right_of_left->ST_RIGHT;
       STATE* E = right_of_left->ST_LEFT;

       /*---------------*
        * Build node r. *
        *---------------*/

       r = new_internal_or_leaf_s(left_subtree->key, left_subtree->ST_CONTENT,
				  left_of_left, E);

       /*---------------*
        * Build node t. *
        *---------------*/

       t = new_internal_or_leaf_s(s->key, s->ST_CONTENT, D, right_subtree);

       /*-----------------*
        * Rebuild node s. *
        *-----------------*/

       s->key        = right_of_left->key;
       s->ST_CONTENT = right_of_left->ST_CONTENT;
       SET_STATE(s->ST_LEFT, r);
       SET_STATE(s->ST_RIGHT, t);

       /*-------------------------------------------------------*
        * Now collapse r and t if called for.  Don't do that 	*
	* before  now because the reference counts of A and B   *
	* might be dropped by the above SET_STATE's.		*
        *-------------------------------------------------------*/

       collapse_s(r);
       collapse_s(t);

	/*------------------------------*
	 * Install the new height of s. *
	 *------------------------------*/

       {register int s_height = t->height;
	register int r_height = r->height;
	if(s_height < r_height) s_height = r_height;
	s->height = s_height + 1;
       }
     }

     /*-------------------------------------------*
      * If no rotation is to be done, do nothing. *
      *-------------------------------------------*/

  }

# ifdef DEBUG
    if(statetrace) {
      trace_i(305);
      print_state(s,1);
    }
# endif

}


/****************************************************************
 *			ASSIGN_STORE				*
 ****************************************************************
 * assign_store(loc,val,demon,oldcontent) sets the box whose    *
 * content is stored at location loc to have content val, 	*
 * sets *demon to the demon that is currently stored with this  *
 * and sets *oldcontent to former content of the box.		*
 * If there is no demon here, then *demon is set to nil and     *
 * *oldcontent is not set.					*
 *								*
 * Note: The value stored at *loc should be the former content  *
 * of the box.  If this is a new box, then *loc should be	*
 * false, or anything with tag other than DEMON_TAG.		*
 ****************************************************************/

PRIVATE void 
assign_store(ENTITY *loc, ENTITY val, 
	     ENTITY *demon, ENTITY *oldcontent)
{
  /*------------------------------------------------------------------*
   * If the tag of *loc is not DEMON_TAG, then just do the assignment *
   * at loc.  There is no demon.				      *
   *------------------------------------------------------------------*/

  if(TAG(*loc) != DEMON_TAG) {
    *loc   = val;
    *demon = nil;
  }

  /*------------------------------------------------------------------*
   * If the tag of *loc is DEMON_TAG, then there is a demon.  Do the  *
   * assignment at the location pointed to by the DEMON_TAG entity.   *
   *------------------------------------------------------------------*/

  else {
    ENTITY* p = ENTVAL(*loc);
    *oldcontent = *p;
    *p          = val;
    *demon      = p[1];
  }
}


/****************************************************************
 *			ON_ASSIGN_STDF				*
 ****************************************************************
 * Install demon value d into the demon list for box b.		*
 ****************************************************************/

ENTITY on_assign_stdf(ENTITY b, ENTITY d)
{
  /*----------------------------------------------------*
   * Get the location where the content of b is stored. *
   * If there is none, make one. 			*
   *----------------------------------------------------*/

  ENTITY* loc;

  if(TAG(b) == WRAP_TAG) b = ENTVAL(b)[1];

  if(TAG(b) == PLACE_TAG) loc = ENTVAL(b);
  else {
    loc = ast_content_s(VAL(b), the_act.state_a, 1);
    if(loc == NULL) {
      simple_ast_assign(b, NOTHING);
      loc = ast_content_s(VAL(b), the_act.state_a, 1);
    }
  }

  /*------------------------------------------------*
   * If there is not a demon here yet, install one. *
   *------------------------------------------------*/

  if(TAG(*loc) != DEMON_TAG) {
    ENTITY* pr = allocate_entity(2);
    pr[0] = *loc;
    pr[1] = ast_pair(d, nil);
    *loc = ENTP(DEMON_TAG, pr);
  }

  /*-------------------------------------------------*
   * If there is already a demon, add d to its list. *
   *-------------------------------------------------*/

  else {
    loc = ENTVAL(*loc);
    loc[1] = ast_pair(d, loc[1]);
  }

  return hermit;

}


/****************************************************************
 *			AST_ASSIGN_S				*
 ****************************************************************
 * Return the state obtained from state s by assigning b := e.	*
 * multref is true if s has multiple references from above, but	*
 * does not take into account s->ref_cnt directly.		*
 *								*
 * Set *demon to the demon that is present at the box that is   *
 * assigned to, and *oldcontent to the former content, when     *
 * there is a demon present.  If there is no demon, set *demon  *
 * to nil.							*
 ****************************************************************/

STATE* ast_assign_s(LONG b, ENTITY e, STATE *s, Boolean multref, 
		    ENTITY *demon, ENTITY *oldcontent)
{
  STATE *t, *result;
  LONG key;
  Boolean mr;

# ifdef DEBUG
    if(statetrace) {
      trace_i(307, b);
      trace_print_entity(e);
      trace_i(308, toint(multref));
      print_state(s, 1);
    }
# endif

  *demon = nil;			/* default */

  /*------------------------------------------------------*
   * If s is null, return a small leaf with this binding. *
   *------------------------------------------------------*/

  if(s == NULL) {
    result = new_small_leaf_s(b,e);
    goto out;
  }

  mr = multref;
  if(s->ref_cnt > 1) mr = TRUE;
  key = s->key;
  switch(STKIND(s)) {

    /*================================================================*/
    case LEAF_STT:
     {STATE *left_subtree, *right_subtree;
      ENTITY *contents, *contentB;
      LONG maxb, new_max, new_min;
      Boolean in_range;

      /*----------------------------------------------------------------*
       * Do the assignment at this leaf, either in place or by creating *
       * a new root. key is the smallest box number in this leaf.       *
       *----------------------------------------------------------------*/

      maxb     = s->ST_MAX;
      contents = s->ST_CONTENTS;
      contentB = contents + toint(b - key);

      /*----------------------------------------------------------------*
       * If there is only one reference, can afford to do the update 	*
       * in-place.							*
       *----------------------------------------------------------------*/

      if(!mr && key <= b && b <= maxb) {
	assign_store(contentB, e, demon, oldcontent);
        result = s;
	goto out;
      }

      /*----------------------------------------------------------------*
       * Otherwise, we need to build a new internal node for the	*
       * modified box.  Build the left subtree of that internal node.   *
       *----------------------------------------------------------------*/

      in_range = TRUE;
      if(b <= key) {
	in_range = FALSE;
	left_subtree = NULL;
      }
      else {
        new_max = min(b-1, maxb);
	left_subtree = new_leaf_s(key, new_max, contents);
      }

      /*---------------------------------------------------*
       * Build the right subtree of the new internal node. *
       *---------------------------------------------------*/

      if(maxb <= b) {
	in_range = FALSE;
	right_subtree = NULL;
      }
      else {
        new_min = max(b+1, key);
	right_subtree = new_leaf_s(new_min, maxb, 
				   contents + toint(new_min-key));
      }

      /*----------------------------------------------------------------*
       * Build the new internal node.  Copy the old content of b into   *
       * this internal node, and then do an assign_store so that demons *
       * will be handled.						*
       *----------------------------------------------------------------*/

      if(in_range && TAG(*contentB) == DEMON_TAG) {
        result = new_internal_s(b, *contentB, left_subtree, right_subtree);
        assign_store(&(result->ST_CONTENT), e, demon, oldcontent);
      }
      else {
	result = new_internal_s(b, e, left_subtree, right_subtree);
      }
      goto out;
     }

    /*================================================================*/
    case SMALL_LEAF_STT:

      /*------------------------------------------------------------------*
       * If b == s->key, then either do the assignment in place or return *
       * a new leaf containing this binding. 				  *
       *------------------------------------------------------------------*/

      if(b == key) {

	/*----------------------------------------------------------*
	 * If there is only one reference to this leaf, then do the *
	 * assignment in-place.					    *
	 *----------------------------------------------------------*/

	if(!mr) result = s;

	/*-----------------------------------------------------------*
	 * If there are two or more references, then build a copy of *
         * this leaf and return it, after doing the assignment.	     *
	 *-----------------------------------------------------------*/

	else {
          result  = allocate_state(SMALL_LEAF_STT);
	  *result = *s;
	  result->ref_cnt = 0;
	}
	assign_store(&(result->ST_SMCONTENT), e, demon, oldcontent);
        goto out;
      }

      /*--------------------------------------------------------------*
       * If this leaf contains a box other than b, then we must build *
       * a new internal node holding b and its content.               *
       *--------------------------------------------------------------*/

      if(b < key) result = new_internal_s(b, e, NULL, s);
      else result = new_internal_s(b, e, s, NULL);
      goto out;
      
    /*================================================================*/
    case INTERNAL_STT:

      /*--------------------------------------------------------------------*
       * When b == s->key, either do the assignment in place or make a copy *
       * of this node containing the new binding. 			    *
       *--------------------------------------------------------------------*/

      if(key == b) {

        /*-------------------------------------------------------------*
         * If there is only one reference, do the assignment in-place. *
         *-------------------------------------------------------------*/

	if(!mr) {
	  assign_store(&(s->ST_CONTENT), e, demon, oldcontent);
	  result = s;
	  goto out;
	}

        /*---------------------------------------------------------------*
         * If there are two or more references, make a copy of this node *
	 * and do the assignment in the copy.				 *
         *---------------------------------------------------------------*/

        result = new_internal_s(b, s->ST_CONTENT, s->ST_LEFT, s->ST_RIGHT);
	assign_store(&(result->ST_CONTENT), e, demon, oldcontent);	
	goto out;
      }

      /*--------------------------------------------------------*
       * If b < s->key, then the assignment must be done in  	*
       * the left subtree 					*
       *--------------------------------------------------------*/

      if(b < key) {
	bmp_state(t = ast_assign_s(b, e, s->ST_LEFT, mr, demon, oldcontent));

        /*-----------------------------------------------------------*
         * If there is only one reference to s, then update its left *
         * subtree in-place.  Rebalance and collapse.		     *
         *-----------------------------------------------------------*/

	if(!mr) {
	  SET_STATE(s->ST_LEFT, t);
	  update_height(s);
	  drop_state(t);
	  bmp_state(result = s);
	  rebalance_s(result);
          collapse_s(result);
	  result->ref_cnt--;
	  goto out;
	}

	/*--------------------------------------------------------*
	 * If there are two or more references to s, then put the *
         * new binding in a copy of s.  Rebalance.		  *
	 *--------------------------------------------------------*/

	bmp_state(result = new_internal_s(key, s->ST_CONTENT,t,s->ST_RIGHT));
	drop_state(t);
	rebalance_s(result);
	result->ref_cnt--;
	goto out;
      }

      /*----------------------------------------------------------------*
       * The case where b > key is symmetric to the case where b < key. *
       *----------------------------------------------------------------*/

      else { /* b > key */
	bmp_state(t = ast_assign_s(b, e, s->ST_RIGHT, mr, demon, oldcontent));
	if(!mr) {
	  SET_STATE(s->ST_RIGHT, t);
	  update_height(s);
	  drop_state(t);
	  bmp_state(result = s);
	  rebalance_s(result);
	  collapse_s(result);
	  result->ref_cnt--;
	  goto out;
	}
	bmp_state(result = new_internal_s(key, s->ST_CONTENT, s->ST_LEFT, t));
	drop_state(t);
	rebalance_s(result);
	result->ref_cnt--;
	goto out;
      }

     default: 
       die(39);
       return NULL;
   }

out:

# ifdef DEBUG
    if(statetrace) {
      trace_i(309);
      print_state(result, 1);
    }
# endif

  return result;
}


/****************************************************************
 *			SIMPLE_AST_ASSIGN_S			*
 ****************************************************************
 * Similar to assign_s, but ignore demon and oldcontent.        *
 ****************************************************************/

STATE* simple_ast_assign_s(LONG b, ENTITY e, STATE *s, Boolean multref)
{
  ENTITY demon, oldcontent;
  return ast_assign_s(b, e, s, multref, &demon, &oldcontent);
}


/****************************************************************
 *			AST_ASSIGN				*
 *			AST_ASSIGN_INIT				*
 *			SIMPLE_AST_ASSIGN			*
 ****************************************************************
 * ast_assign assigns box := e in activation the_act.  It also  *
 * sets *demon to the demon present for box, or to nil if there *
 * is no demon present.  If there is a demon, it sets 		*
 * *oldcontent to the former content of box.			*
 *								*
 * simple_ast_assign is similar to ast_assign, but does not     *
 * have demon or oldcontent parameters.				*
 *								*
 * ast_assign_init assigns box := e in initial_state.		*
 *								*
 * ast_assign_shared is the shared code for these.  It assumes  *
 * that the assignment is to a box, and does not check that.    *
 ****************************************************************/

PRIVATE void 
ast_assign_shared(STATE **st, ENTITY box, ENTITY e, 
		  ENTITY *demon, ENTITY *oldcontent)
{
  LONG   b = VAL(box);
  STATE* s = *st;
  SET_STATE(*st, ast_assign_s(b, e, s, 0, demon, oldcontent));

  /*-----------------------------------------------------------------------*
   * If we are updating box 'precision', then also update precision_blocks *
   *-----------------------------------------------------------------------*/

  if(b == PRECISION_BOX_VAL) {
    ENTITY demonb, oldcontentb;
    LONG prec = get_ival(e, HIGH_PREC_EX);
    precision = prec_digits(prec);
    ast_assign_shared(st, DIGITS_PRECISION_BOX, ENTU(precision),
		      &demonb, &oldcontentb);
  }

}

/*-----------------------------------------------------------------*/

void ast_assign(ENTITY box, ENTITY e, ENTITY *demon, ENTITY *oldcontent)
{
  if(TAG(box) != BOX_TAG) die(40);
  ast_assign_shared(&(the_act.state_a), box, e, demon, oldcontent);
}

/*-----------------------------------------------------------------*/

void simple_ast_assign(ENTITY box, ENTITY e)
{
  ENTITY demon, oldcontent;

  if(TAG(box) != BOX_TAG) die(40);
  ast_assign_shared(&(the_act.state_a), box, e, &demon, &oldcontent);
}

/*-----------------------------------------------------------------*/

PRIVATE void ast_assign_init(ENTITY box, ENTITY e)
{
  ENTITY demon, oldcontent;

  if(TAG(box) != BOX_TAG) die(40);
  ast_assign_shared(&(initial_state), box, e, &demon, &oldcontent);
}


/****************************************************************
 *			ASSIGN_BXPL				*
 ****************************************************************
 * Assign a := b, where a is either a box or a place.  Set      *
 * *demon to the demon for a if there is one, and *oldcontent   *
 * to the former content of a when there is a demon.  Set demon *
 * to nil when there is no demon.				*
 *								*
 * simple_assign_bxpl is similar, but does not handle demon     *
 * and oldcontent.						*
 ****************************************************************/

void assign_bxpl(ENTITY a, ENTITY b, ENTITY *demon, ENTITY *oldcontent)
{
  if(TAG(a) == WRAP_TAG) a = ENTVAL(a)[1];

  if(TAG(a) == BOX_TAG) {
    ast_assign_shared(&(the_act.state_a), a, b, demon, oldcontent);
  }
  else assign_store(ENTVAL(a), b, demon, oldcontent);
}

/*-----------------------------------------------------------------*/

void simple_assign_bxpl(ENTITY a, ENTITY b)
{
  ENTITY demon, oldcontent;
  assign_bxpl(a, b, &demon, &oldcontent);
}


/****************************************************************
 *			ASSIGN_INIT_BXPL			*
 ****************************************************************
 * Assign a := b as an initial content, where a is either a box *
 * or a place.							*
 ****************************************************************/

void assign_init_bxpl(ENTITY a, ENTITY b)
{
  ENTITY demon, oldcontent;

  if(TAG(a) == WRAP_TAG) a = ENTVAL(a)[1];

  if(TAG(a) == BOX_TAG) ast_assign_init(a, b);
  else assign_store(ENTVAL(a), b, &demon, &oldcontent);
}


/****************************************************************
 *				PREC_DIGITS			*
 ****************************************************************
 * Set the current precision to prec.  Return the number of 	*
 * blocks that is used for that many digits.			*
 ****************************************************************/

LONG prec_digits(LONG prec)
{
  if(prec < DBL_DIG) prec = DBL_DIG;
  else if(prec > MAX_PREC) prec = MAX_PREC;
  return DECIMAL_TO_DIGIT(prec);
}


/****************************************************************
 *			GET_PRECISION				*
 ****************************************************************
 * Return the current digits of precision.			*
 ****************************************************************/

LONG get_precision(void)
{
  LONG p;

  p = NRVAL(ast_content(DIGITS_PRECISION_BOX));
  if(failure == EMPTY_BOX_EX) {
    failure = -1;
    return default_digits_prec;
  }
  return p;
}


/****************************************************************
 *			GET_DEC_PRECISION			*
 ****************************************************************
 * Return the current decimal precision.			*
 ****************************************************************/

LONG get_dec_precision(void)
{
  LONG p;

  p = NRVAL(ast_content(PRECISION_BOX));
  if(failure == EMPTY_BOX_EX) {
    failure = -1;
    return default_precision;
  }
  return p;
}


/****************************************************************
 *			STKINDF					*
 ****************************************************************
 * Return the kind of s, but check for a bad reference count.   *
 * This function is only used in garbage-collection test mode.  *
 ****************************************************************/

#ifdef GCTEST

int stkindf(STATE *s)
{
  if(s->ref_cnt < 0) badrc("state", toint(s->ref_cnt), (char *) s);
  return s->kind;
}

#endif


#ifdef DEBUG
/********************************************************
 *			PRINT_STATE			*
 ********************************************************
 * Print state s in debug form on the trace file,	*
 * indented n spaces.					*
 ********************************************************/

void print_state(STATE *s, int n)
{
  indent(n); 

  if(s == NULL) {
    fprintf(TRACE_FILE, "STATE: NULL\n");
    return;
  }

  trace_i(33, toint(s->ref_cnt), toint(s->kind), toint(s->height),  s);

  indent(n+1);
  switch(s->kind) {
    case INTERNAL_STT:
      trace_i(34, s->key);
      trace_print_entity(s->ST_CONTENT);  
      tracenl();
      print_state(s->ST_LEFT, n+1);
      print_state(s->ST_RIGHT, n+1);
      return;

    case LEAF_STT:
      {LONG maxb, i, minb;
       maxb = s->ST_MAX;
       minb = s->key;
       trace_i(35, minb, maxb, s->ST_CONTENTS);
       for(i = minb; i <= maxb; i++) {
	 indent(n+1);
	 trace_print_entity(s->ST_CONTENTS[toint(i - minb)]);
	 tracenl();
       }
       return;
      }

    case SMALL_LEAF_STT:
      trace_i(36, s->key);
      trace_print_entity(s->ST_SMCONTENT); 
      tracenl();
      return;

    default: 
      fprintf(TRACE_FILE, "UNKNOWN TAG\n");
  }
}


/********************************************************
 *			PRINT_STATES			*
 ********************************************************
 * Print list of states l on the trace file in		*
 * debug form.						*
 ********************************************************/

void print_states(LIST *l)
{
  LIST *p;

  if(l == NIL) trace_i(37);
  for(p = l; p != NIL; p = p->tail) {
    fprintf(TRACE_FILE, "STATE_LIST:\n");
    print_state(p->head.state, 1);
  }
}


/********************************************************
 *			SHORT_PRINT_STATE		*
 ********************************************************
 * Print state s in a shortened debug form on the	*
 * trace file.						*
 ********************************************************/

void short_print_state(STATE *s)
{
  if(s == NULL) return;
  switch(s->kind) {
    case INTERNAL_STT:
      short_print_state(s->ST_LEFT);
      fprintf(TRACE_FILE, "[%ld := ", s->key);
      trace_print_entity(s->ST_CONTENT);
      fprintf(TRACE_FILE, "]");
      short_print_state(s->ST_RIGHT);
      return;

    case LEAF_STT:
      {LONG i, maxb, minb;
       maxb = s->ST_MAX;
       minb = s->key;
       for(i = minb; i <= maxb; i++) {
	 fprintf(TRACE_FILE, "[%ld := ", i);
	 trace_print_entity(s->ST_CONTENTS[toint(i - minb)]);
	 fprintf(TRACE_FILE, "]");
       }
       return;
      }

    case SMALL_LEAF_STT:
      fprintf(TRACE_FILE, "[%ld := ", s->key);
      trace_print_entity(s->ST_SMCONTENT);
      fprintf(TRACE_FILE, "]");
      return;

    default: 
      fprintf(TRACE_FILE, "[?]");
  }
}
  
#endif
