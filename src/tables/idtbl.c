/****************************************************************
 * File:    tables/idtbl.c
 * Purpose: Translator table manager for ids.  The functions
 *          here handle both global and local ids.
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
 * This file manages both global and local ids.  The specifics for	*
 * handling local ids is in loctbl.c, and the specifics for global	*
 * ids is in globtbl.c.							*
 ************************************************************************/

#include <string.h>
#include <memory.h>
#include "../misc/misc.h"
#include "../tables/tables.h"
#include "../dcls/dcls.h"
#include "../exprs/expr.h"
#include "../exprs/prtexpr.h"
#include "../classes/classes.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../unify/unify.h"
#include "../error/error.h"
#include "../parser/parser.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			unknowns_ok				*
 ****************************************************************
 * True if old_id_tm should not complain on an unknown id.	*
 ****************************************************************/

Boolean unknowns_ok = FALSE;


/************************************************************************
 *			SINGLE_ENTRY					*
 ************************************************************************
 * Check the chain starting at exp to see if there is only one entry	*
 * that is visible to package package_name, and whose type is compatible*
 * with type t.  If so, then return a pointer to that entry.		*
 * Otherwise, return NULL.						*
 ************************************************************************/

PRIVATE EXPECTATION *
single_entry(char *package_name, EXPECTATION *exp, TYPE *t)
{
  EXPECTATION* g;
  EXPECTATION* the_entry = NULL;

  for(g = exp; g != NULL; g = g->next) {
    if(visible_expectation(package_name, g) &&
       (t == NULL || !disjoint(t, g->type))) {

      if(the_entry != NULL) return NULL;
      else the_entry = g;
    }
  }
  return the_entry;
}


/****************************************************************
 *			CP_OLD_ID_TM				*
 ****************************************************************
 * Same as old_id_tm, but forces the generated id to have the 	*
 * same line number as id, by putting a SAME_E node above it if *
 * necessary.							*
 *								*
 * If id is a local id, then a SAME_E node is always added.	*
 ****************************************************************/

EXPR* cp_old_id_tm(EXPR *id, int outer)
{
  return  zsame_e(old_id_tm(id, outer), id->LINE_NUM);
}


/****************************************************************
 *			OLD_ID_TM				*
 ****************************************************************
 * old_id_tm(id_orig, outer) is used to look up a previously	*
 * defined identifier.  It returns a unique expr node for	*
 * identifier id_orig, by looking in the local and global	*
 * environments.  If outer is 1,  then the local environment is *
 * not looked at, and an outer (global) id is sought.  If 	*
 * outer is 2, get a global id if id_orig is a TEAM_DCL_NOTE id.*
 *								*
 * The following are the cases.					*
 *								*
 *   (1) If id_orig has a nonnull type, then a SAME_E expression*
 *   of the following is returned.  Otherwise, the		*
 *   expression described by the following cases is		*
 *   returned.							*
 *								*
 *   (2) If id_orig's name has not been seen, then an unknown 	*
 *   identifier is returned.					*
 *								*
 *   (3) If id_orig is a local id, and outer = FALSE, then a	*
 *   local id expr for id_orig is returned.			*
 *								*
 *   (4) If id_orig is a global id, or if id_orig is both local	*
 *   and global and outer = TRUE, then 				*
 *								*
 *     (a) If id_orig is not overloaded, a global id expr for	*
 *         id_orig is returned.  The type in the returned	*
 *         expr is a copy of the type in the table.		*
 *								*
 *     (b) If id_orig is overloaded, an overload expr for 	*
 *         id_orig is returned.					*
 ****************************************************************/

EXPR* old_id_tm(EXPR *id_orig, int outer)
{
  EXPR *result, *id;
  GLOBAL_ID_CELL *gic;
  EXPECTATION *one_exp;
  char *name;
  int line;
  EXPR_TAG_TYPE id_kind;

  bump_expr(id_orig);
  bump_expr(id = skip_sames(id_orig));
  name   = id->STR;
  line   = id_orig->LINE_NUM;
  id_kind = EKIND(id);

  /*-----------------------*
   * Check for a local id. *
   *-----------------------*/

  if(outer != 1 && id_kind != GLOBAL_ID_E && id_kind != OVERLOAD_E && 
     id_kind != PAT_FUN_E) {

#   ifdef DEBUG
      if(trace_locals) {
	trace_t(342, name, name);
        print_local_env();
      }
#   endif

    bump_expr(result = get_local_id_tm(name));
    if(result != NULL) {
      if(outer != 2 || result->TEAM_FORM != TEAM_DCL_NOTE) goto out;
    }
  }

  /*------------------------*
   * Check for a global id. *
   *------------------------*/

   gic = get_gic_tm(name, TRUE);

   /*------------------------------------------------------------*
    * If the identifier is unknown, or if no expectation is	 *
    * visible to this package, return an unknown id expression.  *
    * Report an error if not in a context where unknown ids are  *
    * expected.							 *
    *------------------------------------------------------------*/

   if(gic == NULL ||
      (!some_visible(current_package_name, gic->expectations))) {

     if(!unknowns_ok) {
       unknown_id_error(UNKNOWN_ID_ERR, display_name(name), line);
     }

#    ifdef DEBUG
       if(trace) trace_t(343, name);
#    endif

     bump_expr(result = new_expr1(UNKNOWN_ID_E, NULL_E, line));
     goto out;
   }

  /*-----------------------------------------------------*
   * Return a global id or pattern function or expander. *
   *-----------------------------------------------------*/

  one_exp = single_entry(current_package_name, gic->expectations, id->ty);
  if(one_exp != NULL) {

    /*----------------------------*
     * Only one entry, so get it. *
     *----------------------------*/

    bump_expr(result = 
	      new_expr1t(GLOBAL_ID_E, NULL_E, 
			 copy_type(one_exp->type, 0), line));
    bump_role(result->role = get_role_tm(result, NULL));
    result->irregular = one_exp->irregular;
  }

  else { /* one_exp == NULL */

    /*-------------------------------------*
     * More than one entry, or no entries. *
     *-------------------------------------*/

    bump_expr(result = 
	      new_expr1t(OVERLOAD_E, NULL_E, copy_type(gic->container, 0), 
			line));
  }

  result->GIC = gic;
  result->PRIMITIVE = -1;
  if(id_kind == PAT_FUN_E || id->PRIMITIVE == PAT_FUN) {
    result->PRIMITIVE = PAT_FUN;
  }
  else if(id->PRIMITIVE == PAT_CONST) {
    result->PRIMITIVE = PAT_CONST;
  }

  if(id->MARKED_PF) {
    result->MARKED_PF = 1;
    result->pat = 1;
  }

out:

  /*--------------------------------------------------------------------*
   * Place the name in the result, possibly add type of id, and return. *
   *--------------------------------------------------------------------*/

  result->STR = name;
  {EXPR* ee = id_orig;
   while(EKIND(ee) == SAME_E) {
     if(ee->ty != NULL) {
       SET_EXPR(result, new_expr1t(SAME_E, result, ee->ty, line));
     }
     ee = ee->E1;
   }
   if(ee->ty != NULL) {
     SET_EXPR(result, new_expr1t(SAME_E, result, ee->ty, line));
   }
  }
  drop_expr(id);
  drop_expr(id_orig);

# ifdef DEBUG
    if(trace_ids) {
      id_kind = EKIND(skip_sames(result));
      if(id_kind == LOCAL_ID_E) {
	trace_t(344, name);
      }
      else if(id_kind == UNKNOWN_ID_E) {
	trace_t(345, name);
      }
      else {
        trace_t(346, name, id_kind);
        if(id_kind == GLOBAL_ID_E) {
          trace_t(443);
          trace_ty(result->ty);
	  tracenl();
        }
        else if(id_kind == OVERLOAD_E) {
          trace_t(444);
        }
      }
    }
# endif

  if(result != NULL) result->ref_cnt--;
  return result;
}


