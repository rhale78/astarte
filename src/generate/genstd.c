/*********************************************************************
 * File:    generate/genstd.c
 * Author:  Karl Abrahamson
 * Purpose: Declare standard functions
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

/****************************************************************
 * When standard.aso is generated, the definitions of 		*
 * primitives are put in it, so that all primitives are		*
 * available as objects.  The same is done for other		*
 * standard functions that are defined via Import "-name", 	*
 * where name refers to a collection of primitives.		*
 *								*
 * This file contains the functions that generate the		*
 * primitive functions, and other kinds of things.		*
 ****************************************************************/

#include "../misc/misc.h"
#include "../lexer/modes.h"
#include "../parser/tokens.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/rdwrt.h"
#include "../classes/classes.h"
#include "../exprs/expr.h"
#include "../standard/stdtypes.h"
#include "../standard/stdfuns.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../infer/infer.h"  /* glob_bound_vars */
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 * 			GENERATING_STANDARD_FUNS		*
 ****************************************************************
 * generating_standard_funs is true during generation of	*
 * stanadrd functions.						*
 ****************************************************************/

Boolean generating_standard_funs = FALSE;


/****************************************************************
 * 			GEN_STANDARD_FUN			*
 ****************************************************************
 * Generate code for standard thing `name', of type t, 		*
 * computed by instruction instr, of kind prim. Typically,	*
 * name is a function, but it might not be a function. 		*
 * Expression e is name as an expression.			*
 *								*
 *--------------------------------------------------------------*
 * The code looks like this when the primitive is a function	*
 * primitive that is not a TY_... primitive.			*
 *								*
 *   GLABEL_I n			n is the next global label 	*
 *				that is available.		*
 *   ID_DCL_I "name"		name of this primitive.		*
 *   DEFINE_DCL_I n 0           0 is the declaration mode.	*
 *   t				The type of this primitive	*
 *   ENTER_I			End of type instructions	*
 *   0				No globals are needed, unless	*
 *				this is an irregular primitive. *
 *				Then there is take-apart code.  *
 *   STOP_G_I							*
 *   0				No locals needed by the code	*
 *				that creates the function.	*
 *   FUNCTION_I 0		Function extends to LLABEL 0.	*
 *   () type			No type information.		*
 *   END_LET_I			End of () type code.		*
 *   0				No locals needed by the		*
 *				function.			*
 *   PRIM			This is the primitive that does	*
 *				the function.			*
 *   LLABEL_I  0		End of function.		*
 *   INVISIBLE_RETURN_I						*
 *   END_I							*
 *								*
 *--------------------------------------------------------------*
 * The code looks like this when the primitive is a function	*
 * primitive that is a TY_... primitive.			*
 *								*
 *   GLABEL_I n			n is the next global label 	*
 *				that is available.		*
 *   ID_DCL_I "name"		name of this primitive.		*
 *   DEFINE_DCL_I n 0           0 is the declaration mode.	*
 *   t				The type of this primitive	*
 *   ENTER_I			End of type instructions	*
 *   1				A global is needed.		*
 *   (take-apart code)						*
 *   t				The type of this primitive 	*
 *				again.				*
 *   TYPE_ONLY_I		Get the type.			*
 *   STOP_G_I							*
 *   0				No locals needed by the code	*
 *				that creates the function.	*
 *   FUNCTION_I 0		Function extends to LLABEL 0.	*
 *   () type			No type information.		*
 *   END_LET_I			End of () type code.		*
 *   0				No locals needed by the 	*
 *				function.			*
 *   PRIM (0)			This is the primitive that does	*
 *				the function.  The type is in	*
 * 				global environment cell 0.	*
 *   LLABEL_I  0		End of function.		*
 *   INVISIBLE_RETURN_I						*
 *   END_I							*
 *								*
 *--------------------------------------------------------------*
 * For a non-function primitive, the code looks like this.	*
 *								*
 *   GLABEL_I n			n is the next global label 	*
 *				that is available.		*
 *   ID_DCL_I "name"		name of this primitive.		*
 *   DEFINE_DCL_I n 0           0 is the declaration mode.	*
 *   t				The type of this primitive	*
 *   ENTER_I			End of type instructions	*
 *   0				No globals are needed.		*
 *   STOP_G_I							*
 *   0				No locals needed.		*
 *   PRIM			Code to get the primitive.	*
 *   INVISIBLE_RETURN_I						*
 *   END_I							*
 ****************************************************************/

PRIVATE void 
gen_standard_fun(char *name, TYPE *t, int prim, int instr, EXPR *e,
		 MODE_TYPE *mode)
{
  package_index global_env_byte;

  int n = id_loc(name, ID_DCL_I);
  bind_glob_bound_vars(t, mode);

# ifdef DEBUG
   if(trace_gen > 1) {
     trace_t(222, name);
     trace_ty(t);
     tracenl();
   }
# endif

  /*--------------------------------------------------*
   * Prepare to generate code in the glob_code_array. *
   *--------------------------------------------------*/

  init_global_code_array();
  select_global_code_array();

  /*------------------*
   * Generate type t. *
   *------------------*/

  generate_type_g(t, 2);
  gen_g(ENTER_I);

  /*---------------------------------------------------------------------*
   * Take apart t if necessary, and generate the number of globals byte. *
   *---------------------------------------------------------------------*/

  global_env_byte = gen_g(0);  /* In glob_code_array. */
  if(FIRST_TY_PRIM <= prim && prim <= LAST_TY_PRIM) {
    num_globals = 0;
    take_apart_g(t, mode);
  }

  /*-----------------------------------------------------------------*
   * Prepare to generate the executable part in the exec_code array. *
   *-----------------------------------------------------------------*/

  init_exec_code_array();
  select_exec_code_array();

  /*----------------------------------------------*
   * Generate a function primitive as a function. *
   *----------------------------------------------*/

  if(prim <= LAST_FUN_PRIM) {
    gen_name_instr_g(ID_DCL_I, name);
    gen_g(FUNCTION_I);
    gen_g(0);
    gen_g(STAND_T_I);
    gen_g(hermit_type->ctc->std_num);
    gen_g(END_LET_I);
    gen_g(0);
    gen_name_instr_g(ID_DCL_I, name);
    genP_g(LINE_I, 0);
    gen_prim_g(prim, instr, e);
    gen_g(LLABEL_I);
    gen_g(0);
  }

  /*---------------------------------------------------*
   * Generate a non-function primitive in simple form. *
   *---------------------------------------------------*/

  else {
    gen_ent_prim_g(prim, instr);
  }

  /*-------------------------------------------------*
   * Put a return at the end of the executable code. *
   *-------------------------------------------------*/
  
  gen_g(INVISIBLE_RETURN_I);

  /*----------------------------------------------------------------*
   * Update the byte holding the index in env_size of the number of *
   * globals needed, and generate the global id instructions, if    *
   * necessary. 						    *
   *----------------------------------------------------------------*/

  select_global_code_array();
  if(FIRST_TY_PRIM <= prim && prim <= LAST_TY_PRIM) {
    glob_code_array[global_env_byte] = un_env_size_g(num_globals, FALSE);
    generate_globals_g();
  }
  gen_g(STOP_G_I);

  /*-----------*
   * Clean up. *
   *-----------*/

  clear_locs_t(t);
  SET_LIST(glob_bound_vars, NIL);
  SET_LIST(other_vars, NIL);

  /*------------------------*
   * Write the declaration. *
   *------------------------*/

  write_g(DEFINE_DCL_I);
  write_int_m(n, genf);				/* Label of name */
  write_g((int)(get_define_mode(mode) & 0xFF));	/* Mode */
  code_out_g(glob_code_array, &glob_genloc);
  write_g(0);					/* Local env byte */
  code_out_g(exec_code_array, &exec_genloc);
  write_g(END_I);
}


/****************************************************************
 * 			GEN_STANDARD_FUNS			*
 ****************************************************************
 * Generate code for the standard functions and other 		*
 * primitives that are in expression chain 'chain'.  chain      *
 * should be either &std_fun_descr or &import_prim_fun_descr.	*
 *								*
 * If we are not currently generating code, do nothing.		*
 ****************************************************************/

void gen_standard_funs(EXPR **chain)
{
  EXPR *p, *q, *r;

  if(genf == NULL) return;

  /*----------------------------------------------------------------*
   * *chain is a chain, linked through the E2 field, of expr 	    *
   * nodes that indicates primitives that need to be declared.      *
   * First reverse chain, so that the declarations will be made     *
   * in the same order here in which they were made in stdfuns.c.   *
   * (Each standard thing gets added to the front of the chain, so  *
   * we get the chain in reverse order.)  There is no need to	    *
   * modify the reference counts here.				    *
   *----------------------------------------------------------------*/

  q = NULL;
  p = *chain;
  while(p != NULL) {
    r = p->E2;
    p->E2 = q;
    q = p;
    p = r;
  }
  *chain =  q;

  /*---------------------------------------*
   * Now generate code for each primitive. *
   *---------------------------------------*/

  generating_standard_funs = TRUE;
  for(p = *chain; p != NULL_E; p = p->E2) {
    gen_standard_fun(p->STR3, p->ty, p->PRIMITIVE, p->LINE_NUM, p->E1,
		     p->SAME_E_DCL_MODE);
  }
  generating_standard_funs = FALSE;
}
