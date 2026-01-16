/*********************************************************************
 * File:    exprs/expr.c
 * Author:  Karl Abrahamson
 * Purpose: Implement routines for constructing expression nodes
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
 * This file contains basic constructors for expressions.  It also	*
 * constructs a few fixed expressions for the compiler to use.		*
 ************************************************************************/

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../unify/unify.h"
#include "../exprs/expr.h"
#include "../tables/tables.h"
#include "../standard/stdtypes.h"
#include "../standard/stdfuns.h"
#include "../standard/stdids.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

EXPR *hermit_expr;	/* Expression () */

EXPR *nilstr_expr;	/* nil, of type [Char]. */

EXPR *bad_expr;		/* An expression with kind BAD_E. */

EXPR *eqch_expr;	/* A global id expr for id ==. */

EXPR *shared_expr;	/* value shared, of type BoxFlavor. */

EXPR *nonshared_expr;	/* value nonshared, of type BoxFlavor. */


/****************************************************************
 *				EKINDF				*
 ****************************************************************
 * Return the kind of e. Only used in allocator test mode.	*
 * Also checks for a bad reference count, indicating a cell     *
 * that has been freed and then used.				*
 ****************************************************************/

#ifdef GCTEST
EXPR_TAG_TYPE ekindf(EXPR *e)
{
  if(e->ref_cnt < 0 && !force_ok_kind) {
    force_ok_kind = 1;
#   ifdef DEBUG
      fprintf(TRACE_FILE, "\n\nBad expr ref count\n\n");
      print_expr(e, 0);
#   endif
    die(8);
  }
  return e->kind;
}
#endif


/************************************************************************
 *				NEW_EXPR1				*
 ************************************************************************
 * Return a new expr node with given kind, e1 and line fields.		*
 ************************************************************************/

EXPR* new_expr1(EXPR_TAG_TYPE kind, EXPR *e1, int line)
{
  EXPR* e = allocate_expr();

  e->kind     = kind;
  e->LINE_NUM = line;
  bump_expr(e->E1 = e1);
  if(kind == PAT_VAR_E ||
     (e1 != NULL_E && e1->pat && kind != FUNCTION_E && kind != PAT_FUN_E &&
      kind != PAT_DCL_E && kind != EXPAND_E && kind != MATCH_E && 
      kind != FOR_E && kind != LET_E && kind != DEFINE_E)) { 
    e->pat = 1;
  }
  return e;
}


/************************************************************************
 *				NEW_EXPR1t				*
 ************************************************************************
 * Return a new expr node with given kind, e1, ty and line fields.	*
 ************************************************************************/

EXPR* new_expr1t(EXPR_TAG_TYPE kind, EXPR *e1, TYPE *t, int line)
{
  EXPR* e = new_expr1(kind, e1, line);
  bump_type(e->ty = t);
  return e;
}


/************************************************************************
 *				NEW_EXPR2				*
 ************************************************************************
 * Return a new expr node with given kind, e1, e2 and line fields.	*
 ************************************************************************/

EXPR* new_expr2(EXPR_TAG_TYPE kind, EXPR *e1, EXPR *e2, int line)
{
  EXPR* e = new_expr1(kind,e1, line);
  bump_expr(e->E2 = e2);
  if(e2 != NULL_E && e2->pat && kind != FUNCTION_E && kind != PAT_FUN_E 
     && kind != PAT_DCL_E && kind != EXPAND_E) 
    e->pat = 1;
  return e;
}


/************************************************************************
 *				NEW_EXPR3				*
 ************************************************************************
 * Return a new expr node with given kind, e1, e2, e3 and line fields.	*
 ************************************************************************/

EXPR* new_expr3(EXPR_TAG_TYPE kind, EXPR *e1, EXPR *e2, EXPR *e3, int line)
{
  EXPR* e = new_expr1(kind, e1, line);
  bump_expr(e->E2 = e2);
  bump_expr(e->E3 = e3);
  if((e2 != NULL_E && e2->pat) || (e3 != NULL_E && e3->pat)) e->pat = 1;
  return e;
}


/************************************************************************
 *				CONST_EXPR				*
 ************************************************************************
 * Return a constant of kind k with string s and type t.  Mark as 	*
 * occurring at line line.						*
 ************************************************************************/

EXPR* const_expr(char *s, int k, TYPE *t, int line)
{
  EXPR* e  = new_expr1t(CONST_E, NULL_E, t, line);
  e->STR   = s;
  e->SCOPE = k;
  return e;
}


/************************************************************************
 *				GLOBAL_ID_EXPR				*
 ************************************************************************
 * Return a global id expr node for identifier s, at line line.  s need *
 * not be in the string table.						*
 ************************************************************************/

EXPR* global_id_expr(char *s, int line)
{
  EXPR *e;
  bmp_expr(e = id_expr(s, line));
  SET_EXPR(e, cp_old_id_tm(e, 1));
  e->ref_cnt--;
  return e;
}


/************************************************************************
 *			  TYPED_GLOBAL_ID_EXPR				*
 ************************************************************************
 * Return a global id expr node for identifier s, of type t, 		*
 * at line line.  (It might be a SAME_E node, but the type will		*
 * be propagated downward to the GLOBAL_ID_E node.)			*
 * s need not be in the string table.  Returns				*
 * NULL if no such global id.						*
 ************************************************************************/

EXPR* typed_global_id_expr(char *s, TYPE *t, int line)
{
  EXPR *e, *ee;

  bmp_expr(e = id_expr(s, line));
  bump_type(e->ty = t);
  SET_EXPR(e, cp_old_id_tm(e, 1));

  for(ee = e; EKIND(ee) == SAME_E; ee = ee->E1) {
    if(!unify_u(&(ee->ty), &(ee->E1->ty), FALSE)) {
      drop_expr(e);
      return NULL;
    }
  }

  e->ref_cnt--;
  return e;
}


/************************************************************************
 *			  TYPED_GLOBAL_SYM_EXPR				*
 ************************************************************************
 * typed_global_sym_expr(n, t, line) is the same as			*
 * typed_global_id_expr(std_id[n], t, line).				*
 ************************************************************************/

EXPR* typed_global_sym_expr(int n, TYPE *t, int line)
{
  return typed_global_id_expr(std_id[n], t, line);
}


/************************************************************************
 *			  GLOBAL_SYM_EXPR				*
 ************************************************************************
 * global_sym_expr(n, line) is the same as				*
 * global_id_expr(std_id[n], line).					*
 ************************************************************************/

EXPR* global_sym_expr(int n, int line)
{
  return global_id_expr(std_id[n], line);
}


/************************************************************************
 *				ID_EXPR					*
 ************************************************************************
 * Return an identifier expr with name s, at line line.  s need not be	*
 * in the string table.							*
 ************************************************************************/

EXPR* id_expr(char *s, int line)
{
  EXPR* e = new_expr1(IDENTIFIER_E, NULL_E, line);
  e->STR = id_tb0(s);
  return e;
}


/************************************************************************
 *			       TYPED_ID_EXPR				*
 ************************************************************************
 * Similar to id_expr, but put type t in it.				*
 ************************************************************************/

EXPR* typed_id_expr(char *s, TYPE *t, int line)
{
  EXPR* e = id_expr(s, line);
  bump_type(e->ty = t);
  return e;
}
 

/************************************************************************
 *				APPLY_EXPR				*
 ************************************************************************
 * Return expr a(b), at line line.					*
 ************************************************************************/

EXPR* apply_expr(EXPR *a, EXPR *b, int line)
{
  return new_expr2(APPLY_E, a, b, line);
}


/************************************************************************
 *				TRY_EXPR				*
 ************************************************************************
 * Return expr Try a then b else c %Try, with given kind (TRY_F, etc.)  *
 * and line number.							*
 ************************************************************************/

EXPR* try_expr(EXPR *a, EXPR *b, EXPR *c, int kind, int line)
{
  EXPR *result;
  result = new_expr3(TRY_E, a, b, c, line);
  result->TRY_KIND = kind;
  return result;
}


/************************************************************************
 *				CUTHERE_EXPR				*
 ************************************************************************
 * Return expression CutHere e %CutHere.				*
 ************************************************************************/

EXPR* cuthere_expr(EXPR *e)
{
  EXPR* result = new_expr1(SINGLE_E, e, e->LINE_NUM);
  result->SINGLE_MODE = CUTHERE_ATT;
  return result;
}


/****************************************************************
 *			WSAME_E					*
 ****************************************************************
 * Same as same_e, but only make same node if line number is	*
 * different.							*
 ****************************************************************/

EXPR* wsame_e(EXPR *e, int line)
{
  if(e != NULL && line == e->LINE_NUM) return e;
  else return new_expr1(SAME_E, e, line);
}


/****************************************************************
 *			ZSAME_E					*
 ****************************************************************
 * Same as same_e, but only make same node if line number is	*
 * different or if the e is a local id.				*
 ****************************************************************/

EXPR* zsame_e(EXPR *e, int line)
{
  if(e != NULL && line == e->LINE_NUM 
     && EKIND(e) != LOCAL_ID_E && EKIND(e) != IDENTIFIER_E) return e;
  else return new_expr1(SAME_E, e, line);
}


/****************************************************************
 *			SAME_E					*
 ****************************************************************
 * Return a SAME_E expr with child e.				*
 ****************************************************************/

EXPR* same_e(EXPR *e, int line)
{
  return new_expr1(SAME_E, e, line);
}


/****************************************************************
 *		       EXPR_EQUAL				*
 ****************************************************************
 * Return true just when expressions A and B have the same      *
 * structures.							*
 *								*
 * The following forms are not handled.  expr_equal returns     *
 * true when comparing two expressions of these forms.		*
 *    MANUAL_E       						*
 *    PAT_DCL_E							*
 *    EXPAND_E							*
 *    PAT_RULE_E						*
 ****************************************************************/

Boolean expr_equal(EXPR *A, EXPR *B)
{
  EXPR_TAG_TYPE A_kind, B_kind;

  /*-------------------------------------*
   * The for-loop is for tail recursion. *
   *-------------------------------------*/

  for(;;) {
    A = skip_sames(A);
    B = skip_sames(B);

    if(A == NULL) return B == NULL;
    if(B == NULL) return FALSE;

    A_kind = EKIND(A);
    B_kind = EKIND(B);
    if(A_kind != B_kind) return FALSE;

    switch(A_kind) {
      case CONST_E:
	if(A->SCOPE != B->SCOPE || A->OFFSET != B->OFFSET) return FALSE;
        if(A->STR == NULL) return B->STR == NULL;
        if(B->STR == NULL) return FALSE;
        return strcmp(A->STR, B->STR) == 0;

      case IDENTIFIER_E:
      case LOCAL_ID_E:
      case GLOBAL_ID_E:
      case OVERLOAD_E:
      case PAT_VAR_E:
      case UNKNOWN_ID_E:
      case PAT_FUN_E:
	return A->STR == B->STR;

      case SPECIAL_E:
	if(A->PRIMITIVE != B->PRIMITIVE || A->SCOPE != B->SCOPE) return FALSE;
        return TRUE;

      case BAD_E:
	return FALSE;

      case SINGLE_E:
        if(A->SINGLE_MODE != B->SINGLE_MODE) return FALSE;
        if(A->SCOPE != B->SCOPE) return FALSE;
        /* No break; fall through to next case. */

      case OPEN_E:
      case SAME_E:
      case EXECUTE_E:
      case LAZY_LIST_E:
	A = A->E1;
        B = B->E1;
        break;      /* tail recur */

      /*----------------------------------------------------------------* 
       * RECUR_E expressions can be ignored, and we do not handle	*
       * MANUAL_E, EXPAND_E or PAT_DCL_D expressions.   		*
       *----------------------------------------------------------------*/

      case PAT_DCL_E:
      case EXPAND_E:
      case MANUAL_E:
      case RECUR_E:
      case PAT_RULE_E: 
        return TRUE;

      case TRAP_E:
	if(A->TRAP_FORM != B->TRAP_FORM) return FALSE;
        goto handle_pair;   /* just below */

      case LAZY_BOOL_E:
	if(A->LAZY_BOOL_FORM != B->LAZY_BOOL_FORM) return FALSE;
        goto handle_pair;   /* just below */

      case PAIR_E:
	if(A->SCOPE != B->SCOPE) return FALSE;
        goto handle_pair;   /* just below */

      case AWAIT_E:
	if(A->OFFSET != B->OFFSET) return FALSE;
        /* No break; fall through to next case. */

      handle_pair:
      case LET_E:
      case DEFINE_E:
      case APPLY_E:
      case SEQUENCE_E:
      case TEST_E:
      case STREAM_E:
      case FUNCTION_E:
      case LOOP_E:
      case MATCH_E:
      case WHERE_E:
	if(!expr_equal(A->E1, B->E1)) return FALSE;
        A = A->E2;
        B = B->E2;
	break;                     /* tail recur */

      case TRY_E:
        if(A->TRY_KIND != B->TRY_KIND) return FALSE;
	/* No break:  fall through to next case. */

      case IF_E:
      case FOR_E:
        if(!expr_equal(A->E1, B->E1)) return FALSE;
        if(!expr_equal(A->E2, B->E2)) return FALSE;
        A = A->E3;
        B = B->E3;
        break;             /* tail recur */

      default:
        die(10, (char *)tolong(A_kind));
        return FALSE;
    }
  }
}
    

/****************************************************************
 *			INIT_EXPRS				*
 ****************************************************************
 * Initialize expr variables and declare all standard funs      *
 * (done in standard/stdfuns.c.)				*
 ****************************************************************/

void init_exprs()
{
  /*--------------------------------------------------------------*
   * nilstr_expr is declared in standard/stdfuns.c, called below. *
   *--------------------------------------------------------------*/

  bmp_expr(hermit_expr  = const_expr(NULL, HERMIT_CONST, hermit_type, 0));
  bmp_expr(bad_expr     = new_expr1(BAD_E, NULL_E, 0));
  std_funs();
  bmp_expr(eqch_expr    = global_id_expr(std_id[EQ_SYM], 0));
  bmp_expr(shared_expr  = typed_global_id_expr(std_id[SHARED_ID], 
					       boxflavor_type, 0));
  bmp_expr(nonshared_expr  = typed_global_id_expr(std_id[NONSHARED_ID], 
					       boxflavor_type, 0));
}

