/****************************************************************
 * File:    clstbl/classinfo.c
 * Purpose: Table manager for classes.
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
 * This file manages a table giving information about classes.		*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../exprs/expr.h"
#include "../error/error.h"
#include "../parser/parser.h"
#include "../clstbl/classtbl.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 * 			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 * 			class_info_table			*
 ****************************************************************
 * class_info_table holds information for classes.  Associated  *
 * with each class C is the parameter that should be passed to  *
 * the raw constructor constructC! when a constructor is	*
 * created.							*
 *								*
 * If the constants are a, b and c (in that order),		*
 * and the superclass is not Object)then the parameter		*
 * is (c,b,a,super) where super is an identifier.		*
 *								*
 * If the superclass is Object, then the parameter for the      *
 * raw constructor is (c,b,a,object!()).			*
 ****************************************************************/

PRIVATE HASH2_TABLE* class_info_table = NULL;


/****************************************************************
 *			REMEMBER_CLASS_INFO			*
 ****************************************************************
 * Record the class_consts lists for class C in			*
 * class_info_table.						*
 ****************************************************************/

void remember_class_info(char *C, char *superclass, int line)
{
  HASH_KEY u;
  HASH2_CELLPTR h;

  u.str = new_name(C, FALSE);
  h = insert_loc_hash2(&class_info_table, u, strhash(u.str), eq);
  if(h->key.num == 0) {
    h->key.str = u.str;
    bump_expr(h->val.expr = 
	      make_class_constructor_param(class_consts, superclass, line));
  }
}


/****************************************************************
 *			RETRIEVE_CLASS_INFO			*
 ****************************************************************
 * Return the class_consts lists for class C, or NIL if there   *
 * is none.							*
 *								*
 * If there is no entry, and complain is true, then issue an	*
 * error message.						*
 ****************************************************************/

EXPR* retrieve_class_info(char *C, Boolean complain)
{
  HASH_KEY u;
  HASH2_CELLPTR h;

  u.str = new_name(C, FALSE);
  h = locate_hash2(class_info_table, u, strhash(u.str), eq);
  if(h->key.num != 0) {
    return h->val.expr;
  }
  else {
    if(complain) {
      semantic_error1(NOT_CLASS_ERR, quick_display_name(u.str), 0);
    }
    return hermit_expr;
  }
}

