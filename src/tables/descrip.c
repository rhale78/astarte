/****************************************************************
 * File:    tables/descrip.c
 * Purpose: Handle descriptions in tables.
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
 * This file contains functions for handling descriptions, and putting  *
 * them in the global id table.  Each GLOBAL_ID_CELL cell has a 	*
 * DESCRIP_CHAIN chain that tells descriptions.				*
 *									*
 *===================================================================== *
 *			DESCRIP_CHAIN					*
 *									*
 * The cells in the description chain each give information about	*
 * descriptions that have been declared for this id.  Each cell		*
 * has the following fields.						*
 *									*
 * descr	The description.					*
 *									*
 * type		The polymorphic type over which this description 	*
 *		applies.						*
 *									*
 * mode		The mode of the description declaration.		*
 *									*
 * package_name	The name of the package where this description		*
 *		was made.						*
 *									*
 * visible_in	The visibility list, in the usual form.			*
 * 									*
 * line		The line where this description was issued.		*
 *									*
 * next		The next cell in the chain.				* 
 ************************************************************************/

#include <string.h>
#include <memory.h>
#include <ctype.h>
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
#include "../lexer/modes.h"
#include "../parser/parser.h"
#include "../generate/prim.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			SKIP_KEYWORD_PART			*
 ****************************************************************
 * Return a pointer to that part of string s that occurs after  *
 * any leading @{...} prefix, or to the beginning of s if s	*
 * has no such prefix.						*
 ****************************************************************/

PRIVATE char* skip_keyword_part(char *s)
{
  char *p;

  if(s[0] == '@' && s[1] == '{') {
    p = s+2;
    while(*p != 0 && *p != '}') p++;
    if(*p == '}') return p+1;
    else return p;
  }
  else return s;
}


/****************************************************************
 *			COMPARE_DESCRIPTIONS			*
 ****************************************************************
 * Return true if descriptions descr1 and descr2 are		*
 * compatible.  They are compatible if one is a (not		*
 * necessarily proper) prefix of the other, when white space    *
 * is ignored.  When a description includes a substring of the	*
 * form #{str}, checking stops unless both descriptions have    *
 * the same such substring, as the same place.  For example, if *
 * one starts "#{one}...", and the other starts "Once upon...", *
 * then the two descriptions are considered compatible since    *
 * checking stops at #{one}.  If one starts "#{one}...", and	*
 * the other starts "#{two}...", then the two are compabible.   *
 * If both start "#{one}...", then the parts after #{one} are   *
 * checked.							*
 *							 	*
 * Any leading string of the form @{...} is skipped.  Such a    *
 * string includes keywords.					*
 ****************************************************************/

Boolean compare_descriptions(char *descr1, char *descr2)
{
  char *p1, *p2;
  char c1, c2;

  /*------------------------------------------------------*
   * Move to the first nonblank character in each string. *
   *------------------------------------------------------*/

  p1 = skip_white(descr1);
  p2 = skip_white(descr2);

  /*------------------------------------------------------------*
   * Look for a leading string of the form @{...} containing	*
   * keywords.  Skip over it in each description.		*
   *------------------------------------------------------------*/

  p1 = skip_white(skip_keyword_part(p1));
  c1 = *p1;
  p2 = skip_white(skip_keyword_part(p2));
  c2 = *p2;

  /*------------------------------------------------------------*
   * Test for equality, with blank space ignored.  At 		*
   * #{str}, only continue to check if see the same #{str}	*
   * in each description.					*
   *------------------------------------------------------------*/

  while(c1 != '\0' && c2 != '\0') {

    /*-----------------------------*
     * Check for #{str} in descr1. *
     *-----------------------------*/

    if(c1 == '#' && p1[1] == '{') {

      /*---------------------------------------------------------*
       * If #{str} occurs in descr2, then we need to compare the *
       * strings in braces.					 *
       *---------------------------------------------------------*/

      if(c2 == '#' && p2[1] == '{') {
	p1 += 2;
	p2 += 2;
	c1 = *p1;
	c2 = *p2;
	while(c1 != 0 && c1 != '}' && c2 != 0 && c2 != '}' && c1 == c2) {
	  c1 = *(++p1);
	  c2 = *(++p2);
	}
	if(c1 == c2) continue;
	else return TRUE;
      }

      /*----------------------------------------------------------------*
       * If #{str} does not occur in descr2, but does in descr1, then   *
       * don't check further.						*
       *----------------------------------------------------------------*/

      else return TRUE;
    }

    /*------------------------------------------------------------------*
     * If #{str} occurs in descr2 but not in descr1, then don't check 	*
     * further.								*
     *------------------------------------------------------------------*/

    if(c2 == '#' && p2[1] == '{') return TRUE;

    /*------------------------------------------------------------------*
     * If neither string has #{str} at this point, then compare 	*
     * characters.  If they are equal, then go to the next character,   *
     * and skip over white space.					*
     *------------------------------------------------------------------*/

    if(c1 != c2) return FALSE;
    do {
      c1 = *(++p1);
    } while (isspace(c1));
    do {
      c2 = *(++p2);
    } while (isspace(c2));
  }

  /*---------------------------------------------------------------*
   * If we have reached the end of one of the strings, then one is *
   * a (not necessarily proper) prefix of the other.  So return    *
   * TRUE.							   *
   *---------------------------------------------------------------*/

  return TRUE;
}
       

/****************************************************************
 *			CHECK_TYPED_DESCRIPTION			*
 ****************************************************************
 * Check that description descr, with associated type		*
 * descr_type, is compatible with former description old_descr, *
 * which has type old_descr_type and package old_descr_package. *
 * If the descriptions are not compatible, then issue a 	*
 * warning.  name is the name of the identifier being described,*
 * and line is the line where description descr occurs.		*
 *								*
 * Note: If 
 ****************************************************************/

PRIVATE void 
check_typed_description(char *descr, TYPE *descr_type,
			char *descr_package,
			char *old_descr, TYPE *old_descr_type,
			char *old_descr_package, char *name,
			int line)
{
  LIST *mark;

  bump_list(mark = finger_new_binding_list());
  if(UNIFY(old_descr_type, descr_type, TRUE)) {
    undo_bindings_u(mark);
    if(!compare_descriptions(descr, old_descr)) {
      warn1(MAN_MISMATCH_ERR, display_name(name), line);
      err_print(MISMATCH_DESCR_PACKAGE_ERR, descr_package, old_descr_package);
    }
  }
  drop_list(mark);
}


/*===============================================================
 *			DESCRIPTION HANDLING
 *===============================================================*/

/****************************************************************
 *			INSTALL_DESCRIPTION_TM			*
 ****************************************************************
 * Install description descr, with type ty and visibility	*
 * who_sees, into global id cell gic.  Parameter name tells the	*
 * id that this description describes.				*
 ****************************************************************/

void install_description_tm(char *descr, TYPE *ty, STR_LIST *who_sees, 
		     	    GLOBAL_ID_CELL *gic, char *name, int line)
{
  DESCRIP_CHAIN *dc;

  /*---------------------------------------------------------*
   * Check the existing descriptions.  They must be	     *
   * consistent with this one.  If any of them subsume this  *
   * one, then we can ignore this description installation. *
   *---------------------------------------------------------*/

  for(dc = gic->descr_chain; dc != NULL; dc = dc->next) {
    if(visible_intersect(current_package_name, who_sees,
			 dc->package_name, dc->visible_in)) {
      int ov = half_overlap_u(ty, dc->type);
      if(ov != DISJOINT_OV) {

	/*-------------------------------------------------------*
	 * The new and old descriptions must be consistent.	 *
	 *-------------------------------------------------------*/

         check_typed_description(descr, ty, current_package_name,
				 dc->descr, dc->type, dc->package_name,
				 name, line);

	/*--------------------------------------------------------*
	 * Check whether node dc subsumes the current definition. *
	 * If so, add the visibility of who_sees to this cell.    *
	 * We don't bother with this test in the standard package.*
	 *--------------------------------------------------------*/

	if(current_package_name != standard_package_name &&
	   prefix(descr, dc->descr)) {

	  if(ov == EQUAL_OR_CONTAINED_IN_OV) {
	    if(dc->package_name == standard_package_name) return;
	    if(str_list_subset(who_sees, dc->visible_in)) return;
	  }

        }
      } /* end if(ov != DISJOINT_OV) */
    } /* end if(visible_intersect...) */
  } /* end for(dc = ...) */

  /*---------------------------*
   * Install this description. *
   *---------------------------*/

  dc                       = allocate_descrip_chain();
  dc->descr 		   = descr;
  bump_type(dc->type       = ty);
  bump_list(dc->visible_in = who_sees);
  dc->package_name         = current_package_name;
  dc->line 		   = line;
  dc->mode		   = NULL;
  dc->next 		   = gic->descr_chain;
  gic->descr_chain 	   = dc;
}


/*==============================================================*
 *			AHEAD DESCRIPTIONS			*
 *==============================================================*/

/****************************************************************
 *		        INSERT_AHEAD_DESCRIPTION_TM		*
 ****************************************************************
 * Do declaration Description{ahead} id:t descr %Description.   *
 *								*
 * MODE is a safe pointer.  It only lives as long as this	*
 * function call.						*
 ****************************************************************/

void insert_ahead_description_tm(char *id, char *descr, TYPE *t, 
				 MODE_TYPE *mode)
{
  HASH_KEY u;
  HASH2_CELLPTR h;
  LONG hash;
  DESCRIP_CHAIN* dc = allocate_descrip_chain();

  dc->descr         = descr;
  dc->package_name  = current_package_name;
  dc->type          = t;
  dc->mode          = copy_mode(mode);

  hash = strhash(id);
  u.str = id_tb10(id, hash);
  h = insert_loc_hash2(&current_ahead_descr_table, u, hash, eq);
  if(h->key.num == 0) {
    h->key.str         = u.str;
    h->val.descr_chain = NULL;
  }
  dc->next           = h->val.descr_chain;
  h->val.descr_chain = dc;
}


/****************************************************************
 *		        AHEAD_DESCRIPTION_TM			*
 ****************************************************************
 * Return the current ahead description chain for id,		*
 * or NULL if there is none.					*
 ****************************************************************/

PRIVATE DESCRIP_CHAIN * ahead_description_tm(char *id)
{
  HASH_KEY u;
  HASH2_CELLPTR h;

  u.str = id_tb0(id);
  h     = locate_hash2(current_ahead_descr_table, u, strhash(u.str), eq);
  if(h->key.num == 0) return NULL;
  return h->val.descr_chain;
}


/****************************************************************
 *		        DO_AHEAD_DESCRIPTION_TM			*
 ****************************************************************
 * Issue a description for identifier id, of type ty, if an	*
 * ahead description for id has been given in the current	*
 * package.							*
 ****************************************************************/

void do_ahead_description_tm(char *id, TYPE *ty)
{
  DESCRIP_CHAIN* descr_chain = ahead_description_tm(id);

  if(descr_chain != NULL) {
    DESCRIP_CHAIN *p;

    /*------------------------------------------*
     * Try each ahead description in the chain. *
     *------------------------------------------*/

    for(p = descr_chain; p != NULL; p = p->next) {
      LIST* mark = finger_new_binding_list();

      /*------------------------------------------------------------*
       * If this ahead description has a type that unifies with ty, *
       * then issue the description for the unified types.          *
       *------------------------------------------------------------*/

      if(UNIFY(ty, p->type, TRUE)) {
	EXPR* man_dcl;
	EXPR* id_as_expr = new_expr1(IDENTIFIER_E, NULL, current_line_number);
	id_as_expr->STR  = id;

        /*-----------------------------------------------*
         * This ahead description matches, so it must be *
         * issued now.					 *
         *-----------------------------------------------*/

	bump_expr(man_dcl = new_expr1(MANUAL_E, id_as_expr, 
				      current_line_number));
	man_dcl->STR          = p->descr;
	bump_type(man_dcl->ty = ty);
	issue_description_p(man_dcl, p->mode);
	drop_expr(man_dcl);
	undo_bindings_u(mark);
      } /* end if(unify...) */

    } /* end for(p = ...) */

  } /* end if(descr_chain != NULL) */
}
