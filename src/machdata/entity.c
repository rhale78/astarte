/**********************************************************************
 * File:    machdata/entity.c
 * Purpose: Initialize entities.  General entity functions.
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

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../tables/tables.h"
#include "../gc/gc.h"
#include "../rts/rts.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			PUBLIC CONSTANTS			*
 ****************************************************************
 * The following constants are initialized to hold appropriate  *
 * entities.							*
 ****************************************************************/

/****************************************************************
 *			std_box_content_type			*
 *			std_box_name				*
 ****************************************************************
 * std_box_content_type[i] is the content type of standard      *
 * (nonshared) box i.						*
 *								*
 * std_box_name is the name of standard box i.			*
 ****************************************************************/

TYPE* std_box_content_type[FIRST_FREE_BOX_NUM];
char* std_box_name[FIRST_FREE_BOX_NUM];


/************************************************************************
 * What follows are standard entities, available to the interpreter. 	*
 *									*
 *			IMPORTANT					*
 *									*
 * NOTE: if you add a standard entity, see 				*
 *									*
 *     mark_standard_ents_gc 						*
 *     do_relocation_gc							*
 *									*
 * in file gc/gc.c. 						     	*
 ************************************************************************/

/****************************************************************** 
 * The following are preprocessor symbols for SMALL_ENTITIES, but *
 * need to be variables for large entities.  See entity.h.	  *
 ******************************************************************/

#ifndef SMALL_ENTITIES
ENTITY  false_ent, 			/* false */
	true_ent, 			/* true  */

	NOTHING, 			/* A special value that indicates
					   no value. For example, this 
					   is what is put in an empty
					   shared box.  */

	GARBAGE,			/* A special value that indicates
					   a cell that has been collected
					   by the garbage collector. */

	DIGITS_PRECISION_BOX, 		/* A box that holds the current
					   number of digits (base INT_BASE)
					   of precision for real arithmetic.
					   When this is 0, it indicates that
					   ordinary (double-precision) 
					   arithmetic should be used.    */

	PRECISION_BOX, 			/* A box that holds the current
					   precision in Decimal digits.  
					   This is box precision!. */

	STDIN_BOX, 			/* The standard input, as a box. */

	TRUE_STDIN_BOX,			/* The original (keyboard) standard
					   input, before redirection. */

	STDOUT_BOX, 			/* This box holds the file to which
					   stdout has been redirected. */

        STDERR_BOX,			/* This box holds the file to which 
					   stderr has been redirected. */

	PUT_COMMAS_BOX;			/* This is putCommasInNumbers!. */
#endif


ENTITY  ten, 				/* 10 (integer) */
	zero_rat, 			/* 0  (rational) */
	zero_real, 			/* 0  (real)	 */
	a_large_int,			/* large integer */
	one_rat, 			/* 1  (rational) */
	ten_rat, 			/* 10 (rational) */
	one_real, 			/* 1  (real)	 */
	ten_real, 			/* 10 (real)	 */
	half_real,			/* 0.5 (real)	 */
        dollar_fixp_low,		/* 0.01 (real) - see prtnum.c	 */

	large_ten_real, 	        /* 10 (real, forced to large
					   representation, not double. */

	large_one_real, 		/* 1  (real, forced to large
					   representation, not double. */

	newline, 			/* ["\n"] */

	infloop_timeout, 		/* A special value that indicates
					   an infinite loop has been
					   detected. */

	stdout_file,			/* stdout (before redirection via *
					 * tell) */

	stderr_file, 			/* stderr */

	true_stdin_box, 		/* stdin (before redirection) */

	bad_ent, 			/* An undefined entity. */

	divide_by_zero_ex;		/* The exception that occurs at
					   divide by 0. */

ENTITY ten_to_p[TEN_TO_MAX];		/* ten_to_p[n] is 10^n. */

intset  int_tags, 			/* Tags that can be the tag of
					   something of species Natural or
					   Integer. */

	real_tags, 			/* Tags that can be the tag of
					   something of species Real. */

	lazy_tags, 			/* Tags that can indicate something
					   that is lazy, and needs 
					   evaluating before use. Excludes
					   tags that indicate indirections.*/

	all_lazy_tags, 			/* Same as lazy_tags, but also includes
					   tags that indicate indirections. */

	full_eval_tags,			/* Tags that indicate something that
					   might be lazy or contain lazy
					   components. This contains, for
					   example, PAIR_TAG.    */

	no_copy_tags, 			/* Tags that should not be referred
					   to directly, but should be 
					   referred to through indirections. 
					   (They should be referred to through
					   indirections because they might
					   change.) */

	always_store_tags, 		/* Tags that are tags of things that
					   should always be stored as the
					   value of a computation, even
					   when storing is suppressed. */

	all_weak_lazy_tags, 		/* all_lazy_tags less FILE_TAG.
					   A file can be lazy, but in some 
					   circumstances we prefer to 
					   suppress evaluation (that is,
					   reading) of files. */

	binary_ptr_tags,		/* Tags that have associated with
					   them a pointer to a binary 
					   chunk. */

	entp_tags;			/* Tags that have associated with
					   them a pointer of type ENTITY*. */


/*----------------------------------------------------------------------*
 * For SMALL_ENTITIES, MAKEENT is a macro.  For large entities, we	*
 * need it to invoke a function, make_ent.  make_ent(tag,val) is an 	*
 * entity with given tag and associated value.				*
 *----------------------------------------------------------------------*/

#ifndef SMALL_ENTITIES
ENTITY make_ent(int tag, LONG val)
{
  register ENTITY e;
  e.tag = tag;
  e.val = val;
  return e;
}
#endif

#ifdef STRUCT_ENTITY
#ifdef SMALL_ENTITIES
ENTITY make_ent(LONG l)
{
  ENTITY result;
  result.val = l;
  return result;
}
#endif
#endif

/*---------------------------------------------------------*/

#ifndef SMALL_ENTITIES
void init_basic_entities(void)
{
  false_ent		= ENTU(0);
  true_ent	        = ENTU(1);
  NOTHING		= ENTP(INDIRECT_TAG, NULL);
  GARBAGE		= ENTP(RELOCATE_TAG, NULL);
  DIGITS_PRECISION_BOX  = (ENTB(DIGITS_PRECISION_BOX_VAL));
  PRECISION_BOX 	= (ENTB(PRECISION_BOX_VAL));
  STDIN_BOX	     	= (ENTB(STDIN_BOX_VAL));
  TRUE_STDIN_BOX	= (ENTB(TRUE_STDIN_BOX_VAL));
  STDOUT_BOX		= (ENTB(STDOUT_BOX_VAL));
  STDERR_BOX		= (ENTB(STDERR_BOX_VAL));
  PUT_COMMAS_BOX	= (ENTB(PUT_COMMAS_BOX_VAL));
}
#endif

/****************************************************************
 *			INIT_ENTITY				*
 ****************************************************************
 * Set up the standard entities and tag sets.			*
 ****************************************************************/

void init_entity(void)
{
  int i;

  /*----------*
   * Tag sets *
   *----------*/

  int_tags =    (1L << INT_TAG)
	      | (1L << BIGPOSINT_TAG)
	      | (1L << BIGNEGINT_TAG);

  real_tags =   (1L << SMALL_REAL_TAG)
	      | (1L << LARGE_REAL_TAG);

  no_copy_tags = (1L << LAZY_TAG)
	       | (1L << LAZY_LIST_TAG)
	       | (1L << LAZY_PRIM_TAG)
	       | (1L << TREE_TAG)
	       | (1L << GLOBAL_TAG)
	       | (1L << FILE_TAG);

  lazy_tags = no_copy_tags
	      | (1L << FAIL_TAG)
	      | (1L << FILE_TAG);

  all_lazy_tags = lazy_tags
	      | (1L << INDIRECT_TAG)
	      | (1L << GLOBAL_INDIRECT_TAG);

  all_weak_lazy_tags = all_lazy_tags & ~(1L << FILE_TAG);

  full_eval_tags = all_lazy_tags
/*	      | (1L << QUINT_TAG) */
	      | (1L << PAIR_TAG)
	      | (1L << TRIPLE_TAG)
	      | (1L << QUAD_TAG);

  always_store_tags =
		(1L << NOREF_TAG)
	      | (1L << FAIL_TAG)
	      | (1L << SMALL_REAL_TAG)
	      | (1L << BOX_TAG)
	      | (1L << PLACE_TAG);

  binary_ptr_tags = 
                (1L << BIGPOSINT_TAG) 
	      | (1L << BIGNEGINT_TAG)
	      | (1L << STRING_TAG);

  entp_tags =   (1L << GLOBAL_INDIRECT_TAG)
	      | (1L << INDIRECT_TAG)
	      | (1L << LAZY_PRIM_TAG)
	      | (1L << LARGE_REAL_TAG)
	      | (1L << WRAP_TAG)
	      | (1L << QWRAP0_TAG)
	      | (1L << QWRAP1_TAG)
	      | (1L << QWRAP2_TAG)
	      | (1L << QWRAP3_TAG)
	      | (1L << PAIR_TAG)
	      | (1L << TRIPLE_TAG)
	      | (1L << QUAD_TAG)
	      | (1L << TREE_TAG)
	      | (1L << ARRAY_TAG)
	      | (1L << APPEND_TAG)
	      | (1L << FAIL_TAG)
	      | (1L << PLACE_TAG);

  /*--------------------*
   * Standard entities. *
   *--------------------*/

  failure		= -1;		/* Be sure constructors are not
					   confused by failure. */
  ten                   = ENTI(10);
  a_large_int		= ast_make_int(1000000L);
  a_large_int           = ast_mult(a_large_int, a_large_int);
  zero_rat 		= make_rat(zero, one);
  one_rat		= make_rat(one, one);
  ten_rat		= make_rat(ten, one);
  zero_real 		= ast_make_real(0.0);
  one_real 		= ast_make_real(1.0);
  ten_real 		= ast_make_real(10.0);
  half_real 		= ast_make_real(0.5);
  dollar_fixp_low	= ast_make_real(0.01);
  newline		= ast_pair(make_str("\n"), nil);

  {ENTITY *p;
   p                    = allocate_entity(3);
   p[0]			= ENTU(INFLOOP_TMO);
   p[1]			= nil;
   p[2]			= nil;
   infloop_timeout      = ENTP(LAZY_PRIM_TAG, p);
  }

  divide_by_zero_ex     = qwrap(DOMAIN_EX, make_cstr("divide by zero"));
  bad_ent 		= zero;

  {struct file_entity *fent;
   fent           	= alloc_file_entity();
   fent->kind 		= STDOUT_FK;
   fent->descr_index    = 0;
   stdout_file		= ENTP(FILE_TAG, fent);
   fent			= alloc_file_entity();
   fent->kind		= STDERR_FK;
   fent->descr_index    = 0;
   stderr_file		= ENTP(FILE_TAG, fent);
  }

  large_one_real        = int_to_large_real(one);
  large_ten_real        = int_to_large_real(ten);

  /*---------------------------------------------------------------*
   * Powers of 10.  Zero out first, in case of garbage collection. *
   *---------------------------------------------------------------*/

  for(i = 0; i < TEN_TO_MAX; i++) ten_to_p[i] = zero;
  ten_to_p[0] = one;
  for(i = 1; i < TEN_TO_MAX; i++) {
    ten_to_p[i] = ast_mult(ten, ten_to_p[i-1]);
  }

# ifdef DEBUG
    if(gctrace) trace_i(188);
# endif

}


/****************************************************************
 *			ENTVALF					*
 ****************************************************************
 * ENTVAL, as a function.					*
 ****************************************************************/

ENTITY* entvalf(ENTITY e)
{
  return ENTVAL(e);
}


/****************************************************************
 *			TAGF					*
 ****************************************************************
 * TAG, as a function.						*
 ****************************************************************/

int tagf(ENTITY e)
{
  return TAG(e);
}


/****************************************************************
 *			MAKE_FAIL_ENT				*
 ****************************************************************
 * Return a FAIL_TAG entity with exception failure_as_entity.   *
 ****************************************************************/

ENTITY make_fail_ent(void)
{
  ENTITY* f = allocate_entity(1);
  if(ENT_EQ(failure_as_entity, NOTHING)) *f = ENTI(failure);
  else *f = failure_as_entity;
  return ENTP(FAIL_TAG, f);
}


/****************************************************************
 *			MAKE_INDIRECT				*
 ****************************************************************
 * Return an indirection to location loc.  If loc contains a 	*
 * GLOBAL_TAG object, then loc+1 must contain a type for that	*
 * global object.						*
 ****************************************************************/

ENTITY make_indirect(ENTITY *loc)
{
  register int tag;
  tag = (TAG(*loc) == GLOBAL_TAG) ? GLOBAL_INDIRECT_TAG : INDIRECT_TAG;
  return ENTP(tag, loc);
 } 


/****************************************************************
 *			REMOVE_INDIRECTION			*
 ****************************************************************
 * Return the result of skipping over indirections from e.      *
 ****************************************************************/

ENTITY remove_indirection(ENTITY e)
{
  register int tag = TAG(e);

  while(tag == INDIRECT_TAG || tag == GLOBAL_INDIRECT_TAG) {
    e = *ENTVAL(e);
    tag = TAG(e);
  }
  return e;
}

/****************************************************************
 *			TEN_INT_TO				*
 ****************************************************************
 * Return 10^n, as an integer.					*
 ****************************************************************/

ENTITY ten_int_to(LONG n)
{
  ENTITY t;

  if(n < 0) die(52);

  /*------------------------------------------*
   * Look up small powers of 10 in the table. *
   *------------------------------------------*/

  if(n < TEN_TO_MAX) {
    return ten_to_p[toint(n)];
  }

  /*-------------------------------*
   * Compute larger powers of ten. *
   *-------------------------------*/

  /*-------------*
   * Odd powers. *
   *-------------*/

  if((n & 1) == 1) {
    t = ten_int_to(n-1);
    return ast_mult(ten, t);
  }

  /*--------------*
   * Even powers. *
   *--------------*/

  else {
    t = ten_int_to(n>>1);
    return ast_mult(t,t);
  }
}


/****************************************************************
 *			TEN_REAL_TO				*
 ****************************************************************
 * Return 10^n, as a Real.					*
 ****************************************************************/

ENTITY ten_real_to(LONG n)
{
  ENTITY t;
  int sign_n = 1;

  /*-------------------------*
   * Handle negative powers. *
   *-------------------------*/

  if(n < 0) {
    n      = -n;
    sign_n = -1;
  }

  /*-------------------------------------------*
   * Small nonnegative powers are in ten_to_p. *
   *-------------------------------------------*/

  if(n < TEN_TO_MAX) {
    t = ast_real(ten_to_p[toint(n)]);
  }

  /*-------------*
   * Odd powers. *
   *-------------*/

  else if((n & 1) == 1) {
    t = ten_real_to(n-1);
    t = ast_mult(ten_real, t);
  }

  /*--------------*
   * Even powers. *
   *--------------*/

  else {
    t = ten_real_to(n>>1);
    t = ast_mult(t,t);
  }

  if(sign_n < 0) t = ast_reciprocal(t);
  return t;
}


/****************************************************************
 *			IS_LAZY_FILE				*
 ****************************************************************
 * Return true if e is a lazy file entity.  (A lazy file is	*
 * a reference to a file that has not been read.)		*
 ****************************************************************/

Boolean is_lazy_file(ENTITY e)
{
  struct file_entity *fent;
  int kind;

  fent = FILEENT_VAL(e);
  kind = fent->kind;
  if(kind == STDOUT_FK || kind == STDERR_FK || kind == OUTFILE_FK) {
    return FALSE;
  }
  if(kind == STDIN_FK) return TRUE;
  if(kind == INFILE_FK) return ENT_EQ(fent->u.file_data.val, true_ent);
  else return TRUE;
}


/****************************************************************
 *			IS_LAZY					*
 ****************************************************************
 * Return true if e is a lazy entity.				*
 ****************************************************************/

Boolean is_lazy(ENTITY e)
{
  int tag;
  for(;;) {
    tag = TAG(e);
    switch(tag) {
      case FILE_TAG:
	return is_lazy_file(e);

      case LAZY_PRIM_TAG:
	{Boolean b;
	 e = scan_through_unknowns(e, &b);
	 tag = TAG(e);
	 if(MEMBER(tag, lazy_tags)) return TRUE;
	 break;
	}

      case LAZY_TAG:
      case LAZY_LIST_TAG:
      case GLOBAL_TAG:
      case FAIL_TAG:
	return TRUE;

      case INDIRECT_TAG:
	if(ENT_EQ(e, NOTHING)) return FALSE;
      case GLOBAL_INDIRECT_TAG:
	e = *ENTVAL(e);
	break;

      case TREE_TAG:
	return ENT_EQ(*ENTVAL(e),NOTHING);

      default:
	return FALSE;
    }
  }
}


/****************************************************************
 *			HAS_LAZY_PARTS				*
 ****************************************************************
 * Return true if e has any lazy parts to it.  Lazy parts can   *
 * be lazy entities that are found inside pairs, etc.  Do not	*
 * look inside boxes.						*
 ****************************************************************/

Boolean has_lazy_parts(ENTITY ee)
{
  ENTITY a, e;
  Boolean result;
  int tag;

  e = ee;
  for(;;) {
    tag = TAG(e);
    result = TRUE;
    if(MEMBER(tag, lazy_tags)) {
      goto out;
    }
    switch(tag) {
      case GLOBAL_INDIRECT_TAG:
      case INDIRECT_TAG:
      case QWRAP0_TAG:
      case QWRAP1_TAG:
      case QWRAP2_TAG:
      case QWRAP3_TAG:
        e = *ENTVAL(e);
        break;

      case WRAP_TAG:
	e = ENTVAL(e)[1];
	break;

      case APPEND_TAG:
	a = *ENTVAL(e);
	if(has_lazy_parts(a)) {
	  goto out;
        }
        e = ENTVAL(e)[1];
        break;

      case FILE_TAG:
        if(is_lazy_file(e)) goto out;
        /* no break - continue with next case. */

      case PAIR_TAG:
      case TRIPLE_TAG:
      case QUAD_TAG:
      /*case QUINT_TAG:*/
      case ARRAY_TAG:
	a = ast_head(e);
        if(has_lazy_parts(a)) {
          goto out;
        }
        e = ast_tail(e);
        break;

      default: 
	result = FALSE;
	goto out;
    }
  }

 out:
  return result;
}


/****************************************************************
 *			DEF_STDIN				*
 ****************************************************************
 * If file_name is not null, then assign an input file named 	*
 * file_name to box 'box' in state *s, and set *s to the new 	*
 * state.  If file_name is null, then assign the standard input *
 * to box in state s.						*
 ****************************************************************/

PRIVATE void def_stdin(LONG box, char *file_name, STATE **s)
{
  ENTITY *e, l;
  struct file_entity *f;

  /*---------------*
   * Get the file. *
   *---------------*/

  f = alloc_file_entity();
  if(file_name == NULL) {
    f->kind = STDIN_FK;
    f->u.file_data.val  = true_ent;
  }
  else {
    f->kind              = INFILE_FK;
    f->descr_index       = -1;
    f->u.file_data.pos   = 0;
    f->u.file_data.val   = true_ent;
    f->u.file_data.name  = stdin_redirect;
  }
  *(e = allocate_entity(1)) = ENTP(FILE_TAG, f);
  l = ENTP(INDIRECT_TAG, e);

  /*-------------------------*
   * Perform the assignment. *
   *-------------------------*/

  set_state(s, simple_ast_assign_s(box, l, *s, 0));
}


/****************************************************************
 *			INIT_STATE				*
 ****************************************************************
 * This is called when the interpreter initializes.  It sets up *
 * the standard boxes.						*
 ****************************************************************/

void init_state(void)
{
  STATE *s;

# ifdef DEBUG
    if(trace) trace_i(310);
# endif

  /*------------------------------------------------------*
   * Place precision and blocks_precision boxes in state. *
   *------------------------------------------------------*/

  s = NULL;
  SET_STATE(s, simple_ast_assign_s(PRECISION_BOX_VAL, 
			    ast_make_int(default_precision), s, 0));
  SET_STATE(s, simple_ast_assign_s(DIGITS_PRECISION_BOX_VAL, 
			    ast_make_int(default_digits_prec), s, 0));
  bmp_type(std_box_content_type[PRECISION_BOX_VAL]        = natural_type);
  bmp_type(std_box_content_type[DIGITS_PRECISION_BOX_VAL] = natural_type);
  std_box_name[PRECISION_BOX_VAL] = std_id[PRECISION_ID];

  /*---------------------------------------*
   * Place stdin! and trueStdin! in state. *
   *---------------------------------------*/

  def_stdin(STDIN_BOX_VAL, stdin_redirect, &s);
  true_stdin_box = TRUE_STDIN_BOX;
  def_stdin(TRUE_STDIN_BOX_VAL, NULL, &s);
  bmp_type(std_box_content_type[STDIN_BOX_VAL]      = string_type);
  bmp_type(std_box_content_type[TRUE_STDIN_BOX_VAL] = string_type);
  std_box_name[STDIN_BOX_VAL]      = std_id[STDIN_ID];
  std_box_name[TRUE_STDIN_BOX_VAL] = std_id[TRUESTDIN_ID];

  /*---------------------------------*
   * Initialize stdout! and stderr!. *
   *---------------------------------*/

  SET_STATE(s, simple_ast_assign_s(STDOUT_BOX_VAL, stdout_file, s, 0));
  SET_STATE(s, simple_ast_assign_s(STDERR_BOX_VAL, stderr_file, s, 0));
  bmp_type(std_box_content_type[STDOUT_BOX_VAL] = outfile_type);
  bmp_type(std_box_content_type[STDERR_BOX_VAL] = outfile_type);
  std_box_name[STDOUT_BOX_VAL] = std_id[STDOUT_ID];
  std_box_name[STDERR_BOX_VAL] = std_id[STDERR_ID];

  /*------------------------------*
   * Put false in put_commas_box. *
   *------------------------------*/

  SET_STATE(s, simple_ast_assign_s(PUT_COMMAS_BOX_VAL, false_ent, s, 0));
  bmp_type(std_box_content_type[PUT_COMMAS_BOX_VAL] = boolean_type);
  std_box_name[PUT_COMMAS_BOX_VAL] = std_id[PUTCOMMASINNUMBERS_ID];

  next_box_number = FIRST_FREE_BOX_NUM;
  bmp_state(initial_state = s);
  execute_state = s;    /* inherits ref from s. */
}
