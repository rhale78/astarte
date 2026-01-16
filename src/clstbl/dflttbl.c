/****************************************************************
 * File:    clstbl/dflttbl.c
 * Purpose: Functions for managing table of defaults for compiler.
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
 * This file contain functions that manage defaults for the class table *
 * in the compiler.							*
 ************************************************************************/

#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#include "../classes/classes.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/dflttbl.h"
#include "../error/error.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../generate/generate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			 DEFAULT_TM				*
 ****************************************************************
 * Install species or family t as a default for genus or	*
 * community c.  If t is NULL, then erase the default for c.	*
 *								*
 * MODE is the mode of the default declaration that does	*
 * this default.  It is a safe pointer: it does not live longer *
 * than this function call.					*
 ****************************************************************/

void default_tm(CLASS_TABLE_CELL *c, TYPE *t, MODE_TYPE *mode)
{
  /*-------------------------------------------------------*
   * Check that t contains no variables.  The default must *
   * be free of variables.				   *
   *-------------------------------------------------------*/

  if(t != NULL && find_var_t(t,NULL) != NULL) {
    semantic_error(BAD_VAR_ERR, 0);
  }

  /*--------------------------------------------------------------*
   * Defaults defined in the standard package (or in the preamble *
   * before standard.ast is started) are simply put into the      *
   * CTC_DEFAULT field of the class table entry.  Also, defaults  *
   * defined in the same extension as the one that defines the    *
   * genus or community are put into the CTC_DEFAULT field.	  *
   *--------------------------------------------------------------*/

  if(current_package_name == standard_package_name ||
     !c->closed) {
    SET_TYPE(c->CTC_DEFAULT, t);
    if(has_mode(mode, DANGEROUS_MODE)) c->dangerous = 1;
  }

  /*----------------------------------------------------------------*
   * Defaults defined in other packages are not necessarily visible *
   * to all packages.  They are installed in the file-info frame's  *
   * default table.  Insert into all frames that are currently      *
   * active, since all of them can see this default.		    *
   *								    *
   * Also write the default to the byte code if appropriate.	    *
   *----------------------------------------------------------------*/

  else { /* current_package_name != standard_package_name */
    IMPORT_STACK *p;
    HASH2_CELLPTR h;
    HASH_KEY u;
    LONG hash;

    /*------------------------------------------------------------------*
     * Key on c->num + 1, since ANY has number 0, and hash keys must 	*
     * not be 0. 							*
     *------------------------------------------------------------------*/

    u.num = (c == NULL) ? 1 : c->num + 1;
    hash = inthash(u.num);
    for(p = file_info_st; p != NULL; p = p->next) {

      h = insert_loc_hash2(&(p->default_table), u, hash, eq);
      if(h->key.num == 0) {
	h->key.num  = u.num;
	h->val.type = NULL;
      }

      /*--------------------------------------------------------*
       * If this default is dangerous, then indicate that by	*
       * setting the TOFFSET field of t to 1.  We need to have	*
       * a new copy of t to do that safely.			*
       *--------------------------------------------------------*/

      if(t != NULL && has_mode(mode, DANGEROUS_MODE)) {
	TYPE* new_t = new_type(0,NULL);
        *new_t = *t;
        new_t->ref_cnt = 1;
        new_t->TOFFSET = 1;
        t = new_t;
      }

      SET_TYPE(h->val.type, t);
    }

    /*----------------------------------------------------------*
     * Write the declaration into the byte code if appropriate. *
     *----------------------------------------------------------*/

    if(gen_code) {
      generate_default_dcl_g(c, t);
    }
  }
}


/****************************************************************
 *			 GET_DEFAULT_TM				*
 ****************************************************************
 * V is a variable that ranges over domain c.  If c is NULL, 	*
 * then it represents domain ANY.				*
 *								*
 * If variable V has kind TYPE_VAR_T or FAM_VAR_T or		*
 * PRIMARY_TYPE_VAR_T or PRIMARY_FAM_VAR_T, then return the	*
 * default for genus or community c, or NULL if there is none.  *
 * If c == NULL, then the default is for ANY.			*
 *								*
 * If variable V has kind WRAP_TYPE_VAR_T or WRAP_FAM_VAR_T, 	*
 * then return a WRAP_TYPE_T or WRAP_FAM_T X where X is		*
 * the domain of V.						*
 *								*
 * Set *dangerous = 0 if this is not a dangerous default, and   *
 * 1 if it is dangerous.					*
 *								*
 * V can be NULL.  If so, it is taken to be a variable `?.	*
 ****************************************************************/

TYPE* get_default_tm(TYPE *V, CLASS_TABLE_CELL *c, Boolean *dangerous)
{
  TYPE_TAG_TYPE V_kind = (V == NULL) ? TYPE_VAR_T : TKIND(V);

  /*--------------------------------------------*
   * Case of WRAP_TYPE_VAR_T or WRAP_FAM_VAR_T. *
   *--------------------------------------------*/

  if(IS_WRAP_VAR_T(V_kind)) {
    TYPE_TAG_TYPE new_kind = 
      (V_kind == WRAP_TYPE_VAR_T) ? WRAP_TYPE_T : WRAP_FAM_T;
    TYPE* t = new_type(new_kind, NULL);
    t->ctc = c;
    *dangerous = FALSE;
    return t;
  }

  /*------------------------------------------------------------*
   * Case of TYPE_VAR_T or FAM_VAR_T or PRIMARY_TYPE_VAR_T or 	*
   * PRIMARY_FAM_VAR_T.						*
   *------------------------------------------------------------*/

  else {
    HASH2_CELLPTR h;
    HASH_KEY u;

    if(c == NULL) c = ctcs[0];
    u.num = c->num + 1;
    h = locate_hash2(current_default_table, u, inthash(u.num), eq);
    if(h->key.num != 0) {
      TYPE* result = h->val.type;
      *dangerous = (Boolean) ((result == NULL) ? 0 : result->TOFFSET);
      return result;
    }
    else {
      *dangerous = c->dangerous;
      return c->CTC_DEFAULT;
    }
  }

}
