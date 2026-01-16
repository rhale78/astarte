/***************************************************************
 * File:    tables/primtbl.c
 * Purpose: Functions for declaring primitives.
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


/****************************************************************
 * These functions are used in standard/stdfuns.c to declare 	*
 * primitives.  Largely, they are special cases of the more     *
 * general functions in other files that are a little faster.   *
 * That speeds up stdfuns.c.  Also, when a primitive is 	*
 * declared, it must be put into the list of primitives to be	*
 * generated in generate/genprim.c.				*
 ****************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../lexer/modes.h"
#include "../parser/parser.h"
#include "../classes/classes.h"
#include "../standard/stdfuns.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../dcls/dcls.h"
#include "../machdata/except.h"
#include "../generate/prim.h"
#include "../evaluate/instruc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *		       PUBLIC VARIABLES				*
 ****************************************************************/

/****************************************************************
 *			no_gen_prim				*
 ****************************************************************
 * Normally, when a primitive is declared, an entry is made for *
 * it in the std_fun_descr chain.  If no_gen_prim is true, then *
 * no such entry is made.					*
 ****************************************************************/

Boolean no_gen_prim = FALSE;

/****************************************************************
 *			BASIC_MODE_PRIMITIVE_TM			*
 ****************************************************************
 * Declare id:t, with role r, as a primitive.  			*
 *								*
 * kind is the primitive kind (such as PRIM_FUN), and instr	*
 * is the argument of the primitive kind.  			*
 *								*
 * mode is the declaration mode.        			*
 *								*
 * std_prim is TRUE if this definition is being made in 	*
 * standard.ast, and is FALSE if this definition is being made  *
 * in another package.						*
 ****************************************************************/

PRIVATE void 
basic_mode_primitive_tm(char *id, TYPE *t, ROLE *r,
			int kind, int instr, MODE_TYPE *mode, Boolean std_prim)
{
  register EXPR *id_as_expr;
  EXPR **fun_descr;

  /*-----------------------------*
   * Build the id to be defined. *
   *-----------------------------*/

  bmp_expr(id_as_expr	    = new_expr1t(GLOBAL_ID_E, NULL_E, t, 0));
  id_as_expr->STR 	    = id;
  id_as_expr->PRIMITIVE     = kind;
  id_as_expr->SCOPE 	    = instr;
  if(has_mode(mode, IRREGULAR_MODE)) id_as_expr->irregular = 1;
  bump_role(id_as_expr->role = r);

  /*--------------------------------------------------*
   * Indicate whether should trap standard exception. *
   *--------------------------------------------------*/

  if((kind == STD_CONST && t == exception_type && instr != TEST_EX
     && instr != SPECIES_EX) || 
     (kind == PRIM_QWRAP)) id_as_expr->bound = 1;

  /*--------------------------*
   * Add the id to the table. *
   *--------------------------*/

  if(std_prim) {
    MODE_TYPE *newmode = copy_mode(mode);
    add_mode(newmode, PRIMITIVE_MODE);
    define_prim_id_tm(id_as_expr, newmode);
    drop_mode(newmode);
  }
  else {
    quick_issue_define_p(id_as_expr, id, mode, 0);
  }

  /*--------------------------------------------------------------------*
   * If we are compiling standard.asi, and std_prim is true, then we 	*
   * need to add this id to std_fun_descr, so that a definition of it	*
   * will be put in standard.aso.  Similarly, if std_prim is false and  *
   * we are doing an export, then we must be compiling the package      *
   * that defines this primitive, so we must add it to 			*
   * import_prim_fun_descr so that it will be generated into the .aso	*
   * file.								*
   *--------------------------------------------------------------------*/

  if(kind <= LAST_ENT_PRIM && !no_gen_prim) {
    fun_descr = 
      (compiling_standard_asi && std_prim)   
	 ? &std_fun_descr :
      (!compiling_standard_asi && !std_prim && main_context == EXPORT_CX) 
	  ? &import_prim_fun_descr 
	  : NULL;

    if(fun_descr != NULL) {
      set_expr(fun_descr, new_expr2(PAIR_E, id_as_expr, *fun_descr, instr));
      (*fun_descr)->PRIMITIVE    = kind;
      (*fun_descr)->STR3         = id_as_expr->STR;
      bump_mode((*fun_descr)->SAME_E_DCL_MODE = mode);
      bump_type((*fun_descr)->ty = t);
    }
  }
  drop_expr(id_as_expr);
}


/****************************************************************
 *			MODE_PRIMITIVE_TM			*
 ****************************************************************
 * Declare id:t, with role r, to be a primitive of primitive    *
 * kind 'kind' (PRIM_FUN, for example), and with additional     *
 * information instr.  mode is the declaration mode for this    *
 * primitive.							*
 *								*
 * Note: This should only be called when the current package    *
 * name is "standard".  It uses a null visibility list.		*
 ****************************************************************/

void mode_primitive_tm(char *id, TYPE *t, ROLE *r,
		       int kind, int instr, MODE_TYPE *mode)
{
  basic_mode_primitive_tm(stat_id_tb(id), t, r, kind, instr, mode, 1);
}


/****************************************************************
 *			SYM_MODE_PRIM_TM			*
 *			IMPORT_SYM_MODE_PRIM_TM			*
 ****************************************************************
 * Declare std_id[sym]:t, with role r, to be a primitive of	*
 * primitive kind 'kind' (PRIM_FUN, for example), and with	*
 * additional information instr.  mode is the declaration mode	*
 * for this primitive.						*
 *								*
 * Note: sym_mode_prim_tm should only be called when the	*
 * current package  name is "standard".  It uses a null		*
 * visibility list.						*
 *								*
 * import_sym_mode_prim_tm should be used only for a package 	*
 * other than "standard".					*
 ****************************************************************/

void sym_mode_prim_tm(int sym, TYPE *t, ROLE *r,
		      int kind, int instr, MODE_TYPE *mode)
{
  basic_mode_primitive_tm(std_id[sym], t, r, kind, instr, mode, 1);
}

/*--------------------------------------------------------------*/

void import_sym_mode_prim_tm(int sym, TYPE *t, ROLE *r, int kind,
			     int instr, MODE_TYPE *mode)
{
  basic_mode_primitive_tm(std_id[sym], t, r, kind, instr, mode, 0);
}


/****************************************************************
 *			PRIMITIVE_TM				*
 ****************************************************************
 * Declare id:t to be a primitive of primitive kind 'kind' 	*
 * (PRIM_FUN, for example), and with additional information	*
 * instr. 							*
 *								*
 * Note: This should only be called when the current package    *
 * name is "standard".  It uses a null visibility list.		*
 ****************************************************************/

void primitive_tm(char *id, TYPE *t, int kind, int instr)
{
  mode_primitive_tm(id, t, NULL, kind, instr, NULL);
}


/****************************************************************
 *			SYM_PRIM_TM				*
 *			IMPORT_SYM_PRIM_TM			*
 ****************************************************************
 * Declare std_id[sym]:t to be a primitive of primitive		*
 * kind 'kind' (PRIM_FUN, for example), and with additional	*
 * information instr.  The mode is NULL - no special mode 	*
 * information.							*
 *								*
 * Note: sym_prim_tm should only be called when the current	*
 * package name is "standard".  It uses a null visibility list.	*
 *								*
 * import_sym_prim_tm should only be used for a package other	*
 * than "standard".						*
 ****************************************************************/

void sym_prim_tm(int sym, TYPE *t, int kind, int instr)
{
  basic_mode_primitive_tm(std_id[sym], t, NULL, kind, instr, NULL, 1);
}

/*--------------------------------------------------------------*/

void import_sym_prim_tm(int sym, TYPE *t, int kind, int instr)
{
  basic_mode_primitive_tm(std_id[sym], t, NULL, kind, instr, 0, 0);
}


/****************************************************************
 *			EXPECT_SYM_PRIM_TM			*
 ****************************************************************
 * Expect primitive name, of class t.				*
 * This expectation or anticipation 				*
 *								*
 *   should not be noted    if antic = 0			*
 *   is an expectation      if antic = EXPECT_ATT		*
 *   is an anticipation     if antic = ANTICIPAT_ATT	 	*
 *								*
 * Note: This should only be called when the current package    *
 * name is "standard".  It uses a null visibility list.		*
 ****************************************************************/

void expect_sym_prim_tm(int sym, TYPE *t, MODE_TYPE *mode, int antic)
{
  char* name = std_id[sym];
  LIST *exps;

  bump_type(t);
  bump_list(exps = expect_global_id_tm(name, t, NULL, NIL, FALSE, FALSE, 
				       FALSE, FALSE, mode, 
				       current_package_name, 0, NULL));
  drop_list(exps);
  if(antic != 0) {
    note_expectation_tm(name, t, NIL, antic, 0,
		        current_package_name, current_line_number);
  }
  drop_type(t);
}


/****************************************************************
 *			PRIM_CONST_TM				*
 ****************************************************************
 * Declare name:t to be a primitive constant with value val.	*
 *								*
 * Note: This should only be called when the current package    *
 * name is "standard".  It uses a null visibility list.		*
 ****************************************************************/

void prim_const_tm(char *name, TYPE *t, int val)
{
  if(val == 0) primitive_tm(name, t, PRIM_CONST, ZERO_I);
  else primitive_tm(name, t, STD_CONST, val);
}


/****************************************************************
 *			SYM_PRIM_CONST_TM			*
 ****************************************************************
 * Declare std_id[sym]:t to be a primitive constant with value	*
 * val.								*
 *								*
 * Note: This should only be called when the current package    *
 * name is "standard".  It uses a null visibility list.		*
 ****************************************************************/

void sym_prim_const_tm(int sym, TYPE *t, int val)
{
  prim_const_tm(std_id[sym], t, val);
}


/****************************************************************
 *			PRIM_EXCEPTION_CONST_TM			*
 ****************************************************************
 * Declare an exception called std_id[sym], with number prim,	*
 * having no additional information.				*
 *								*
 * Note: This should only be called when the current package    *
 * name is "standard".  It uses a null visibility list.		*
 ****************************************************************/

void prim_exception_const_tm(int sym, int prim)
{
   char* name       = std_id[sym];
   char* testr_name = tester_name(name);
   char* role_name  = concat_id("act__fail__", name);

   prim_const_tm(name, exception_type, prim);
   attach_property(role_name, name, exception_type);
   primitive_tm (testr_name, exc_to_bool_type,  PRIM_QTEST, prim);
   auto_pat_const_p(name,exception_type,testr_name,exc_to_bool_type,FALSE);
}


/****************************************************************
 *			PRIM_EXCEPTION_FUN_TM			*
 ****************************************************************
 * Declare an exception named std_id[sym], with number prim, 	*
 * having additional data of type [Char].			*
 *								*
 * Note: This should only be called when the current package    *
 * name is "standard".  It uses a null visibility list.		*
 ****************************************************************/

void prim_exception_fun_tm(int sym, int prim)
{
  char* name        = std_id[sym];
  char* testr_name  = tester_name(name);
  char* destr_name  = destructor_name(name);
  char* role_name   = concat_id("pot__fail__", name);

  primitive_tm(name, str_to_exc_type, PRIM_QWRAP, prim);
  attach_property(role_name, name, str_to_exc_type); 
  primitive_tm (testr_name, exc_to_bool_type, PRIM_QTEST, prim);
  primitive_tm(destr_name, exc_to_str_type, PRIM_QUNWRAP, prim);
  basic_pat_fun_p(name, str_to_exc_type, destr_name, exc_to_str_type, FALSE);
}


/****************************************************************
 *			PRIM_BOX_TM				*
 ****************************************************************
 * Declare name:t to be a primitive box whose box number is 	*
 * val.								*
 *								*
 * Note: This should only be called when the current package    *
 * name is "standard".  It uses a null visibility list.		*
 ****************************************************************/

void prim_box_tm(char *name, TYPE *t, int val)
{
  primitive_tm(name, t, STD_BOX, val);
}


/****************************************************************
 *			SYM_PRIM_BOX_TM				*
 ****************************************************************
 * Declare std_id[sym]:t to be a primitive box whose box	*
 * number is val.						*
 *								*
 * Note: This should only be called when the current package    *
 * name is "standard".  It uses a null visibility list.		*
 ****************************************************************/

void sym_prim_box_tm(int sym, TYPE *t, int val)
{
  sym_prim_tm(sym, t, STD_BOX, val);
}

