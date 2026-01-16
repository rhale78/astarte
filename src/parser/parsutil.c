/*****************************************************************
 * File:    parser/parsutil.c
 * Purpose: Assorted utilities for parser.
 * Author:  Karl Abrahamson
 *****************************************************************/

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
 * This file contains assorted utilities used by the parser.  Included	*
 * are									*
 *									*
 *  functions to build a few specialized kinds of expressions and	*
 *  types,								*
 *									*
 *  functions for handling advisories					*
 *									*
 *  functions for handling loops and choose expressions			*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../machdata/except.h"
#include "../classes/classes.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/meettbl.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../parser/parser.h"
#include "../parser/tokens.h"
#include "../lexer/modes.h"
#include "../patmatch/patmatch.h"
#include "../infer/infer.h"
#include "../ids/ids.h"
#include "../unify/unify.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			MAKE_ROLE_TYPE_EXPR_P			*
 ****************************************************************
 * r is an identifier and v is a role/type expr. Build		*
 * expression r1::v.						*
 ****************************************************************/

RTYPE make_role_type_expr_p(char *r, RTYPE v)
{
  RTYPE result;
  ROLE *rol, *bas;

  bump_role(bas = basic_role(r));
  SET_LIST(bas->namelist, complete_namelist(bas->namelist));
  bump_role(rol = meld_roles(v.role, bas));
  role_error_occurred = FALSE;
  if(rol != NULL) check_namelist(rol->namelist, NULL);
  if(role_error_occurred) {
    Boolean sing;
    SET_LIST(rol->namelist, clean_role(rol->namelist, &sing));
  }
  result.type = v.type;
  result.role = rol;
  drop_role(bas);
  if(rol != NULL) rol->ref_cnt--;
  return result;
}


/****************************************************************
 *			TYPE_ID_TOK_P				*
 *			TYPE_ID_TOK_FROM_CTC_P			*
 ****************************************************************
 * Handle a TYPE_ID_TOK, FAM_ID_TOK, GENUS_ID_TOK or		*
 * COMM_ID_TOK token.  Return the type.  The returned type is   *
 * a primary species for a TYPE_ID_TOK or FAM_ID_TOK, and is a	*
 * secondary species for a GENUS_ID_TOK or a COMM_ID_TOK.	*
 * 								*
 * There are two versions.  One receives the name of the  	*
 * type, and the other receives a pointer to its class table	*
 * cell.							*
 *								*
 * Parameter name can be NULL, indicating ANY.			*
 ****************************************************************/

TYPE* type_id_tok_from_ctc_p(CLASS_TABLE_CELL *ctc)
{
  TYPE *t;
  int ctc_code = ctc == NULL ? GENUS_ID_CODE : ctc->code;

  /*---------------------------------------------------------*
   * Case where ctc is a species or family.  Then   	     *
   * get the type node out of the ctc entry.		     *
   *							     *
   * If we are asked to create a wrap type or family, then   *
   * we must copy that node and change its kind.	     *
   *---------------------------------------------------------*/

  if(ctc != NULL && (ctc_code == TYPE_ID_CODE || ctc_code == FAM_ID_CODE)) {
    t = ctc->ty;
  }

  /*------------------------------------------------------------*
   * If ctc is a genus or community, then just build a node    	*
   * for it.   It must be a wrapped type.			*
   *------------------------------------------------------------*/

  else {
    t = new_type(ctc_code == GENUS_ID_CODE ? WRAP_TYPE_T : WRAP_FAM_T, NULL);
    if(ctc == ctcs[0]) ctc = NULL;
    t->ctc = ctc;
  }

  return t;
}

/*--------------------------------------------------------------*/

TYPE* type_id_tok_p(char *name)
{
  CLASS_TABLE_CELL *ctc;

  /*----------------------------------------------------*
   * If name is NULL, then the class table cell		*
   * is NULL as well.  Otherwise, get the class table	*
   * entry.  It will not be null, but check anyway.	*
   *----------------------------------------------------*/

  if(name == NULL || strcmp(name, std_id[ANYG_TYPE_ID]) == 0) {
    ctc = NULL;
  }
  else {
    ctc = get_ctc_tm(name);
    if(ctc == NULL) {
      semantic_error1(UNKNOWN_ID_ERR, name, 0);
      return hermit_type;
    }
  }

  return type_id_tok_from_ctc_p(ctc);
}


/****************************************************************
 *			DO_FOR_ADVISORY				*
 ****************************************************************
 * Process declaration 						*
 *								*
 *   Advisory props for ids:tag %Advisory			*
 ****************************************************************/

void do_for_advisory(LIST *props, LIST *ids, TYPE *tag)
{
  LIST *p,*q;
  char *prop;

  for(p = props; p != NIL; p = p->tail) {
    prop = p->head.str;
    for(q = ids; q != NIL; q = q->tail) {
      attach_property(prop, q->head.str, tag);
    }
  }
}

/****************************************************************
 *			MAKE_FOR_ADVISORY			*
 ****************************************************************
 * Return an expression that represents declaration		*
 *								*
 *  Advisory props for ids: tag %Advisory			*
 ****************************************************************/

EXPR* make_for_advisory(LIST *props, LIST *ids, RTYPE tag, int line)
{
  LIST *p;
  EXPR *result, *this_advisory;

  result = NULL;
  for(p = ids; p != NIL; p = p->tail) {
    this_advisory = new_expr1(MANUAL_E, 
			      tagged_id_p(p->head.str, tag, line),
			      line);
    bump_list(this_advisory->EL2 = props);
    this_advisory->PRIMITIVE = 2;
    if(result == NULL) result = this_advisory;
    else result = apply_expr(this_advisory, result, line);
  }
  return result;
}


/****************************************************************
 *			HANDLE_LOOP_P				*
 ****************************************************************
 * Build a loop expression.  					*
 *								*
 *  d1 is the LOOP_TOK attribute,				*
 *  d5 is the expression that follows 'matching', if there is	*
 *        one,							*
 *  d7 is the expression representing the list of cases,	*
 *  d8 is then attribute of the end token that matches LOOP_TOK.*
 *								*
 * If the loop has a control pattern and initial value, then    *
 * those expressions are found in a MATCH_E expression on the   *
 * top of choose_st.						*
 ****************************************************************/

EXPR* handle_loop_p(struct lstr d1, EXPR *d5, EXPR *d7, struct lstr d8)
{
  EXPR *result;
  CHOOSE_INFO* inf = top_choose_info(choose_st);

  check_end_p(&(d1), LOOP_TOK, &(d8));
  check_for_cases(d7, d1.line);

  /*--------------------------------------------------------------------*
   * Build the loop.  It was already pushed onto loop_st, so that it	*
   * could be referred to at continues.  So just get it from the 	*
   * loop stack.							*
   *--------------------------------------------------------------------*/

  bump_expr(result = inf->loop_ref);
  bump_expr(result->E2 = possibly_apply(d5, d7, 0));
  result->LINE_NUM = d1.line;

  /*---------------------------------------------*
   * Tag the loop expression as a kind of choose *
   * expression.				 *
   *---------------------------------------------*/

  SET_EXPR(result, same_e(result, d1.line));
  result->SAME_MODE = 6;
  result->SAME_CLOSE = 1;

  /*------------------------------------------------*
   * If this is a choose-one-mixed expression, then *
   * wrap it inside a CutHere construct.	    *
   *------------------------------------------------*/

  if(inf->which == ONE_MIXED_ATT) {
    SET_EXPR(result, cuthere_expr(result));
  }

  /*---------------------------------------------*
   * Pop the stacks that were used to manage	 *
   * the loop expression.			 *
   *---------------------------------------------*/

  finish_choose();

  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			FINISH_CHOOSE				*
 ****************************************************************
 * Clean up at the end of a choose or loop expression.		*
 ****************************************************************/

void finish_choose(void)
{
  CHOOSE_INFO* inf = top_choose_info(choose_st);
  if(inf->match_kind) {
    LIST* pl = inf->working_choose_matching_list;
    if(pl != NIL) {
      add_list_to_choose_matching_lists_p(NULL, pl);
    }
  }
  free_choose_info(inf);
  pop(&choose_st);
  pop(&case_kind_st);
  pop_local_assumes_and_finger_tm();
}


/****************************************************************
 *			GET_DISCRIM_P				*
 ****************************************************************
 * Return the appropriate thing for a member of a class		*
 * union list, depending on the context in which it occurs.	*
 * Global variable union_context tells the context.		*
 *								*
 * A class union list is the right hand side of a species or	*
 * extension declaration.  What we are building is just one	*
 * part of that list.						*
 *								*
 * dis is the name of the constructor, or NULL if there is	*
 * none.							*
 *								*
 * ts is a pair of parallel lists.  It gives the types/roles 	*
 * that follow the constructor.					*
 *								*
 * withs is the information that follows the word 'with'.	*
 *								*
 * irr is true if this part is marked irregular.		*
 *								*
 * line is the line number where this element starts.		*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

CLASS_UNION_CELL* 
get_discrim_p(char *dis, RTLIST_PAIR ts, 
	      LIST *withs, int irr, int line, MODE_TYPE *mode)
{
  /*-----------------------------------------------*
   * A constructor is not allowed in a relate      *
   * declaration, except when it is by itself.     *
   *-----------------------------------------------*/

  if(union_context == RELATE_DCL_CX && dis != NULL &&
     (ts.types->tail != NIL || !is_hermit_type(ts.types->head.type))) {
    syntax_error(BAD_DISCRIM_ERR, line);
    return NULL;
  }

  else {
    CLASS_UNION_CELL* result   = allocate_cuc();
    result->name               = dis;
    result->special	       = (withs == NIL) ? irr : 1;
    bump_list(result->withs    = withs);
    result->line	       = line;
    result->mode               = copy_mode(mode);
    if(ts.types->tail != NULL) {
      result->tok                 = TYPE_LIST_TOK;
      bump_list(result->CUC_TYPES = ts.types);
      bump_list(result->CUC_ROLES = ts.roles);
    }
    else {
      result->tok                = TYPE_ID_TOK;
      bump_type(result->CUC_TYPE = ts.types->head.type);
      bump_role(result->CUC_ROLE = ts.roles->head.role);
    }
    return result;
  }
}


/****************************************************************
 *			GET_NAME_FROM_CUCS_P			*
 ****************************************************************
 * Parameter cucs is a list of class union cells.  It should    *
 * have only one, and that one should either be a genus id,     *
 * a community id or an unknown id.  Return the id.		*
 *								*
 * If cucs has the wrong form, return NULL.			*
 ****************************************************************/

char* get_name_from_cucs_p(LIST *cucs)
{
  if(cucs == NULL || cucs->tail != NULL) {
    return NULL;
  }
  else {
    CLASS_UNION_CELL* cuc = cucs->head.cuc;
    int tok = cuc->tok;

    if(tok == COMM_ID_TOK) return cuc->name;
    else if(tok == TYPE_ID_TOK) {
      TYPE* t = cuc->CUC_TYPE;
      if(TKIND(t) == WRAP_TYPE_T) return t->ctc->name;
      else if(is_hermit_type(t)) return cuc->name;
      else return NULL;
    }
    else return NULL;
  }
}


/****************************************************************
 *			PERFORM_MEET_P				*
 ****************************************************************
 * Process meet declaration A & B = rhs.			*
 ****************************************************************/

void perform_meet_p(char *A, char *B, LIST *rhs)
{
  if(rhs == NIL) {
    semantic_error(BAD_MEET_ERR5, 0);
  }
  else {
    char *c, *l1, *l2;
    CLASS_TABLE_CELL *a_ctc, *b_ctc;
    
    check_modes(RELATE_DCL_MODES, 0);
    c = rhs->head.str;
    if(rhs->tail == NIL) l1 = l2 = NULL;
    else {
      LIST* tl = rhs->tail;
      l1 = tl->head.str;
      l2 = (tl->tail == NIL) ? NULL : tl->tail->head.str;
    }
    a_ctc = get_ctc_tm(A);
    b_ctc = get_ctc_tm(B);
    
    if(has_mode(&this_mode, AHEAD_MODE)
       && (a_ctc == NULL || b_ctc == NULL)) {
      add_ahead_meet_tm(A, B, c, l1, l2);
    }
    else {
      add_intersection_from_strings_tm(a_ctc, b_ctc, c, l1, l2);
    }
  }
}


/****************************************************************
 *			FAM_CU_MEM_P				*
 ****************************************************************
 * Return the result of a classUnion that is the family or	*
 * community whose table entry is ctc.  Parameter line tells	*
 * the line that this family or community occurs on.		*
 *								*
 * If the context does not allow this, then print a warning and *
 * return NULL.							*
 ****************************************************************/

CLASS_UNION_CELL* fam_cu_mem_p(CLASS_TABLE_CELL *ctc, int line)
{
  CLASS_UNION_CELL *result;

  if(union_context != RELATE_DCL_CX) {
    syntax_error(BAD_CU_MEM_ERR, 0);
    err_print(BAD_MEM_IS_ERR);
    err_print(STR_ERR, ctc->name);
    return NULL;
  }

  else {
    result          = allocate_cuc();
    result->name    = ctc->name;
    result->tok     = MAKE_TOK(ctc->code);
    result->line    = line;
    return result;
  }
}


/****************************************************************
 *			ADD_CLASS_CONSTS			*
 ****************************************************************
 * A declaration 						*
 *								*
 *   Const{mode} x,y,z : T.					*
 *								*
 * is being processed, where list IDS is the list of x,y,z.     *
 *								*
 * Add expression nodes to class_consts, one for each		*
 * identifier in list IDS.  					*
 *								*
 * Mode mode is put into the SAME_E_DCL_MODE			*
 * field, type T is put in the ty field, and the identifier is  *
 * put in the STR field.  The expression has kind SAME_E, with  *
 * a null E1 field and SAME_MODE EXPECT_ATT.			*
 ****************************************************************/

void add_class_consts(STR_LIST *ids, RTYPE T)
{
  EXPR_LIST* p;
  TYPE *t;

  bump_type(t = copy_type(T.type, 4));

  for(p = ids; p != NIL; p = p->tail) {
    EXPR* node            = new_expr1t(SAME_E, NULL, t, current_line_number);
    node->SAME_E_DCL_MODE = copy_mode(&this_mode);
    node->STR             = p->head.str;
    bump_role(node->role  = T.role);
    SET_LIST(class_consts, expr_cons(node, class_consts));
  }
  drop_type(t);
}


/****************************************************************
 *			ADD_CLASS_EXPECTS			*
 ****************************************************************
 * Handle an expect or anticipate declaration in a class by	*
 * adding each to class_expects.  IDS is the list of		*
 * identifiers, RT gives the type and role, and descr it the 	*
 * description (or NULL if none).				*
 *								*
 * expect_context is either EXPECT_ATT (for an expect dcl) or   *
 * ANTICIPATE_ATT (for an anticipate dcl).			*
 ****************************************************************/

void add_class_expects(STR_LIST *ids, RTYPE rt, char *descr)
{
  STR_LIST *p;

  /*----------------------------------------*
   * Build the expect nodes and add them to *
   * class_expects.			    *
   *----------------------------------------*/

  for(p = ids; p != NIL; p = p->tail) {
    EXPR* node            = new_expr1t(SAME_E, NULL, rt.type, 
				       current_line_number);
    node->SAME_MODE       = expect_context;
    node->SAME_E_DCL_MODE = copy_mode(&this_mode);
    node->STR             = p->head.str;
    node->STR3            = descr;
    bump_role(node->role  = rt.role);
    SET_LIST(class_expects, expr_cons(node, class_expects));
  }
}


