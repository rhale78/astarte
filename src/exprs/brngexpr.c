/*****************************************************************
 * File:    exprs/brngexpr.c
 * Purpose: Support for bring declarations and expressions
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
 * This file builds expressions for doing brings. 			*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#include "../unify/unify.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../exprs/expr.h"
#include "../exprs/brngexpr.h"
#include "../dcls/dcls.h"
#include "../infer/infer.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			MAKE_BRING				*
 ****************************************************************
 * Return expression						*
 *								*
 *   Let new_id:rt = cast(old: up(rt.type)) %Let		*
 *								*
 * where up(T) is cast_for_bring_t(T).				*
 *								*
 * If cast_for_bring_t indicates that this is impossible, then  *
 * return NULL.							*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

PRIVATE EXPR* 
make_bring(char *new_id, RTYPE rt, EXPR *from, int line, MODE_TYPE *mode)
{ 
  RTYPE bdyrt;
  EXPR *id, *bdy, *caster;

  bdyrt.type = cast_for_bring_t(rt.type, mode, line);
  if(bdyrt.type == NULL) {
    semantic_error1(CANNOT_BRING_ERR, new_id, 0);
    return NULL;
  }

  bdyrt.role = NULL;
  id         = tagged_id_p(new_id, rt, line);
  caster     = make_cast(bdyrt.type, rt.type, line);
  from       = same_e(from, from->LINE_NUM);
  bump_type(from->ty = bdyrt.type);
  bdy        = new_expr2(APPLY_E, caster, from, line);
  return new_expr2(LET_E, id, bdy, line);
}


/****************************************************************
 *			BRING_EXPRS				*
 ****************************************************************
 * Return a list of expressions that perform the individual     *
 * definitions indicated by					*
 *								*
 *  Bring ids: tag from fro.					*
 *								*
 * where tag is NULL if it is omitted, and from is NULL if it	*
 * is omitted.							*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

EXPR_LIST* 
bring_exprs(LIST *ids, RTYPE tag, EXPR *fro, int line, MODE_TYPE *mode)
{
  /*------------------------------------------*
   * You cannot have a from phrase with more  *
   * than one symbol in the id list to bring. *
   *------------------------------------------*/

  if(fro != NULL && list_length(ids) != 1) {
    syntax_error(BRING_WITH_LIST_EQUAL_ERR, 0);
    return NIL;
  }
  else {
    STR_LIST *p;
    EXPR_LIST *result;
    EXPR *from, *let;

    result = NIL;
    for(p = ids; p != NIL; p = p->tail) {
      from = (fro == NULL) ? id_expr(p->head.str, line) : fro;
      let  = make_bring(p->head.str, tag, from, line, mode);
      if(let != NULL) {
        result = expr_cons(let, result);
      }
    }
    return result;
  }
}


/****************************************************************
 *			MAKE_BRING_BY				*
 ****************************************************************
 * Return expression 						*
 *								*
 *    Let id:tag = (?.x => id(fun(.x))).			*
 *								*
 * at the given line number.					*
 ****************************************************************/

PRIVATE EXPR* make_bring_by(char *id, RTYPE tag, EXPR *fun, int line)
{
  EXPR *pat_x, *lhs_id_expr, *rhs_id_expr, *fun_rhs, *def_rhs;

  pat_x       = new_pat_var(HIDE_CHAR_STR "x", line);
  lhs_id_expr = typed_id_expr(id, tag.type, line);
  bump_role(lhs_id_expr->role = tag.role);
  rhs_id_expr = id_expr(id, line);
  fun_rhs     = apply_expr(rhs_id_expr,
			   apply_expr(fun, pat_x->E1, line),
			   line);
  def_rhs     = new_expr2(FUNCTION_E, pat_x, fun_rhs, line);
  return new_expr2(LET_E, lhs_id_expr, def_rhs, line);
}


/****************************************************************
 *			BRING_BY_EXPRS				*
 ****************************************************************
 * Return a list of the definitions indicated by		*
 *								*
 *  Bring ids: tag by fun.					*
 *								*
 * where tag is NULL if it is omitted.				*
 *--------------------------------------------------------------*/

EXPR_LIST* bring_by_exprs(LIST *ids, RTYPE tag, EXPR *fun, int line)
{
  STR_LIST *p;
  EXPR_LIST *result;

  result = NIL;
  for(p = ids; p != NULL; p = p->tail) {
    result = expr_cons(make_bring_by(p->head.str, tag, fun, line),
		       result);
  }
  return result;
}




