/**************************************************************
 * File:    classes/bring.c
 * Purpose: Utilities for handling types in bring declarations
 *          and expressions.
 * Author:  Karl Abrahamson
 **************************************************************/

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
#include "../classes/classes.h"
#include "../exprs/expr.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../tables/tables.h"
#include "../unify/unify.h"
#include "../error/error.h"
#include "../utils/hash.h"
#include "../lexer/modes.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			MARKED_IN_T				*
 ****************************************************************
 * Return true if t contains a marked thing.			*
 ****************************************************************/

#ifdef NEVER
PRIVATE Boolean marked_in_t(TYPE *t)
{
  if(t == NULL) return FALSE;

  TYPE_TAG_TYPE t_kind = TKIND(t);

  if(IS_STRUCTURED_T(t_kind)) {
    return marked_in_t(t->TY1) || marked_in_t(t->TY2);
  }
  
  else if(t_kind == MARK_T) return TRUE;

  else return FALSE;
}
#endif


/****************************************************************
 *			CHECK_DANGEROUS				*
 ****************************************************************
 * Warn if this is bring in a right context from a partial      *
 * representation.						*
 *								*
 * ctc is the class-table entry for the thing being brought to. *
 ****************************************************************/
 
PRIVATE void check_dangerous(CLASS_TABLE_CELL *ctc, int line)
{
  if(ctc->partial) {
    semantic_error(BRING_CONTEXT_ERR, line);
  }
}


/****************************************************************
 *			CLEAN_MARKS_FOR_BRING			*
 ****************************************************************
 * Return the result of removing all marks in t except those    *
 * on primary species or families and primary variables that	*
 * range over RANKED or ENUMERATED.  Complain if there is a	*
 * mark that needs to be removed.	       			*
 *								*
 * Also complain if there is marked thing in a right context    *
 * that has a partial representation.				*
 * Suppress this complaint if context_is_left is -1.		*
 *								*
 * context_is_left is 1 if t is in a left context, and 0  	*
 * if t is in a right context.					*
 *								*
 * line is the line number where the bring is.			*
 ****************************************************************/

PRIVATE TYPE* clean_marks_for_bring(TYPE *t, int context_is_left, int line)
{
  TYPE* st;
  TYPE_TAG_TYPE t_kind;
  Boolean marked;

  if(t == NULL) return var_t(NULL);

  st     = find_mark_u(t,&marked);  
  t_kind = TKIND(st);
  if(IS_STRUCTURED_T(t_kind)) {
    int t1_context = (context_is_left < 0)  ? -1 :
                     (t_kind == FUNCTION_T) ? 1 - context_is_left
					    : context_is_left;
    TYPE* t1 = clean_marks_for_bring(st->TY1, t1_context, line);
    TYPE* t2 = clean_marks_for_bring(st->TY2, context_is_left, line);
    if(!marked && t1 == st->TY1 && t2 == st->TY2) return t;
    else return new_type2(t_kind, t1, t2);
  }

  else if(IS_VAR_T(t_kind)) {
    if(!marked) return t;
    else if(st->ctc == RANKED_ctc || st->ctc == ENUMERATED_ctc) {
      if(context_is_left == 0) {
	check_dangerous(st->ctc, line);
      }
      if(!IS_PRIMARY_VAR_T(t_kind)) {
	semantic_error(NONPRIMARY_RANKED_IN_BRING_ERR, line);
      }
      return t;
    }
    else {
      semantic_error(BAD_MARKED_VAR_IN_BRING_ERR, line);
      err_print_ty(t);
      err_nl();
      return st;
    }
  }

  else if(t_kind == TYPE_ID_T || t_kind == FAM_T) {
    if(context_is_left == 0) {
      check_dangerous(st->ctc, line);
    }
    return t;
  }

  else return st;
}


/******************************************************************
 *			TRANSFER_MARKS_T			  *
 ******************************************************************
 * Transfer marks from source to dest, according to the structure *
 * of both.  That is, put a mark on each thing in	  	  *
 * dest that occurs in a place corresponding to a marked thing    *
 * in source.  Changes are made by rebinding types in-place.	  *
 *								  *
 * It is required that source and dest be identical types.	  *
 ******************************************************************/

PRIVATE void transfer_marks(TYPE *source, TYPE *dest)
{
  Boolean source_is_marked, dest_is_marked;
  TYPE *source_val, *dest_val;
  TYPE_TAG_TYPE source_kind, dest_kind;

  source_val  = find_mark_u(source, &source_is_marked);
  dest_val    = find_mark_u(dest, &dest_is_marked);
  source_kind = TKIND(source_val);
  dest_kind   = TKIND(dest_val);

  if(IS_STRUCTURED_T(source_kind)) {
    if(dest_kind != source_kind) {
      semantic_error(CANNOT_TRANSFER_MARKS_ERR, 0);
    }
    else {
      transfer_marks(source->TY1, dest->TY1);
      transfer_marks(source->TY2, dest->TY2);
    }
  }

  else if(source_is_marked != dest_is_marked) {
    if(dest == dest_val) {
      semantic_error(CANNOT_TRANSFER_MARKS_ERR, 0);
    }
    else {
      SET_TYPE(dest->ty1, dest_val);
      dest->kind = MARK_T;
      dest->copy = 1;
    }
  }
}


/************************************************************************
 *			CAST_FOR_BRING_T				*
 ************************************************************************
 * Returns the type to cast from when bringing to type t.  That is, if  *
 * a bring declaration has the form					*
 *									*
 *    Bring f:t from g.							*
 *									*
 * then cast_for_bring_t(t) returns the type of g.  			*
 *									*
 * Returns NULL if no answer can be found.				*
 *									*
 * Parameter line is the line where this bring occurs.			*
 *									*
 * Helper function cast_for_bring_help_t has an extra parameter that    *
 * tells whether the cast is in the right (0) or left (1) context of a  *
 * function type.							*
 *									*
 * Cast_for_bring_help_t sets *changed to 1 if its return value is	*
 * different from t, and to 0 if the the return value is the same as t. *
 *									*
 * MODE is a safe pointer.  It only lives as long as this		*
 * function call.							*
 ************************************************************************/

PRIVATE TYPE* 
cast_for_bring_help_t(TYPE *t, MODE_TYPE *mode,
		      Boolean *changed, Boolean context_is_left)
{
  Boolean starred;

  *changed = FALSE;
  if(t == NULL) return NULL;

  t = find_mark_u(t, &starred);

  switch(TKIND(t)) {

    case TYPE_ID_T:

      /*--------------------------------------------*
       * Convert to representation type if marked.  *
       *--------------------------------------------*/

      if(starred) {
	TYPE* rep_type = t->ctc->CTC_REP_TYPE;
	if(rep_type == NULL) return NULL;
	*changed = TRUE;
        return rep_type;
      }
      else {
        return t;
      }

    case FAM_VAR_T:
    case PRIMARY_FAM_VAR_T:
    case WRAP_FAM_VAR_T:
    case FAM_T:

      /*------------------------------------*
       * These are handled under FAM_MEM_T. *
       *------------------------------------*/

      return t;

    case TYPE_VAR_T:
    case PRIMARY_TYPE_VAR_T:
    case WRAP_TYPE_VAR_T:

      /*---------------------------------------------------------------*
       * Check for RANKED or ENUMERATED: both have representation type *
       * Natural.  						       *
       *---------------------------------------------------------------*/

      if(starred && (t->ctc == RANKED_ctc || t->ctc == ENUMERATED_ctc)) {
	*changed = TRUE;
	return natural_type;
      }
      else return t;

    case FAM_MEM_T:

      /*-------------------------------------------------------------*
       * To convert F(B), where F is marked, find the definition     *
       * F(A) = R of family F.  Unify A with B.  Unification will    *
       * destroy marks.  Transfer the marks from B to A.	     *
       *							     *
       * Now let R' be R with the bindings from the unification,     *
       * and with marks restored.  Convert R' recursively.	     *
       *-------------------------------------------------------------*/

      {TYPE* fam = find_mark_u(t->TY2, &starred);
       if(starred && !IS_VAR_T(TKIND(fam))) {
	 TYPE *rep_type, *cpy_arg, *cpy_rep, *result;
	 HASH2_TABLE *ty_b;

	 rep_type = fam->ctc->CTC_REP_TYPE;

	 /*----------------------------------------------------------*
	  * The representation type for a family defined by          *
	  * Species F(A) = R is (A,R).  If this family has such      *
	  * a representation type, then set cpy_arg to a copy of A,  *
	  * and cpy_rep to a copy of R.                              *
	  *----------------------------------------------------------*/

	 if(rep_type == NULL || TKIND(rep_type) != PAIR_T) return NULL;

	 *changed = TRUE;
	 ty_b     = NULL;
	 bump_type(cpy_arg = copy_type1(rep_type->TY1, &ty_b, 1));
	 bump_type(cpy_rep = copy_type1(rep_type->TY2, &ty_b, 1));
	 scan_and_clear_hash2(&ty_b, drop_hash_type);

	 /*---------------------------------------------------------*
	  * Unify A with the actual argument in t, so that bindings *
	  * will be reflected in cpy_rep.  Then transfer marks from *
          * the actual argument, since they are destroyed by	    *
	  * unification.					    *
	  *---------------------------------------------------------*/

	 if(!UNIFY(t->TY1, cpy_arg, TRUE)) {
	   drop_type(cpy_arg);
	   drop_type(cpy_rep);
	   return NULL;
	 }
	 transfer_marks(t->TY1, cpy_arg);

	 {Boolean changed1;
	  bump_type(result =  
		    cast_for_bring_help_t(cpy_rep, mode,
					  &changed1, context_is_left));
	 }
	 drop_type(cpy_arg);
         drop_type(cpy_rep);
	 if(result != NULL) result->ref_cnt--;
	 return result;
       }

       else  /* !starred */ {
         TYPE *t1;
         bump_type(t1 = 
		   cast_for_bring_help_t(t->TY1, mode,
					 changed, context_is_left));
         if(t1 == NULL) {
	   return NULL;
	 }

         if(*changed) {
	   TYPE* result = new_type2(FAM_MEM_T, t1, t->TY2);
	   drop_type(t1);
	   return result;
         }
         else {
           drop_type(t1);
           return t;
         }
       }
      }

    case PAIR_T:
    case FUNCTION_T:
      {TYPE *t1, *t2;
       Boolean changed1, changed2, lcontext;

       /*-------------------------------------*
        * Handle basic pair or function type. *
        *-------------------------------------*/

       lcontext = (TKIND(t) == PAIR_T) ? context_is_left
				       : 1 - context_is_left;
       bump_type(t1 = 
		 cast_for_bring_help_t(t->TY1, mode, &changed1, lcontext));
       bump_type(t2 = 
		 cast_for_bring_help_t(t->TY2, mode,
				       &changed2, context_is_left));
       if(t1 == NULL || t2 == NULL) {
	 drop_type(t1);
	 drop_type(t2);
	 return NULL;
       }
       if(changed1 || changed2) {
         TYPE* result = new_type2(TKIND(t), t1, t2);

	 *changed = TRUE;
	 drop_type(t1);
	 drop_type(t2);
	 return result;
       }
       drop_type(t1);
       drop_type(t2);
       return t;
      }

    case BAD_T:
      return t;

    default: 
      die(7);
      return NULL;
  }
}

/*---------------------------------------------------------*/

TYPE* cast_for_bring_t(TYPE *t, MODE_TYPE *mode, int line)
{
  TYPE *result;
  int clean_mode;
  Boolean changed;

  clean_mode = has_mode(mode, SAFE_MODE) ? -1 : 0;
  bump_type(t = clean_marks_for_bring(t, clean_mode, line));
  result = cast_for_bring_help_t(t, mode, &changed, 0);
  drop_type(t);
  return result;
}

