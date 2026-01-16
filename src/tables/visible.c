/****************************************************************
 * File:    tables/visible.c
 * Purpose: Visibility checking for translator global id table.
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
 * This file contains functions that perform visibility checks for	*
 * the global id table.  A node typically contains two fields that	*
 * influence its visibility: the package name and a list of package	*
 * names.  A pair (pname, l) represents a set of packages that can see  *
 * the node.								*
 *									*
 * If pname is not "standard", then pair (pname, l) represents the set  *
 * of all names in list l.  If pname is "standard", then pair (pname, l)*
 * represents the set of all package names that are NOT in l.  This is  *
 * done because nodes placed by the standard package are visible to	*
 * all packages, and we do not want to put them in the table again for	*
 * each package that is read.						*
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
#include "../patmatch/patmatch.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/************************************************************************
 *			IS_VISIBLE					*
 ************************************************************************
 * Return true if a cell with visibility list visible_in and package	*
 * dcl_package_name is visible to package req_package_name.		*
 * Both package names must be in the string table.			*
 *									*
 * Take into account that anything visible to the interface package is  *
 * also visible to the implementation packate.				*
 ************************************************************************/

Boolean is_visible(char *dcl_package_name, STR_LIST *visible_in,
		   char *req_package_name)
{
  /*----------------------------------------------------------------*
   * Note that package standard must be handled in the opposite way *
   * from other packages.					    *
   *----------------------------------------------------------------*/

  Boolean result = str_memq(req_package_name, visible_in);
  if(dcl_package_name == standard_package_name) return !result;
  else {
    if(result) return TRUE;
    if(main_package_imp_name != NULL && 
       req_package_name == main_package_imp_name) {
      return str_memq(main_package_name, visible_in);
    }
    else return FALSE;
  }
}


/************************************************************************
 *			IS_INVISIBLE					*
 ************************************************************************
 * Pair (name, v) represents a set of names (those in list v if name	* 
 * is anything but standard_package_name, and those not in v if name	*
 * is standard_package_name).  Return true just when the set		*
 * represented is the empty set.					*
 ************************************************************************/

Boolean is_invisible(char *name, STR_LIST *v)
{
  return name != standard_package_name && v == NIL;
}


/************************************************************************
 *			VISIBLE_EXPECTATION				*
 ************************************************************************
 * Return true if exp represents an expectation that is visible to	*
 * package package_name.						*
 ************************************************************************/

Boolean visible_expectation(char *package_name, EXPECTATION *exp)
{
  return is_visible(exp->package_name, exp->visible_in, package_name);
}


/************************************************************************
 *			VISIBLE_PART					*
 ************************************************************************
 * Return true if part represents a part that is visible to		*
 * package package_name.						*
 ************************************************************************/

Boolean visible_part(char *package_name, PART *part)
{
  return is_visible(part->package_name, part->visible_in, package_name);
}


/************************************************************************
 *			SOME_VISIBLE					*
 ************************************************************************
 * Return true if there is an entry in the chain exp that   		*
 * is visible to package package_name.					*
 ************************************************************************/

Boolean some_visible(char *package_name, EXPECTATION *exp)
{
  EXPECTATION *p;

  for(p = exp; p != NULL; p = p->next) {
    if(visible_expectation(package_name, p)) {
      return TRUE;
    }
  }
  return FALSE;
}


/****************************************************************
 *			IS_VISIBLE_GLOBAL_TM			*
 ****************************************************************
 * Return true if name is a global id that is visible in	*
 * package package_name.  If force is true, then		*
 * name does not need to be in the string table.  Otherwise, 	*
 * it does.							*
 ****************************************************************/

Boolean is_visible_global_tm(char *name, char *package_name, Boolean force)
{
  GLOBAL_ID_CELL* gic = get_gic_tm(name, force);

  if(gic == NULL) return FALSE;
  else {
    return some_visible(package_name, gic->expectations);
  }
}


/****************************************************************
 *			IS_VISIBLE_TYPED_GLOBAL_TM		*
 ****************************************************************
 * Return TRUE just when global id name is visible to package   *
 * package_name with a type that has a nonnull intersection	*
 * with type t.  If force is true, then name does not need to	*
 * be in the string table.  Otherwise, it does.			*
 ****************************************************************/

Boolean 
is_visible_typed_global_tm(char *name, char *package_name, TYPE *t,
			   Boolean force)
{
  GLOBAL_ID_CELL* gic = get_gic_tm(name, force);

  if(gic == NULL) return FALSE;
  else {
    EXPECTATION *p;
    TYPE *cpyt;

    bump_type(cpyt = copy_type(t, 0));
    for(p = gic->expectations; p != NULL; p = p->next) {
      if(visible_expectation(package_name, p) && 
	 UNIFY(p->type, cpyt, TRUE)) {
	drop_type(cpyt);
        return TRUE;
      } 
    }
    drop_type(cpyt);
    return FALSE;
  }
}


/************************************************************************
 *			VISIBLE_INTERSECT				*
 ************************************************************************
 * Return true just when the name sets represented by pairs 		*
 * (a_name, a_list) and (b_name, b_list) have a nonempty intersection.  *
 ************************************************************************/

Boolean visible_intersect(char *a_name, STR_LIST *a_list, 
			  char *b_name, STR_LIST *b_list)
{
  STR_LIST *p;

  if(a_name != standard_package_name) {
    for(p = a_list; p != NIL; p = p->tail) {
      if(is_visible(b_name, b_list, p->head.str)) return TRUE;
    }
    return FALSE;
  }

  else {
    if(b_name == standard_package_name) return TRUE;
    else {
      for(p = b_list; p != NIL; p = p->tail) {
        if(is_visible(a_name, a_list, p->head.str)) return TRUE;
      }
      return FALSE;
    }
  }
}


/****************************************************************
 *			REMOVE_VISIBILITY			*
 ****************************************************************
 * A pair (name, v) represents a set of names.  If name is      *
 * anything but standard_package_name, it represents the set    *
 * of all name in list v.  If name is standard_package_name,    *
 * it represents all names that are not in list v.		*
 *								*
 * If (*a_name, *a_list) represents set A and (b_name, b_list)  *
 * represents set B, then set *a_name and *a_list so that 	*
 * (*a_name, *a_list) represents the difference A - B.		*
 ****************************************************************/

void remove_visibility(char **a_name, STR_LIST **a_list,
		       char *b_name, STR_LIST *b_list)
{
  if(*a_name == standard_package_name) {

    /*------------------------------------------------------------*
     * If both a_name and b_name are standard_package_name, then  *
     * both sets are given in negative form.  The difference is	  *
     * given by list b_list - a_list, in positive form.  To force *
     * positive form, change a_name to ALT_STANDARD_AST_NAME.	  *
     *------------------------------------------------------------*/

    if(b_name == standard_package_name) {
      set_list(a_list, str_list_difference(b_list, *a_list));
      *a_name = alt_standard_package_name;
    }

    /*----------------------------------------------------------*
     * If a is in negative form and b is in positive form	*
     * then a-b is in negative form, and the list is the	*
     * union of a_list and b_list.		 		*
     *----------------------------------------------------------*/

    else {
      set_list(a_list, str_list_union(*a_list, b_list));
    }
  }

  else { /* *a_name != standard_package_name */

    /*----------------------------------------------------------*
     * If a is in positive form and b is in negative form	*
     * then a-b is in positive form, and the list is the	*
     * intersection of a_list and b_list.		 	*
     *----------------------------------------------------------*/

     if(b_name == standard_package_name) {
       set_list(a_list, str_list_intersect(*a_list, b_list));
     }

    /*----------------------------------------------------------*
     * If a is in positive form and b is in positive form	*
     * then a-b is in positive form, and the list is the	*
     * difference of a_list and b_list.		 		*
     *----------------------------------------------------------*/

     else {
       set_list(a_list, str_list_difference(*a_list, b_list));
     }
  }
}
