/******************************************************************
 * File:    generate/genexec.c
 * Purpose: generate executable code
 * Author:  Karl Abrahamson
 ******************************************************************/

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

/********************************************************
 * This file holds the main code generation functions.  *
 * The functions here generate the executable part of	*
 * the byte-code.					*
 ********************************************************/

#include <string.h>
#include "../misc/misc.h"
#ifdef SMALL_STACK
#  ifdef USE_MALLOC_H
#    include <malloc.h>
#  else
#    include <stdlib.h>
#  endif
#endif
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/hash.h"
#include "../evaluate/instruc.h"
#include "../error/error.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../tables/tables.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../generate/selmod.h"
#include "../patmatch/patmatch.h"
#include "../unify/unify.h"
#include "../machdata/except.h"
#include "../parser/parser.h"
#include "../dcls/dcls.h"
#include "../infer/infer.h"
#include "../ids/ids.h"
#include "../machstrc/machstrc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE void 	  generate_local_id_g (EXPR *e, Boolean final);
PRIVATE void      gen_sequence_g      (EXPR *e, Boolean tro);
PRIVATE void      gen_lazy_g          (EXPR *e, Boolean tro, int line);
PRIVATE void      gen_lazy_list_g     (EXPR *e);
PRIVATE void      gen_single_g        (EXPR *e, Boolean tro);
PRIVATE void      gen_test_g          (EXPR *e);
PRIVATE void      gen_stream_g	      (EXPR *e, Boolean tro);
PRIVATE void 	  gen_const_g         (EXPR *e);
PRIVATE void 	  gen_define_g        (EXPR *e);
PRIVATE void 	  gen_letexpr_g       (EXPR *e);
PRIVATE void      gen_pair_g          (EXPR *e);
PRIVATE void      gen_trap_g          (EXPR *e);
PRIVATE void      gen_if_try_g        (EXPR *e, Boolean tro);
PRIVATE void      gen_lazy_bool_g     (EXPR *e, Boolean tro);
PRIVATE void 	  gen_function_g      (EXPR *e);
PRIVATE void      gen_same_g          (EXPR *e, Boolean tro, int line);
PRIVATE void      gen_for_g           (EXPR *e);
PRIVATE void      gen_loop_g          (EXPR *e, Boolean tro);
PRIVATE void      gen_recur_g         (EXPR *e);
PRIVATE void      gen_where_g         (EXPR *e);
PRIVATE void      gen_application_g   (EXPR *e1, EXPR *e2, Boolean tro,
				       Boolean closed);
PRIVATE int	  bind_team_ids_g     (EXPR *e);

/****************************************************************
 *			PUBLIC_VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			generate_implicit_pop			*
 ****************************************************************
 * If generate_implicit_pop is true, then generate a POP_I	*
 * instruction at the end of the executable code of a		*
 * declaration.							*
 ****************************************************************/

Boolean generate_implicit_pop = FALSE;

/****************************************************************
 *			irregular_gen				*
 ****************************************************************
 * irregular_gen is -1 when entering generate_exec_g on an	*
 * irregular function, and is 1 when entering generate_exec_g	*
 * on an ordinary function.  It is incremented when entering a	*
 * new scope, and decremented when leaving a scope.  The effect *
 * is that irregular_gen is 1 when generating the body of the	*
 * outer function of the definition, and not inside any inner	*
 * scopes. 							*
 ****************************************************************/

int irregular_gen;

/****************************************************************
 *			suppress_tro				*
 *			forever_suppress_tro			*
 ****************************************************************
 * suppress_tro is true if tail-recursion optimization should	*
 * be suppressed. Normally, suppress_tro is in effect for only  *
 * one declaration, and is automatically turned off at the end  *
 * of a declaration.  forever_suppress_tro is true if 		*
 * suppress_tro should remain true across many declarations.    *
 ****************************************************************/

Boolean suppress_tro         = FALSE;
Boolean forever_suppress_tro = FALSE;

/****************************************************************
 *			PRIVATE_VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			gen_loop_st				*
 ****************************************************************
 * This stack holds, on its top, the offset in the local 	*
 * environment of the control variable for a loop, while the	*
 * loop is being generated. 					*
 ****************************************************************/

PRIVATE INT_STACK gen_loop_st  = NULL;  

/****************************************************************
 *			in_try					*
 ****************************************************************
 * in_try holds the number of try expression in which the	*
 * current context is nested.  It is used to determine 		*
 * whether a failure will be caught locally.			*
 ****************************************************************/

PRIVATE int  in_try = 0;

/****************************************************************
 *			exec_kind				*
 *			exec_name				*
 ****************************************************************
 * When code for a function or lazy expression is generated, it *
 * begins with an instruction that gives the name associated    *
 * with the code.  The name is used for reporting to the user   *
 * who was executing at a given point.  			*
 *								*
 * exec_name is the name that will be used when the next	*
 * function or lazy expression is generated.			*
 *								*
 * exec_kind is the kind of instruction to use.  It is ID_DCL_I	*
 * if exec_name is the name of an identifier that is defined    *
 * in the outer environment.  It is STRING_DCL_I if		*
 * exec_name is  a string that is not an outer identifier.	*
 ****************************************************************/

PRIVATE int  exec_kind;
PRIVATE char *exec_name;

/****************************************************************
 *			prev_line_num				*
 ****************************************************************
 * When starting line n, the code generator generates LINE_I n. *
 * To avoid redundant LINE_I instructions, the code generator   *
 * keeps the line of the last LINE_I instruction in		*
 * prev_line_num.  It knows to generate a new LINE_I instruction*
 * when the current line is different from prev_line_num.	*
 ****************************************************************/

PRIVATE int prev_line_num;


/****************************************************************/

#define START_SCOPE(mark) mark = top_i(envloc_st)
#define CURRENT_ENVIRONMENT_SIZE_BYTE un_env_size_g(top_i(numlocals_st), TRUE)

/****************************************************************
 *			GENERATE_CODE_G				*
 ****************************************************************
 * Generate code for expression e, which runs in scope `scope'. *
 * The code will arrange for the result of e to be on the top	*
 * of the stack.  'name' is the name of this code, and 		*
 * kind = ID_DCL_I if name is to be declared via ID_DCL_I and   *
 * is STRING_DCL_I if name is to be declared via STRING_DCL_I.  *
 *								*
 * The return value is the number of locals that are used in    *
 * performing the declaration.					*
 *								*
 * This is the top level entry to the code generator.		*
 *								*
 * XREF: Called by gendcl.c to generate code for a declaration.	*
 ****************************************************************/

int generate_code_g(EXPR **e, int scope, int kind, char *name)
{
  int num_locals;

# ifdef DEBUG
    if(trace_gen) {
      trace_t(122, *e, name);
    }
    if(trace_gen > 2) {
      trace_t(123, scope);
      print_expr(*e, 1);
    }
# endif

  /*--------------------------------------------------------------*
   * Before generating code, simplify the expression if possible. *
   *--------------------------------------------------------------*/

  simplify_expr(e);

# ifdef DEBUG
    if(trace_gen > 2) {
      trace_t(124);
      print_expr(*e, 1);
    }
# endif

  /*-----------------------------------------*
   * Set up the context for generate_exec_g. *
   *-----------------------------------------*/

  SET_LIST(envloc_st,    NIL);
  SET_LIST(numlocals_st, NIL);
  SET_LIST(env_id_st,    NIL);

  last_label_loc = NULL;
  prev_line_num  = 0;
  in_try         = 0;

  true_push_scope_g();
  current_scope = scope;
  init_gen_globals();

  /*--------------------------------------------------------------*
   * Generate the name instruction for the declaration. If we are *
   * defining a function, then this is NOT the name instruction   *
   * for the function.  It is the name for the code that creates  *
   * the function.						  *
   *--------------------------------------------------------------*/

  exec_kind   = kind;
  exec_name   = name;
  if(*e != NULL) genP_g(LINE_I, (*e)->LINE_NUM);
  gen_name_instr_g(kind, name);

  /*------------------------------------------------------------*
   * Determine which ids occurrences are final uses, so that	*
   * final fetches can be generated for them.			*
   *------------------------------------------------------------*/

  mark_final_ids(*e);

  /*--------------------------------------------------------*
   * Call generate_exec_g to do the actual code generation. *
   *--------------------------------------------------------*/

  bump_expr(*e);
  generate_exec_g(*e, TRUE, 0);

  /*-----------------------------------------------------------*
   * Generate a RETURN_I for the declaration.  This terminates *
   * the declaration.  Note, again, that this is the return    *
   * for the declaration, not for the function, if we are      *
   * defining a function.				       *
   *-----------------------------------------------------------*/

  gen_g(RETURN_I);

  /*-----------*
   * Clean up. *
   *-----------*/

  drop_expr(*e);
  num_locals = top_i(numlocals_st);
  true_pop_scope_g();

  SET_LIST(envloc_st,    NIL);
  SET_LIST(numlocals_st, NIL);
  SET_LIST(env_id_st,    NIL);

  return num_locals;
}


/****************************************************************
 *			GENERATE_EXEC_G				*
 ****************************************************************
 * Generate code for expression e.  This fuction should only	*
 * be used from within generate_code_g, and functions that are  *
 * running within that that function, since it requires that    *
 * the context be set up as is done there.			*
 *								*
 * Tro is true if this expression is logically followed by a	*
 * return, so that tail-recursion can be improved.		*
 *								*
 * Line is 0 to use e's line number when generating LINE_I	*
 * instructions, and nonzero to force a given line number.	*
 *								*
 * XREF: Called only here and in prim.c to handle role-modify	*
 * code.  The code in prim.c is only called from within		*
 * genereate_exec_g.						*
 ****************************************************************/

void generate_exec_g(EXPR *e, Boolean tro, int line)
{
  EXPR_TAG_TYPE kind;

  if(e == NULL) return;

  if(line == 0) line = e->LINE_NUM;

# ifdef DEBUG
    if(trace_gen) {
      trace_t(125, e, expr_kind_name[EKIND(e)], toint(tro), 
	      line, current_scope);
    }
    if(trace) {
      if(line == 0) trace_t(127);
    }
# endif

  /*---------------------------------------------------------------*
   * If we are starting a new line, generate a LINE_I instruction. *
   *---------------------------------------------------------------*/

  if(line != 0 && line != prev_line_num) {
    genP_g(LINE_I, line);
    prev_line_num = line;
  }

  /*--------------------------------------------------------------------*
   * Get the parts of e, since it is safe to do so, and this saves	*
   * having to do so over and over below.  Then switch on the		*
   * kind of expression.						*
   *--------------------------------------------------------------------*/

  kind = EKIND(e);
  switch(kind) {

    case APPLY_E:
	gen_application_g(e->E1, e->E2, tro, e->SCOPE);
	break;

    case SEQUENCE_E:
	gen_sequence_g(e, tro);
	break;

    case LAZY_LIST_E:
	gen_lazy_list_g(e);
	break;

    case AWAIT_E:
	gen_lazy_g(e, tro, line);
	break;

    case SINGLE_E:
        gen_single_g(e, tro);
	break;

    case TEST_E:
        gen_test_g(e);
        break;

    case STREAM_E:
        gen_stream_g(e, tro);
        break;

    case CONST_E:
	gen_const_g(e);
	break;

    case DEFINE_E:
        gen_define_g(e);
        break;

    case LET_E:
        gen_letexpr_g(e);
        break;

    case LOCAL_ID_E:
	generate_local_id_g(e, FALSE);
	break;

    case GLOBAL_ID_E:
	gen_global_id_g(e, FALSE);
	break;

    case SPECIAL_E:
	gen_special_g(e);
        break;

    case PAIR_E:
        gen_pair_g(e);
        break;

    case TRAP_E:
        gen_trap_g(e);
        break;

    case TRY_E:
        in_try++;
        /* No break - fall through to next case. */

    case IF_E:
        gen_if_try_g(e, tro);
        break;

    case OPEN_E:

#       ifdef DEBUG
	  if(trace_gen > 1) {
	    trace_t(134);
	    print_str_list_nl(e->EL1);
	  }
#       endif

        set_offsets_g(e->EL1, e->EL2);
        generate_exec_g(e->E1, tro, 0);
        break;

    case LAZY_BOOL_E:
	gen_lazy_bool_g(e, tro);
	break;

    case FUNCTION_E:
	gen_function_g(e);
	break;

    case SAME_E:
	gen_same_g(e, tro, line);
	break;

    case EXECUTE_E:
	{int envmark;
         START_SCOPE(envmark);
	 generate_exec_g(e->E1, tro, line);
	 break;
        }

    case MANUAL_E:
	gen_g(HERMIT_I);
	break;

    case FOR_E:
	gen_for_g(e);
	break;

    case LOOP_E:
        gen_loop_g(e, tro);
        break;

    case RECUR_E:
        gen_recur_g(e);
        break;

    case WHERE_E:
        gen_where_g(e);
        break;

    case PAT_VAR_E:
	semantic_error(GEN_PATVAR_ERR, 0);
        break;

    case IDENTIFIER_E:
    case BAD_E:
	semantic_error(NO_GEN_ERR, 0);
	break;

    default:
	semantic_error1(BAD_EXPR_KIND_ERR, (char *) tolong(kind), e->LINE_NUM);
	break;
  }

# ifdef DEBUG
    if(trace_gen) trace_t(139, e);
# endif
}


/*****************************************************************
 *			GENERATE_LOCAL_ID_G			 *
 *****************************************************************
 * Generate local id e, which is a final fetch if final is true. *
 *****************************************************************/

PRIVATE void generate_local_id_g(EXPR *e, Boolean final)
{
  int n;
  int e_scope = e->SCOPE;

  /*------------------------------------------------------------*
   * Any id at scope 0 is a recursive reference, and is global. *
   *------------------------------------------------------------*/

  if(e_scope == 0) {
    e->kind = GLOBAL_ID_E;
    e->GIC = NULL;
    gen_global_id_g(e, FALSE);
    return;
  }

  /*--------------------------------------------------------------*
   * If e->OFFSET == 0, then we are generating an identifier that *
   * is defined in expression A, and used in B, where the program *
   * has expression A B, and A computes a function.  This is not  *
   * allowed.  It is not detected earlier since the id handler    *
   * ids/ids.d:process_ids, must run before type inference is	  *
   * done, so it cannot recognize an expression whose type is a   *
   * function type.						  *
   *--------------------------------------------------------------*/

  if(e->OFFSET == 0) {
    semantic_error1(UNBOUND_ID_IN_GEN_ERR, display_name(e->STR), e->LINE_NUM);
    return;
  }

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(144, e->STR, toint(e_scope), toint(e->OFFSET - 1), final,
	      current_scope);
    }
# endif

  n = (e_scope == -1) 
         ? 0
	 : scopes[current_scope].offset - scopes[e_scope].offset;
  if(n == 0) {
    gen_g(final ? FINAL_FETCH_I : FETCH_I); 
    gen_g(toint(e->OFFSET) - 1);
  }
  else {
    gen_g(final ? FINAL_LONG_FETCH_I : LONG_FETCH_I); 
    gen_g(toint(e->OFFSET) - 1); 
    gen_g(n);
  }
}


/****************************************************************
 *		   GEN_SEQUENCE_G				*
 ****************************************************************
 * Generate a SEQUENCE_E expression e.				*
 *								*
 * tro is as for generate_exec_g.				*
 ****************************************************************/

PRIVATE void gen_sequence_g(EXPR* e, Boolean tro)
{
  int envmark;

  /*----------------------------------------------------------*
   * A SEQUENCE_E expression should be generated as follows.  *
   *							      *
   *	compute e1					      *
   *	SPLIT_I						      *
   *	compute e2					      *
   * 							      *
   * The SCOPE field of e is 0 if bindings done within e1     *
   * should be hidden.					      *
   *----------------------------------------------------------*/

  START_SCOPE(envmark);
  generate_exec_g(e->E1, FALSE, 0);
  if(e->SCOPE == 0) pop_scope_g(envmark);
  gen_g(SPLIT_I);
  generate_exec_g(e->E2, tro, 0);
}


/****************************************************************
 *		GEN_LAZY_DOUBLE_COMPILE_START_G			*
 ****************************************************************
 * A small expression Await e1 then e2 &Await is compiled twice,*
 * once for the case where e1 is not lazy, and once for the	*
 * case where e1 is lazy.					*
 *								*
 * This function generates the test for laziness and the part	*
 * for e1 not lazy.						*
 *								*
 * *loc3 is set to the index where the local label that follows *
 * the entire lazy expression needs to be put, and *loc3_high   *
 * is the index where the label prefix (for a long label)       *
 * needs to be put.						*
 ****************************************************************/

PRIVATE void 
gen_lazy_double_compile_start_g(EXPR *e, package_index *loc3, 
				package_index* loc3_high)
{
  Boolean old_scope_nonempty;
  int test_count = 0, envmark, e_scope;
  package_index loc1_high;
  INT_LIST *low_list, *high_list;

  EXPR* e1 = e->E1;
  EXPR* e2 = e->E2;
      
  START_SCOPE(envmark);

  /*------------------------------------------------------------*
   * low_list and high_list hold locations that need 		*
   * backpatching with local labels.				*
   *------------------------------------------------------------*/

  low_list = high_list = NIL;
  while(EKIND(e1) == PAIR_E) {
    generate_exec_g(e1->E1, FALSE, 0);
    gen_g((test_count++ == 0) ? TEST_LAZY1_I : TEST_LAZY_I);
    loc1_high = begin_labeled_instr_g();
    gen_g(GOTO_IF_FALSE_I);
    SET_LIST(low_list, int_cons(gen_g(0), low_list));
    SET_LIST(high_list, int_cons(loc1_high, high_list));
    e1 = e1->E2;
  }

  /*------------------------------------------------------------*
   * Generate e2 for eager execution.  The scope will be the 	*
   * same as for e1, since e->SCOPE will be forced empty.	*
   * Preserve the contents of nonshared boxes, since that is	*
   * required for await expressions.  				*
   *------------------------------------------------------------*/

  /*--------------------------------------------------------*
   * Update the scopes array, forcing e->SCOPE to be empty. *
   *--------------------------------------------------------*/

  e_scope = e->SCOPE;
  old_scope_nonempty = scopes[e_scope].nonempty;
  scopes[e_scope].nonempty = 0;
  fix_scopes_array_g();

  /*-------------------------------------------------*
   * Generate e2, preserving nonshared box contents. *
   *-------------------------------------------------*/

  START_SCOPE(envmark);
  gen_g(BEGIN_PRESERVE_I);
  generate_exec_g(e2, FALSE, 0);
  gen_g(END_PRESERVE_I);
  pop_scope_g(envmark);

  /*----------------------------------------------------*
   * After evaluating e2, jump to just after this await *
   * expression.					*
   *----------------------------------------------------*/

  *loc3_high = begin_labeled_instr_g();
  gen_g(GOTO_I);
  *loc3 = gen_g(0);		/* patched below. */
       
  /*-------------------------------------------------*
   * Back-patch the labels at the conditional jumps. *
   *-------------------------------------------------*/

  if(low_list != NIL) {
    UBYTE lbl_low, lbl_high;
    LIST *plow, *phigh;
    package_index first_low = low_list->head.i;
    package_index first_high = high_list->head.i;
	 
    label_g(first_high, first_low);
    lbl_low  = current_code_array[first_low];
    lbl_high = current_code_array[first_high];
    for(plow = low_list->tail,
	phigh = high_list->tail; 
	plow != NIL; 
	plow = plow->tail,
	phigh = phigh->tail) {
      current_code_array[plow->head.i]  = lbl_low;
      current_code_array[phigh->head.i] = lbl_high;
    }
  }

  /*-------------------------------------------------*
   * Put the scopes array back to what it should be. *
   *-------------------------------------------------*/

  scopes[e_scope].nonempty = old_scope_nonempty;
  fix_scopes_array_g();
}


/****************************************************************
 *			GEN_LAZY_G				*
 ****************************************************************
 * Generate AWAIT_E expression e.				*
 *								*
 * tro and line are as for generate_exec_g.			*
 ****************************************************************/

PRIVATE void gen_lazy_g(EXPR *e, Boolean tro, int line)
{
  /*------------------------------------------------------------*
   * This expression is await a1,a2,... then b &await, where e1 *
   * is (a1,a2,...), or is NULL if there is nothing between	*
   * await and then, and e2 is b.				*
   *							      	*
   * e->DOUBLE_COMPILE is 1 to compile twice, once in a lazy 	*
   * form and once in an eager form, for execution speed (what	*
   * there is of it.)  						*
   *								*
   * e->extra is true if this await should be ignored here.	*
   *								*
   * e->SCOPE is the scope in which e2 should run.  (e2 needs	*
   * its own scope.)						*
   *								*
   * e->OFFSET is 0 for an ordinary await, and 1 for await&...	*
   *------------------------------------------------------------*/

  /*--------------------------------------------------------------*
   * Ignore an await that is at the top of a (|a,b,c|) construct. *
   *--------------------------------------------------------------*/

  if(e->extra) {
    generate_exec_g(e->E2, tro, line);
  }
  else {
    int prev_scope, double_compile, recompute;
    package_index loc1, loc1_high, loc2, loc3 = 0, loc3_high = 0;
    LIST *binding_list_mark;

    /*----------------------------------------------------------*
     * If double-compiling, then generate the eager part first. *
     *----------------------------------------------------------*/

    double_compile = e->E1 != NULL && e->DOUBLE_COMPILE != 0;
    if(double_compile) {
      gen_lazy_double_compile_start_g(e, &loc3, &loc3_high);
    }

    /*-----------------------------*
     * Now generate the lazy part. *
     *-----------------------------*/

    recompute = e->OFFSET;
    loc1_high = begin_labeled_instr_g();
    gen_g(recompute? LAZY_RECOMPUTE_I : LAZY_I);

    /*---------------------------------------------------------------*
     * Generate the label of the code following the LAZY_RECOMPUTE_I *
     * or LAZY_I -- patched below.				     *
     *---------------------------------------------------------------*/

    loc1 = gen_g(0);

    /*---------------------------*
     * Generate the return type. *
     *---------------------------*/

    generate_type_g(e->ty, 0);
    gen_g(END_LET_I);

    /*------------------------------------------------------*
     * Generate the environment size byte -- patched below. *
     *------------------------------------------------------*/

    loc2 = gen_g(0);

    /*-------------------------------------------------*
     * Push a new scope for the body of the lazy part. *
     *-------------------------------------------------*/

    bump_list(binding_list_mark = finger_new_binding_list());
    prev_scope    = current_scope;
    current_scope = e->SCOPE;
#   ifdef DEBUG
      if(trace_gen > 1) {
	trace_t(130, current_scope);
      }
#   endif
    true_push_scope_g();

    /*----------------------------------------------------------*
     * Generate the name instruction for the body and then 	*
     * generate the body, followed by a RETURN_I. 		*
     *----------------------------------------------------------*/

    gen_name_instr_g(exec_kind, exec_name);
    generate_exec_g(e->E2, TRUE, 0);
    gen_g(RETURN_I);

    /*----------------------------------------------------*
     * Patch the environment size byte and pop the scope. *
     *----------------------------------------------------*/

    current_code_array[loc2] = CURRENT_ENVIRONMENT_SIZE_BYTE;
    true_pop_scope_g();
    current_scope = prev_scope;

#   ifdef DEBUG
      if(trace_gen > 1) {
	trace_t(131, current_scope);
      }
#   endif

    /*-------------------------------------------------------------*
     * Patch the label of the code after the await, and the label  *
     * of goto after a double-compiled part. 			   *
     *-------------------------------------------------------------*/

    label_g(loc1_high, loc1);
    if(double_compile) {
      current_code_array[loc3]      = current_code_array[loc1];
      current_code_array[loc3_high] = current_code_array[loc1_high];
    }

    undo_bindings_u(binding_list_mark);
    drop_list(binding_list_mark);
  }
}


/****************************************************************
 *			GEN_LAZY_LIST_G				*
 ****************************************************************
 * Generate lazy list expression e, which has the form [:a:].	*
 ****************************************************************/

PRIVATE void gen_lazy_list_g(EXPR *e)
{
  EXPR* e1 = e->E1;
  package_index loc1, loc1_high, loc2;
  int prev_scope;
  LIST *binding_list_mark;

  /*------------------------------------------------------------*
   * The SCOPE field of e is the number of the scope in which 	*
   * e1 is to run.  (e1 needs, in general, to allocate an 	*
   * environment for itself.)  The OFFSET field is 0 normally,	*
   * and 1 if e is [:& e1:].		    			*
   *								*
   * [:e1:] is generated as follows.				*
   *								*
   *   LONG_LLABEL_I(h)						*
   *   LAZY_LIST_I(lab)    (or LAZY_LIST_RECOMPUTE_I)		*
   *   --type e1->ty						*
   *   END_LET_I						*
   *   n                   (local environment size)		*   
   *   --code for e1						*
   *   RETURN_I							*
   *------------------------------------------------------------*/

  loc1_high = begin_labeled_instr_g();
  gen_g(e->OFFSET ? LAZY_LIST_RECOMPUTE_I : LAZY_LIST_I);

  /*-------------------------------------------------------------*
   * The LAZY_LIST_I or LAZY_LIST_RECOMPUTE_I is followed by	 *
   *								 *
   *    1. The local label that just follows [:e1:]		 *
   *    2. The type of e1, followed by END_LET_I.		 *
   *    3. The index of the size of the environment needed.	 *
   *								 *
   * The local label and environment size index must be generated*
   * here, and patched below, since we don't know what they are  *
   * yet.							 *
   *-------------------------------------------------------------*/

  loc1 = gen_g(0);		/* local label at end *
				 * - patched below.   */
  generate_type_g(e->ty, 0);
  gen_g(END_LET_I);
  loc2 = gen_g(0);   		/* environment size index - patched below. */

  /*----------------------*
   * Start the new scope. *
   *----------------------*/

  bump_list(binding_list_mark = finger_new_binding_list());
  prev_scope = current_scope;
  current_scope = e->SCOPE;
# ifdef DEBUG
    if(trace_gen > 1) trace_t(128, current_scope); 
# endif
  true_push_scope_g();

  /*----------------------------------------------*
   * Generate the body, e1, followed by RETURN_I. *
   *----------------------------------------------*/

  generate_exec_g(e1, TRUE, 0);
  gen_g(RETURN_I);

  /*------------------------------------------------------*
   * Patch the environment size index, and go back to the *
   * previous scope.  					  *
   *------------------------------------------------------*/

  current_code_array[loc2] = CURRENT_ENVIRONMENT_SIZE_BYTE;
  true_pop_scope_g();
  current_scope = prev_scope;
  undo_bindings_u(binding_list_mark);
  drop_list(binding_list_mark);

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(129, current_scope); 
    }
# endif

  /*--------------------------------------*
   * Generate the local label, and patch. *
   *--------------------------------------*/

  label_g(loc1_high, loc1);
}


/****************************************************************
 *			   GEN_SINGLE_G				*
 ****************************************************************
 * Generate SINGLE_E expression e.				*
 *								*
 * tro is as for generate_exec_g.				*
 ****************************************************************/

PRIVATE void
gen_single_g(EXPR *e, Boolean tro)
{
  int attr, envmark;
  Boolean newtro;
	 
  attr    = e->SINGLE_MODE;
  if(attr == ROLE_MODIFY_ATT) {
    gen_role_modify_g(e);
  }
  else {
    START_SCOPE(envmark);
    newtro = tro;
    if(attr == ATOMIC_ATT) {
      gen_g(BEGIN_ATOMIC_I); 
      newtro = 0;
    }
    else if(attr == FIRST_ATT || attr == CUTHERE_ATT) {
      gen_g(BEGIN_CUT_I); 
      newtro = 0;
    }

    generate_exec_g(e->E1, newtro, 0);

    if(attr == CUTHERE_ATT) gen_g(END_CUT_I);
    else if(attr == FIRST_ATT) {
      gen_g(HERMIT_I);
      gen_g(CUT_I);
      gen_g(END_CUT_I);
    }
    else if(attr == ATOMIC_ATT) gen_g(END_ATOMIC_I);
    if(e->SCOPE == 0) pop_scope_g(envmark);
  }
}


/****************************************************************
 *		   	GEN_TEST_G				*
 ****************************************************************
 * Generate TEST_E expression e.				*
 ****************************************************************/

PRIVATE void
gen_test_g(EXPR *e)
{
  int prim, instr;
  package_index loc1, loc1_high;
  PART *part;
  INT_LIST *sel_list;
  Boolean irregular;

  EXPR* e1 = e->E1;
  EXPR* e2 = e->E2;

  /*---------------------------------------------------------*
   * This expression is {e1 else e2}, where e2 might be NULL *
   * if it is omitted.  The generated code looks like	     *
   *							     *
   *    code for e1					     *
   *    TEST_I x					     *
   *    HERMIT_I					     *
   *							     *
   * when e2 is a fixed, standard exception (or NULL, which  *
   * is assumed to mean testX), and is 			     *
   *							     *
   *    code for e1					     *
   *    NOT_I						     *
   *    GOTO_IF_FALSE_I (l)				     *
   *    code for e2					     *
   *    FAIL_I						     *
   *    LLABEL_I(l)					     *
   *    HERMIT_I					     *
   *							     *
   * otherwise.						     *
   *---------------------------------------------------------*/

  generate_exec_g(e1, FALSE, 0);
  if(e2 == NULL) {
    gen_g(TEST_I); gen_g(0);
  }
  else {
    prim = get_prim_g(e2, &instr, &part, &irregular, &sel_list);
    if(prim == STD_CONST) {
      gen_g(TEST_I); gen_g(instr);
    }
    else {
      gen_g(NOT_I);
      loc1_high = begin_labeled_instr_g();
      gen_g(GOTO_IF_FALSE_I);
      loc1 = gen_g(0);	/* patched below. */
      generate_exec_g(e2, FALSE, 0);
      gen_g(FAIL_I);
      label_g(loc1_high, loc1);
    }
  }
  gen_g(HERMIT_I);
}


/****************************************************************
 *		   	GEN_STREAM_G				*
 ****************************************************************
 * Generate STREAM_E expression e.				*
 *								*
 * tro is as for generate_exec_g.				*
 ****************************************************************/

PRIVATE void
gen_stream_g(EXPR *e, Boolean tro)
{
  int envmark;
  package_index loc1, loc1_high, loc2, loc2_high;
  LIST *binding_list_mark;

  /*--------------------------------------------------------------*
   * This expression is either Stream e1 then e2 %Stream or	  *
   * Mix e1 with e2 %Mix.   e->STREAM_MODE is STREAM_ATT for the  *
   * former, and MIX_ATT for the latter.			  *
   *								  *
   * The generated instructions are				  *
   *       STREAM_I l1						  *
   *       code for e1						  *
   *       GOTO_I l2						  *
   *   l1:							  *
   *       code for e2						  *
   *   l2:							  *
   * except that a MIX_I instruction is used for a mix, and the   *
   * GOTO_I is replaced by a RETURN_I in the case where tro is	  *
   * true.							  *
   *--------------------------------------------------------------*/

  loc1_high = begin_labeled_instr_g();
  if(e->STREAM_MODE == STREAM_ATT) gen_g(STREAM_I);
  else gen_g(MIX_I);
  loc1 = gen_g(0);    /* label, patched below. */
  bump_list(binding_list_mark = finger_new_binding_list());
  START_SCOPE(envmark);
  generate_exec_g(e->E1, tro, 0);
  pop_scope_g(envmark);
  if(tro) {
    gen_g(RETURN_I);
    loc2 = loc2_high = 0;     /* No effect - suppresses warning */
  }
  else {
    loc2_high = begin_labeled_instr_g();
    gen_g(GOTO_I);
    loc2 = gen_g(0);
  }
  label_g(loc1_high, loc1);
  START_SCOPE(envmark);
  generate_exec_g(e->E2, tro, 0);
  pop_scope_g(envmark);
  if(!tro) label_g(loc2_high, loc2);
  undo_bindings_u(binding_list_mark);
  drop_list(binding_list_mark);
}


/****************************************************************
 *		   	GEN_CONST_G				*
 ****************************************************************
 * Generate CONST_E expression e.				*
 ****************************************************************/

PRIVATE void gen_const_g(EXPR *e)
{
  switch(e->SCOPE) {
    case CHAR_CONST:
      gen_g(CHAR_I);
      gen_g(char_val(e->STR, e->LINE_NUM));
      break;

    case BOOLEAN_CONST:
      if(e->STR == NULL) gen_g(NIL_I);
      else gen_g(TRUE_I);
      break;

    case FAIL_CONST:
      gen_g(FAILC_I);
      gen_g(e->OFFSET);
      break;

    case STRING_CONST:
       if(e->STR[0] == '\0') gen_g(NIL_I);
       else {
	 int n = string_const_loc(e->STR, STRING_DCL_I);
	 genP_g(CONST_I, n);
       }
       break;

    case HERMIT_CONST:
      gen_g(HERMIT_I);
      break;

    case REAL_CONST:
      {int n = string_const_loc(e->STR, REAL_DCL_I);
       genP_g(CONST_I, n);
       break;
      }

    case NAT_CONST:
      {char* s = e->STR;
       int n;

       if(strlen(s) <= 2) {
	 sscanf(s, "%d", &n);
	 if(n == 0) gen_g(ZERO_I);
	 else {
	   gen_g(SMALL_INT_I);
	   gen_g(n);
	 }
       }
       else {
	 n = string_const_loc(s, INT_DCL_I);
	 genP_g(CONST_I, n);
       }
       break;
      }

    default:
      semantic_error1(BAD_CONST_KIND_ERR, (char *) tolong(e->SCOPE), 
		      e->LINE_NUM);
  }
}


/****************************************************************
 *		   	GEN_DEFINE_G				*
 ****************************************************************
 * Generate DEFINE_E expression e.				*
 ****************************************************************/

PRIVATE void gen_define_g(EXPR *e)
{
  package_index loc1, loc1_high, loc2;
  int prev_scope;
  LIST *binding_list_mark;

  EXPR* e1 = e->E1;
  EXPR* e2 = e->E2;

  /*------------------------------------------------------------*
   * This expression is Define e1 = e2.  e->SCOPE is the	*
   * scope in which lazy expression e2 is to run.		*
   *------------------------------------------------------------*/

  e1 = skip_sames(e1);

  /*-----------------------------------------------------*
   * Generate a DEF_I, followed by			 *
   *							 *
   *  1. The local label just after this expression.	 *
   *  2. The index of the identifier being defined (e1). *
   *  3. The name of e1.				 *
   *  4. The species of e1, followed by END_LET_I.	 *
   *  5. The index of the environment size needed by e2  *
   *-----------------------------------------------------*/
	  
  loc1_high = begin_labeled_instr_g();
  gen_g(DEF_I);
  loc1 = gen_g(0);
  if(e1->OFFSET == 0) {
    gen_g((e1->OFFSET = incr_env_g()) - 1);
    note_id_g(e1);
  }
  else {
    gen_g(e1->OFFSET - 1);
  }

  e1->bound = 1;
  gen_str_g(name_tail(e1->STR));
  generate_type_g(e1->ty, 0);
  gen_g(END_LET_I);
  loc2 = gen_g(0);    /* environment size index byte. */

  /*------------------------------------------------*
   * Move into scope e->SCOPE before generating e2. *
   *------------------------------------------------*/

  bump_list(binding_list_mark = finger_new_binding_list());
  prev_scope = current_scope;
  current_scope = e->SCOPE;
# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(132, current_scope);
    }
# endif
  true_push_scope_g();

  /*-------------------------------------------------------------*
   * Generate e2.  Any identifiers defined inside e2 should have *
   * e1. prepended to their names.  For example, if identifier   *
   * gamma is defined in the body of the definition of alpha, 	 *
   * then its name is reported to the user as "alpha.gamma".	 *
   *-------------------------------------------------------------*/

  {int len, old_exec_kind;
   char *old_exec_name;
#  ifdef SMALL_STACK
     char *newname = (char *) BAREMALLOC(MAX_NAME_LENGTH + 1);
#  else
     char newname[MAX_NAME_LENGTH+1];
#  endif

   old_exec_kind = exec_kind;
   old_exec_name = exec_name;
   len           = strlen(exec_name);
   strcpy(newname, exec_name);
   strncat(newname, ".", MAX_NAME_LENGTH-len);
   strncat(newname, display_name(e1->STR), MAX_NAME_LENGTH-len-1);
   exec_kind = STRING_DCL_I;
   exec_name = newname;
   gen_name_instr_g(exec_kind, exec_name);

   generate_exec_g(e2, TRUE, 0);

   exec_kind = old_exec_kind;
   exec_name = old_exec_name;
#  ifdef SMALL_STACK
     FREE(newname);
#  endif
 }

  /*-------------------------------------------------------*
   * The code for e2 is followed by a RETURN_I, which ends *
   * the lazy code.					   *
   *-------------------------------------------------------*/

  gen_g(RETURN_I);
  
  /*-----------------------------*
   * Return to the former scope. *
   *-----------------------------*/

  current_code_array[loc2] = CURRENT_ENVIRONMENT_SIZE_BYTE;
  true_pop_scope_g();
  current_scope = prev_scope;
  undo_bindings_u(binding_list_mark);
  drop_list(binding_list_mark);

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(133, current_scope);
    }
# endif

  /*-----------------------------------------------------------*
   * Patch the label that follows the defin expr.  The value *
   * of the define is (), so generate that.		       *
   *-----------------------------------------------------------*/

  label_g(loc1_high, loc1);
  gen_g(HERMIT_I);
}


/****************************************************************
 *		   	GEN_LETEXPR_G				*
 ****************************************************************
 * Generate LET_E expression e.					*
 ****************************************************************/

PRIVATE void gen_letexpr_g(EXPR *e)
{
  int envmark, old_exec_kind, len;
  char *old_exec_name;
# ifdef SMALL_STACK
    char *newname = (char *) BAREMALLOC(MAX_NAME_LENGTH + 1);
# else
    char newname[MAX_NAME_LENGTH+1];
# endif

  EXPR* e1 = e->E1;
  EXPR* e2 = e->E2;

  /*------------------------------------------------------------*
   * A let is similar to a define, but the body is not lazy,	*
   * and so is not computed in a new scope.			*
   *------------------------------------------------------------*/

  e1 = skip_sames(e1);

  /*------------------------------------------------------*
   * Generate the body, e2. Any identifiers defined there *
   * have the name of e1 prepended to their names.	  *
   *------------------------------------------------------*/

  old_exec_kind = exec_kind;
  old_exec_name = exec_name;
  len           = strlen(exec_name);
  strcpy(newname, exec_name);
  strncat(newname, ".", MAX_NAME_LENGTH-len);
  strncat(newname, display_name(e1->STR), MAX_NAME_LENGTH-len-1);
  exec_kind = STRING_DCL_I;
  exec_name = newname;
  START_SCOPE(envmark);

  generate_exec_g(e2, FALSE, 0);

  exec_kind = old_exec_kind;
  exec_name = old_exec_name;

  /*----------------------------------------------------------*
   * Check for open let.  If not open, then pop the bindings. *
   *----------------------------------------------------------*/

  if(e->SCOPE == 0) pop_scope_g(envmark);

  /*-------------------------------------------------------*
   * Generate a let for e1 if e->LET_FORM indicates a let. *
   *-------------------------------------------------------*/

  if(e->LET_FORM == 0) {
    if(e1->OFFSET == 0) {
      gen_let_g(e1);
    }
    else {
      gen_g(e1->used_id ? LET_I : DEAD_LET_I);
      gen_g(e1->OFFSET - 1);
      gen_str_g(name_tail(e1->STR));
      generate_type_g(e1->ty, 0);
      gen_g(END_LET_I);
    }
  }

  /*----------------------------------------------------*
   * Generate a relet if e->LET_FORM indicates a relet. *
   *----------------------------------------------------*/

  else {
    gen_g(RELET_I);
    gen_g(e1->OFFSET - 1);
  }

  /*-----------------------------------------*
   * Generate a following HERMIT_I and quit. *
   *-----------------------------------------*/

  e1->bound = 1;
  gen_g(HERMIT_I);

# ifdef SMALL_STACK
    FREE(newname);
# endif
}


/****************************************************************
 *		   	GEN_COROUTINE_PAIR_G			*
 ****************************************************************
 * Generate coroutine PAIR_E expression e.			*
 ****************************************************************/

PRIVATE void gen_coroutine_pair_g(EXPR *e)
{
  int envmark = 0;
  int k1, k2, envmark1;
  package_index loc1, loc1_high, loc2, loc2_high;

  START_SCOPE(envmark);
  k1 = incr_env_g() - 1;
  k2 = incr_env_g() - 1;
  START_SCOPE(envmark1);

  loc1_high = begin_labeled_instr_g();
  gen_g(COROUTINE_I);
  loc1 = gen_g(0);
  gen_g(k1);
  generate_exec_g(e->E1, FALSE, 0);
  pop_scope_g(envmark1);
  gen_g(STORE_INDIRECT_I);
  gen_g(k1);
  gen_g(CHECK_DONE_I);
  gen_g(k2);
  gen_g(RESUME_I);

  label_g(loc1_high, loc1);
  loc2_high = begin_labeled_instr_g();
  gen_g(COROUTINE_I);
  loc2 = gen_g(0);
  gen_g(k2);
  generate_exec_g(e->E2, FALSE, 0);
  pop_scope_g(envmark1);
  gen_g(STORE_INDIRECT_I);
  gen_g(k2);
  gen_g(CHECK_DONE_I);
  gen_g(k1);
  label_g(loc2_high, loc2);
  gen_g(RESUME_I);

  gen_g(FETCH_I);
  gen_g(k1);
  gen_g(FETCH_I);
  gen_g(k2);
  pop_scope_g(envmark);
  gen_g(PAIR_I);
}


/****************************************************************
 *		   	GEN_PAIR_G				*
 ****************************************************************
 * Generate PAIR_E expression e.				*
 ****************************************************************/

PRIVATE void gen_pair_g(EXPR *e)
{
  int envmark = 0;

  if(e->COROUTINE_FORM == 0) {

    /*---------------------------*
     * An ordinary ordered pair. *
     *---------------------------*/
    
    Boolean should_pop = !e->SCOPE;
    if(should_pop) START_SCOPE(envmark);
    generate_exec_g(e->E1, FALSE, 0);
    if(should_pop) pop_env_scope_g(envmark);
    generate_exec_g(e->E2, FALSE, 0);
    if(should_pop) pop_env_scope_g(envmark);
    gen_g(PAIR_I);
  }

  else {
    gen_coroutine_pair_g(e);
  }
}


/****************************************************************
 *		   	GEN_TRAP_G				*
 ****************************************************************
 * Generate TRAP_E expression e.				*
 ****************************************************************/

PRIVATE void gen_trap_g(EXPR *e)
{
  int envmark;

  /*--------------------------------------*
   * This expression is Trap e1 => e2 or  *
   * Untrap e1 => e2 			  *
   *					  *
   * e1 is NULL to trap or untrap all.	  *
   * e->TRAP_FORM is TRAP_ATT for a trap, *
   * and UNTRAP_ATT for an untrap.	  *
   *--------------------------------------*/

  START_SCOPE(envmark);
  gen_g(PUSH_TRAP_I);
  if(e->E1 == NULL_E) {
    gen_g(ALL_TRAP_I);
  }
  else generate_exec_g(e->E1, FALSE, 0);

  if(e->TRAP_FORM == TRAP_ATT) gen_g(TRAP_I);
  else gen_g(UNTRAP_I);

  generate_exec_g(e->E2, FALSE, 0);
  gen_g(POP_TRAP_I);
  if(e->SCOPE == 0) pop_scope_g(envmark);
}


/****************************************************************
 *		   	GEN_IF_TRY_G				*
 ****************************************************************
 * Generate IF_E or TRY_E expression e.				*
 ****************************************************************/

PRIVATE void gen_if_try_g(EXPR *e, Boolean tro)
{
  package_index loc1, loc1_high;
  int envmark, envmark1;
  TYPE *ty;
  package_index loc2, loc2_high;
  LIST *binding_list_mark, *binding_list_mark1;
  Boolean herm, hermE3, e1_binds_vars, use_type_scope_instrs;

  Boolean       gen_push_exc = FALSE;    /* Set true if a PUSH_EXC_I is *
					  * generated.			*/
  TYPE_TAG_TYPE kind         = EKIND(e);

  /*----------------------------------*
   * A try needs to start with TRY_I. *
   *----------------------------------*/

  if(kind == TRY_E) {
    loc1_high = begin_labeled_instr_g();
    gen_g(TRY_I); 
    loc1 = gen_g(0);	/* patched below. */
    gen_g(e->TRY_KIND);

    if(e->TRY_KIND < TRY_F || e->TRY_KIND > TRYEACHTERM_F) die(169);
  }
  else loc1 = loc1_high = 0;  /* No effect -- suppresses warning */

  /*----------------------------------------------------*
   * Generate e1, the condition being tested. 		*
   * For a try, we must push the type binding list	*
   * so that it can be recovered on failure, if e1	*
   * binds any type variables.				*
   *----------------------------------------------------*/

  START_SCOPE(envmark);
  bump_list(binding_list_mark = finger_new_binding_list());
  e1_binds_vars = binds_type_variables(e->E1);
  if(e1_binds_vars && kind == TRY_E) {
    use_type_scope_instrs = TRUE;
    gen_g(PUSH_TYPE_BINDINGS_I);
  }
  else use_type_scope_instrs = FALSE;
  generate_exec_g(e->E1, FALSE, 0);

  /*---------------------------------------------------------*
   * For a try, if e1 finishes, then it must have succeeded. *
   * For an if, we need to test the value of e1, and do a    *
   * conditional jump.					     *
   *							     *
   * If a PUSH_TYPE_BINDINGS_I instruction was used, then    *
   * type bindings must be committed to when e1 finishes     *
   * successfully.				 	     *
   *---------------------------------------------------------*/

  if(kind == TRY_E) {
    if(use_type_scope_instrs) gen_g(COMMIT_TYPE_BINDINGS_I);
    gen_g(POP_I); 
    gen_g(THEN_I);
  }
  else {
    loc1_high = begin_labeled_instr_g();
    gen_g(GOTO_IF_FALSE_I); 
    loc1 = gen_g(0);
  }

  /*---------------------------------------------------------*
   * Generate the then-part, e2. If e2 is (), we will be     *
   * popping it in all likelihood.  Generate the POP_I just  *
   * after e2, so that we will get a HERMIT_I POP_I pair     *
   * typically.  (The code improver will eliminate those     *
   * instructions.)  For correctness, we must then put a     *
   * HERMIT_I instruction after the if or try expression.    *
   *---------------------------------------------------------*/
         
  bump_list(binding_list_mark1 = finger_new_binding_list());
  START_SCOPE(envmark1);
  generate_exec_g(e->E2, tro, 0);
  herm   = FALSE;
  hermE3 = is_hermit_expr(e->E3);
  ty     = find_u(e->E2->ty);
  if(is_hermit_type(ty) && (!tro || hermE3)) {
    herm = TRUE;
    gen_g(POP_I);
  }
  if(kind == IF_E && e->mark == 0) pop_scope_g(envmark1);
  else                             pop_scope_g(envmark);

  /*-----------------------------------------------------*
   * If the else-part is not () or we generated a        *
   * PUSH_TYPE_BINDINGS_I, then we need to jump	 	 *
   * around the else part and generate it.		 *
   *-----------------------------------------------------*/

  if(!hermE3 || use_type_scope_instrs) {
    if(tro) {
      gen_g(RETURN_I);
      loc2 = loc2_high = 0;  /* No effect -- suppresses warning */
    }
    else {
      loc2_high = begin_labeled_instr_g();
      gen_g(GOTO_I);
      loc2 = gen_g(0);
    }
    label_g(loc1_high, loc1);
    if(kind == TRY_E) {
      START_SCOPE(envmark);
      if(use_type_scope_instrs) gen_g(POP_TYPE_BINDINGS_I);
      if(uses_exception(e->E3)) {
	gen_push_exc = TRUE;
	gen_g(PUSH_EXC_I);
      }
    }
    undo_bindings_u(kind == TRY_E ? binding_list_mark : binding_list_mark1);
    generate_exec_g(e->E3, tro, 0);

    /*--------------------------------------------------------*
     * If the then-part did an extra pop, we must do one here *
     * for consistency. 				      *
     *--------------------------------------------------------*/

    if(herm) gen_g(POP_I);
    if(kind == TRY_E && gen_push_exc) gen_g(POP_EXC_I);
    if(!tro) label_g(loc2_high, loc2);
  }

  /*------------------------------------------------------*
   * If the else-part is (), then we can avoid generating *
   * anything for it.					  *
   *------------------------------------------------------*/

  else {
    label_g(loc1_high, loc1);
  }

  /*---------------------------------------*
   * Clean up at the end of the if or try. *
   *---------------------------------------*/
  
  pop_scope_g(envmark);
  if(kind == TRY_E) undo_bindings_u(binding_list_mark);
  else undo_bindings_u(binding_list_mark1);
  drop_list(binding_list_mark);
  drop_list(binding_list_mark1);
  if(herm) gen_g(HERMIT_I);

  if(kind == TRY_E) in_try--;
}


/****************************************************************
 *		   	GEN_LAZY_BOOL_G				*
 ****************************************************************
 * Generate LAZY_BOOL_E expression e.				*
 ****************************************************************/

PRIVATE void gen_lazy_bool_g(EXPR *e, Boolean tro)
{
  int instr, envmark, bool_mode;
  package_index loc1, loc1_high, loc2 = 0, loc2_high = 0;

  /*----------------------------*
   * e is one of		*
   *   e1 _and_ e2		*
   *   e1 _or_  e2		*
   *   e1 _implies_ e2		*
   *----------------------------*/

  START_SCOPE(envmark);
  bool_mode = e->LAZY_BOOL_FORM;
  instr = (bool_mode == AND_BOOL) ? AND_SKIP_I : OR_SKIP_I;

  generate_exec_g(e->E1, FALSE, 0);

  loc1_high = begin_labeled_instr_g();
  gen_g((bool_mode == IMPLIES_BOOL) ? GOTO_IF_FALSE_I : instr);
  loc1 = gen_g(0);  /* label, patched below. */
  pop_scope_g(envmark);

  generate_exec_g(e->E2, tro, 0);

  if(bool_mode == IMPLIES_BOOL) {
    loc2_high = begin_labeled_instr_g();
    gen_g(GOTO_I);
    loc2 = gen_g(0);
  }
  label_g(loc1_high, loc1);
  pop_scope_g(envmark);
  if(bool_mode == IMPLIES_BOOL) {
    gen_g(TRUE_I);
    label_g(loc2_high, loc2);
  }
}


/****************************************************************
 *		   	GEN_FUNCTION_G				*
 ****************************************************************
 * Generate FUNCTION_E expression e.				*
 ****************************************************************/

PRIVATE void gen_function_g(EXPR *e)
{
  Boolean local_generate_implicit_pop;
  int n, prev_scope;
  package_index loc1, loc1_high, loc2;
  TYPE *t;
  LIST *binding_list_mark;

  /*------------------------------------------------------------*
   * The expression is (e1 => e2).  Due to former processing,	*
   * e1 must be a special expression indicating the top of 	*
   * the stack at this point.  So the body, e2, just needs 	*
   * to compute as if the argument is on top of the stack.	*
   *							    	*
   * The FUNCTION_I instruction is followed by			*
   *								*
   *  1. The label that follows the function expresion.		*
   *  2. The codomain species of the function, followed by	*
   *     END_LET_I.						*
   *  3. The index of the environment size needed by e2		*
   *  4. The function body expression (e2).			*
   *  5. RETURN_I.						*
   *								*
   * If e->OFFSET = 0, then this function needs its own   	*
   * scope.  If e->OFFSET = n > 0, then this function shares	*
   * its environment with its context, and the first open	*
   * cell in the shared environment has offset n.		*
   *------------------------------------------------------------*/

  /*------------------------------------------------------------*
   * In the case where generate_implicit_pop is true, we must	*
   * put a POP_I instruction just before the RETURN_I.  But 	*
   * only do that in the outermost function.  So keep track	*
   * of generate_implicit_pop now, and clear it.		*
   *------------------------------------------------------------*/

  local_generate_implicit_pop = generate_implicit_pop;
  generate_implicit_pop = FALSE;

  /*----------------------------------------------------*
   * The body of the function needs to be computed in a *
   * new scope, unless it shares its scope with the	*
   * enclosing context.					*
   *----------------------------------------------------*/

  bump_list(binding_list_mark = finger_new_binding_list());
  prev_scope    = current_scope;
  current_scope = e->SCOPE;

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(135, current_scope);
    }
# endif

  n = e->OFFSET;
  if(n == 0) true_push_scope_g();

  /*---------------------------------------------------------*
   * Generate the FUNCTION_I instruction and its parameters. *
   *---------------------------------------------------------*/
  t = find_u(e->ty);
  loc1_high = begin_labeled_instr_g();
  gen_g(FUNCTION_I); 
  loc1 = gen_g(0);	/* Label after the function. */
  generate_type_g(t->TY2, 1);
  gen_g(END_LET_I);
  loc2 = gen_g(0);	/* The environment size index. */

  /*---------------------------------------------------------*
   * The function code starts with the name of the function. *
   *---------------------------------------------------------*/

  gen_name_instr_g(exec_kind, exec_name);
  
  /*---------------------------------------------------------*
   * Generate code in a special way for irregular functions. *
   * After the code runs, it pushes its result type onto     *
   * the type stack.					     *
   *---------------------------------------------------------*/

  if(irregular_gen == 1) {
#   ifdef DEBUG
      if(trace_gen) trace_t(136);
#   endif
    generate_exec_g(e->E2, FALSE, 0);
    generate_type_g(find_u(e->ty)->TY2, 0);
  }
  else {
    generate_exec_g(e->E2, TRUE, 0);
  }
  if(local_generate_implicit_pop) gen_g(POP_I);
  gen_g(RETURN_I);
  label_g(loc1_high, loc1);	/* patch */

  /*-----------------------------------------------*
   * Go back to the enclosing scope, and patch the *
   * environment size index byte if necessary.	   *
   *-----------------------------------------------*/

  if(n == 0) {
    current_code_array[loc2] = CURRENT_ENVIRONMENT_SIZE_BYTE;
    true_pop_scope_g();
  }
  current_scope = prev_scope;
  undo_bindings_u(binding_list_mark);
  drop_list(binding_list_mark);

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(137, current_scope);
    }
# endif

}


/****************************************************************
 *		   	GEN_SAME_G				*
 ****************************************************************
 * Generate SAME_E expression e.				*
 ****************************************************************/

PRIVATE void gen_same_g(EXPR *e, Boolean tro, int line)
{
  int envmark;
  EXPR* e1 = e->E1;

  /*----------------------------*
   * Check for a try in a team. *
   *----------------------------*/

  if(e->SAME_MODE == 2) {
    in_try++;
    generate_exec_g(e1, tro, line);
    in_try--;
    return;
  }

  /*-------------------*
   * Check for a team. *
   *-------------------*/

  else if(e->TEAM_MODE != 0) {
    int low_offset, n_ids;

    /*-------------------------------------------------------------*
     * This is a team.  Need to bind the defined ids to locations. *
     *-------------------------------------------------------------*/

    low_offset = top_i(envloc_st);
    n_ids = bind_team_ids_g(e1);
    gen_g(TEAM_I);
    gen_g(n_ids);
    gen_g(low_offset);
  }

  else if(e->extra) {
    EXPR *sse1;
    sse1 = skip_sames(e1);
    if(EKIND(sse1) == LOCAL_ID_E) {

      /*------------------------*
       * This is a final fetch. *
       *------------------------*/

      generate_local_id_g(sse1, TRUE);
      return;
    }
  }
  
  START_SCOPE(envmark);
  generate_exec_g(e1, tro, line);
  if(e->SAME_CLOSE) pop_scope_g(envmark);
}


/****************************************************************
 *		   	GEN_FOR_G				*
 ****************************************************************
 * Generate FOR_E expression e.					*
 ****************************************************************/

PRIVATE void gen_for_g(EXPR *e)
{
  int envmark, looptop;
  Boolean handle_type_bindings, simple;
  package_index loc1, loc1_high, loc2 = 0, loc2_high = 0;
  LIST *binding_list_mark;

  /*----------------------------------------------------*
   * A for loop						*
   * 							*
   *   For p from l do s %For				*
   * 							*
   * is to be translated, where				*
   *   e1 is the translation of Match p = (stack top).	*
   *   e2 is l,						*
   *   e->E3 is s. 					*
   * 							*
   * The code generated is as follows if p is		*
   * a pattern that manifestly cannot fail.		*
   *							*
   *  code for e2					*
   *  LLABEL_I (looptop)				*
   *  GOTO_IF_NIL_I(l1)					*
   *  PUSH_TYPE_BINDINGS_I	(**)			*
   *  SPLIT_I						*
   *  SWAP_I						*
   *  code for e1  (to match p against stack top) 	*
   *  POP_I	 (pops () from match)			*	
   *  code for e3  (s)					*
   *  POP_I	 (pops (), val of s)			*
   *  POP_TYPE_BINDINGS_I	(**)			*
   *  GOTO_I (looptop)					*
   *  LLABEL_I(l1)					*
   *							*
   * The code generated is as follows if p is a   	*
   * pattern that might fail.				*
   *							*
   *  code for e2					*
   *  LLABEL_I (looptop)				*
   *  GOTO_IF_NIL_I(l1)					*
   *  PUSH_TYPE_BINDINGS_I	(**)			*
   *  SPLIT_I						*
   *  SWAP_I						*
   *  LOCK_TEMP_I (to make try put stack in control	*
   *               without value being tested on the    *
   *               stack)				*
   *  TRY_I (l2) TRY_F					*
   *  GET_TEMP_I					*
   *  code for e1  (to match p against stack top) 	*
   *  POP_I	 (pops () from match)			*
   *  THEN_I						*
   *  code for e3  (s)					*
   *  POP_I	 (pops () = val of s)			*
   *  LLABEL_I(l2)					*
   *  POP_TYPE_BINDINGS_I	(**)			*
   *  GOTO_I (looptop)					*
   *  LLABEL_I(l1)					*
   *							*
   * (**) type-binding instructions are only generated  *
   * if either e1 or e3 bind type variables.		*
   *----------------------------------------------------*/

  bump_list(binding_list_mark = finger_new_binding_list());
  simple = e->extra;		/* True if match cannot fail. */
  handle_type_bindings = binds_type_variables(e->E1) || 
			 binds_type_variables(e->E3);

  /*----------------------------------------*
   * Generate e2, the list to iterate over. *
   *----------------------------------------*/

  START_SCOPE(envmark);
  generate_exec_g(e->E2, FALSE, 0);
  pop_scope_g(envmark);

  /*--------------------------------------------*
   * Label the top of the loop, and jump to the	*
   * bottom if the remaining list is empty. 	*
   *--------------------------------------------*/

  looptop   = label_g(0, 0);
  loc1_high = begin_labeled_instr_g();
  gen_g(GOTO_IF_NIL_I);
  loc1 = gen_g(0);       /* patched below */
  if(handle_type_bindings) gen_g(PUSH_TYPE_BINDINGS_I);

  /*---------------------------------------------*
   * Generate instructions to split the list and *
   * do the pattern match. If the match fails,   *
   * jump back to the top of the loop.		 *
   *---------------------------------------------*/

  gen_g(SPLIT_I);
  gen_g(SWAP_I);
  if(!simple) {
    gen_g(LOCK_TEMP_I);
    loc2_high = begin_labeled_instr_g();
    gen_g(TRY_I);
    loc2 = gen_g(0);
    gen_g(TRY_F);
    gen_g(GET_TEMP_I);
  }
  generate_exec_g(e->E1, FALSE, 0);
  gen_g(POP_I);

  /*-------------------------*
   * Generate the loop body. *
   *-------------------------*/

  if(!simple) gen_g(THEN_I);
  generate_exec_g(e->E3, FALSE, 0);
  gen_g(POP_I);

  /*-----------------------*
   * Jump back to the top. *
   *-----------------------*/

  if(!simple) label_g(loc2_high, loc2);
  pop_scope_g(envmark);
  if(handle_type_bindings) gen_g(POP_TYPE_BINDINGS_I);
  gen_long_label_g(looptop);
  gen_g(GOTO_I);
  gen_g(looptop & 0xff);
  label_g(loc1_high, loc1);
  undo_bindings_u(binding_list_mark);
  drop_list(binding_list_mark);
}


/****************************************************************
 *		   	GEN_LOOP_G				*
 ****************************************************************
 * Generate LOOP_E expression e.				*
 ****************************************************************/

PRIVATE void gen_loop_g(EXPR *e, Boolean tro)
{
  int envmark;
  LIST *binding_list_mark;

  /*----------------------------------------------*
   * This expression is a loop, of the form	  *
   * 						  *
   *  Loop p = a (b)				  *
   *						  *
   * where b is a choose-expression that is the   *
   * loop body.  e2 holds the translation of	  *
   *						  *
   *     Match p = (stack-top) %Match (b)	  *
   *						  *
   * and e1 is expression Match p = a, or is 	  *
   * NULL if phrase p=a is not present.		  *
   * The generated code is			  *
   *						  *
   *   code for a				  *
   *   LLABEL_I(top)				  *
   *   PUSH_TYPE_BINDINGS_I			  *
   *   code for e2				  *
   *   COMMIT_TYPE_BINDINGS_I			  *
   *						  *
   * Within e2, RECUR_E expressions generate	  *
   * code to put the new loop control value on    *
   * the stack, pop the type bindings and jump	  *
   * back to top of the loop.  The jump at the	  *
   * RECUR_E gets label top from the  	          *
   * LOOP_E's SCOPE field.			  *
   * It also pops the scope back, and used the	  *
   * top of gen_loop_st to know how far to pop	  *
   * it back.					  *
   *						  *
   * The type binding instructions are only	  *
   * generated if e2 binds type variables.	  *
   * In that case, be sure that the done field of *
   * e is set: any RECUR_E expressions are going  *
   * to look at that to see whether to pop        *
   * the type binding list.			  *
   *----------------------------------------------*/

  if(binds_type_variables(e->E2)) e->done = 1;
  bump_list(binding_list_mark = finger_new_binding_list());
  START_SCOPE(envmark);

  if(e->E1 != NULL_E) {
    generate_exec_g(e->E1->E2, FALSE, 0);
    if(e->OPEN_LOOP == 0) pop_scope_g(envmark);
  }
  e->SCOPE = label_g(0, 0);
  if(e->done) gen_g(PUSH_TYPE_BINDINGS_I);
  push_int(gen_loop_st, top_i(envloc_st));

  generate_exec_g(e->E2, tro, 0);

  if(e->OPEN_LOOP == 0) pop_scope_g(envmark);
  if(e->done) gen_g(COMMIT_TYPE_BINDINGS_I);
  pop(&gen_loop_st);
  undo_bindings_u(binding_list_mark);
  drop_list(binding_list_mark);
}


/****************************************************************
 *		   	GEN_RECUR_G				*
 ****************************************************************
 * Generate RECUR_E expression e.				*
 ****************************************************************/

PRIVATE void gen_recur_g(EXPR *e)
{
  if(e->E1 != NULL_E) {
    generate_exec_g(e->E1, FALSE, 0);
  }

  if(top_i(envloc_st) != top_i(gen_loop_st)) {
    gen_g(EXIT_SCOPE_I);
    gen_g(top_i(gen_loop_st));
  }
  {EXPR* e2 = e->E2;
   if(e2->done) gen_g(POP_TYPE_BINDINGS_I);
   gen_long_label_g(e2->SCOPE);
   gen_g(GOTO_I);
   gen_g(e2->SCOPE & 0xff);
  }
}


/****************************************************************
 *		   	GEN_WHERE_G				*
 ****************************************************************
 * Generate WHERE_E expression e.				*
 ****************************************************************/

PRIVATE void gen_where_g(EXPR *e)
{
  int envmark;

  /*----------------------------------------------------*
   * e is e1 where e2.  Generate it as e1 followed by a *
   * test that e2 is true.			        *
   *----------------------------------------------------*/
  
  generate_exec_g(e->E1, FALSE, 0);
  START_SCOPE(envmark);
  generate_exec_g(e->E2, FALSE, 0);
  pop_scope_g(envmark);
  gen_g(TEST_I);
  gen_g(0);
}


/****************************************************************
 *		   GENERATE_ARG_G				*
 ****************************************************************
 * Generate e, but do not generate PAIR_I instructions at the   *
 * end of the code for e.  Instead, put in *pair_count the	*
 * number of PAIR_I instructions that were omitted.		*
 ****************************************************************/

PRIVATE void
generate_arg_g(EXPR *e, int *pair_count, int line)
{
  if(line == 0) line = e->LINE_NUM;

  e = skip_sames(e);
  if(EKIND(e) == PAIR_E) {
    int pair_count1;

    generate_exec_g(e->E1, FALSE, line);
    generate_arg_g(e->E2, &pair_count1, line);
    *pair_count = 1 + pair_count1;
  }

  else /* EKIND(e) != PAIR_E */ {
    *pair_count = 0;
    generate_exec_g(e, FALSE, line);
  }
}


/****************************************************************
 *		   GEN_NONPRIM_APPL_G				*
 ****************************************************************
 * Generate an application of function e1 to argument e2, with	*
 * tail application used if tro is true.  e1 is not a primitive *
 * function (prim = 0), or is a PRIM_QEQ primitive (prim =	*
 * PRIM_QEQ).							*
 *								*
 * is_irregular is true if e1 is an irregular function.		*
 *								*
 * This function is used only in situations where it is allowed *
 * to evaluate e1 before e2.					*
 ****************************************************************/

PRIVATE void
gen_nonprim_appl_g(EXPR *e1, EXPR *e2, Boolean tro, Boolean is_irregular, 
		   int prim)
{
  EXPR* sse1 = skip_sames(e1);
  int pair_count;

# ifdef DEBUG
    if(trace_gen > 1) trace_t(545);
# endif

  /*--------------------------------------------------*
   * Evaluate the argument e2 now.  Keep track of     *
   * how many pair instructions need to be done on it.*
   *						      *
   * PRIM_QEQ wants two values on the stack, so       *
   * split the top, and do not use pairing	      *
   * improvement. 				      *
   *--------------------------------------------------*/

  if(prim != PRIM_QEQ) {
    generate_arg_g(e2, &pair_count, 0);
  }
  else {
    generate_exec_g(e2, FALSE, 0);
    gen_g(SPLIT_I);
  }

  /*--------------------------------------------------------------*
   * Generate the function.  An irregular function needs to be    *
   * generated by gen_global_id_g here, so that the third 	  *
   * parameter can be made TRUE.  generate_exec_g will reject an  *
   * irregular function. 					  *
   *--------------------------------------------------------------*/

  if(is_irregular) {
    sse1->irregular = 1;
    gen_global_id_g(sse1, TRUE);
  }
  else {
    generate_exec_g(e1, FALSE, 0);
  }

  /*------------------------------------------------------------*
   * Generate an instruction to apply the function to its	*
   * argument.  These instructions expect the function to	*
   * be on top of the stack, and the argument to be below it. 	*
   * We can do a tail application if not suppressed and this	*
   * application is logically followed by RETURN_I.		*
   *								*
   * If any pairing instructions were suppressed above by	*
   * generate_arg_g, then we need to bring them back here by	*
   * doing an application that has built in pairing.		*
   *------------------------------------------------------------*/

  if(prim != PRIM_QEQ) {
    if(tro && !suppress_tro) {
      EXPR*         fun      = get_applied_fun(sse1, FALSE);
      EXPR_TAG_TYPE fun_kind = EKIND(fun);
      if((fun_kind == GLOBAL_ID_E || fun_kind == LOCAL_ID_E) &&
	  no_troq_tm(fun->STR)) {
	if(pair_count > 0) {
	  gen_g(REV_PAIR_APPLY_I); 
	  gen_g(pair_count);
	}
	else gen_g(REV_APPLY_I);
      }
      else {
	if(pair_count > 0) {
	  gen_g(REV_PAIR_TAIL_APPLY_I); 
	  gen_g(pair_count);
	}
        else gen_g(REV_TAIL_APPLY_I);
      }
    }
    else {
      if(pair_count > 0) {
	gen_g(REV_PAIR_APPLY_I); 
	gen_g(pair_count);
      }
      else gen_g(REV_APPLY_I);
    }
  }
  else gen_g(QEQ_APPLY_I);

  /*--------------------------------------------------------------------*
   * If we have just applied an irregular function, then it will come 	*
   * back with the type of its result on the top of the type stack.   	*
   *								  	*
   * Generate code for an irregular function, to check			*
   * that the output type is the same as expected, and to bind 		*
   * variables dynamically where necessary.				*
   * The type on the type stack must be some type in the restriction	*
   * codomain of this irregular function.				*
   *--------------------------------------------------------------------*/

  if(is_irregular) {
    TYPE *stack_type;
    package_index finger;
    Boolean did_something;

    bump_type(stack_type = get_restriction_type_tm(sse1));
    finger = finger_code_array();
    gen_unwrap_test_g(stack_type, e1->ty->TY2, 
		      sse1, &did_something);

    /*-------------------------------------------------------*
     * If gen_unwrap_test did not generate any interesting   *
     * code, then retract anything that it did generate,     *
     * and pop the type from the type stack.		     *
     *-------------------------------------------------------*/

    if(!did_something) {
      retract_code_to(finger);
      gen_g(POP_T_I);
    }
    drop_type(stack_type);

  } /* end if(is_irregular) */
}


/****************************************************************
 *			GEN_PRIM_APPL_G				*
 ****************************************************************
 * Generate an application of function e1 to argument e2, with	*
 * tail application used if tro is true.  Function e1 is a 	*
 * primitive with primitive info (prim, instr, sel_list,	*
 * irregular_prim).						*
 ****************************************************************/

PRIVATE void 
gen_prim_appl_g(EXPR *e1, EXPR *e2,
		INT_LIST *sel_list, int prim, int instr, 
		Boolean irregular_prim)
{
  EXPR* sse1 = skip_sames(e1);
  TYPE* e1_ty = find_u(e1->ty);

# ifdef DEBUG
    if(trace_gen > 1) trace_t(544);
# endif

  /*----------------------------------------------------------------*
   * Be sure that primitive info is installed correctly.  Only need *
   * to worry about EL3 and irregular fields. 			    *
   *----------------------------------------------------------------*/

  SET_LIST(sse1->EL3, sel_list);
  sse1->irregular = irregular_prim;

  /*------------------------*
   * Generate the argument. *
   *------------------------*/

  generate_exec_g(e2, FALSE, 0);

  /*----------------------------------------*
   * Generate the primitive instruction(s). *
   *----------------------------------------*/

  gen_prim_g(prim, instr, e1);

  /*--------------------------------------------------------*
   * If the primitive is irregular, we need to unify the    *
   * result type with the expected type.  However, the cases*
   * of PRIM_UNWRAP and PRIM_DUNWRAP have already been	    *
   * handled in gen_prim_g, so skip them.		    *
   *--------------------------------------------------------*/

  if(irregular_prim && prim != PRIM_UNWRAP && prim != PRIM_DUNWRAP) {

    /*--------------------------------------------------------*
     * Normally, the result type from the irregular primitive *
     * is already on the type stack.  For primitive "forget", *
     * we need to push it now.				      *
     * The argument of forget has type e1->ty->TY1.  	      *
     *--------------------------------------------------------*/

    Boolean did_something;
    package_index finger = finger_code_array();
    if(prim == PRIM_CAST) {
      generate_type_g(e1_ty->TY1, 0);
    }
    gen_unwrap_test_g(e1_ty->TY1, e1_ty->TY2,
		      sse1, &did_something);
    if(!did_something) retract_code_to(finger);
  }
}


/****************************************************************
 *			GEN_FORW_APPL_G				*
 ****************************************************************
 * Generate e1 e2, where e1 must have type ().  This expression *
 * is logically followed by a RETURN_I if tro is true.		*
 ****************************************************************/

PRIVATE void
gen_forw_appl_g(EXPR *e1, EXPR *e2, Boolean tro)
{
# ifdef DEBUG
    if(trace_gen > 1) trace_t(543);
# endif

  /*-------------------------------------------------------------*
   * Generate the left-hand side, e1, then pop (), then generate *
   * the right-hand side e2.					 *
   *-------------------------------------------------------------*/

  generate_exec_g(e1, FALSE, 0);
  gen_g(POP_I);
  generate_exec_g(e2,  tro, 0);
}


/****************************************************************
 *			GEN_APPLICATION_G			*
 ****************************************************************
 * Generate an application of function e1 to argument e2, with	*
 * tail application used if tro is true.			*
 *								*
 * Closed is true if local bindings to identifier should	*
 * be hidden.							*
 ****************************************************************/

PRIVATE void
gen_application_g(EXPR *e1, EXPR *e2, Boolean tro, Boolean closed)
{
  EXPR_TAG_TYPE kind;
  Boolean is_hermit, is_irregular, irregular_prim;
  int prim, instr, envmark;
  EXPR *sse1;
  TYPE *e1_ty;
  PART *part;
  INT_LIST *sel_list;

 /*----------------------------------------------*
  * Get information about the function e1. 	 *
  * (Note that e1 might not be a function, but   *
  * might be an expression that evaluates to (). *
  *----------------------------------------------*/

  prim         = get_prim_g(e1, &instr, &part, &irregular_prim, &sel_list);
  e1_ty        = find_u(e1->ty);
  sse1         = skip_sames(e1);
  kind         = EKIND(sse1);
  is_hermit    = is_hermit_type(e1_ty);
  is_irregular = irregular_prim || 
		 (kind == GLOBAL_ID_E && sse1->irregular);

  /*------------------------------------------------------*
   * If this is a closed apply, we will need to remove    *
   * bindings after doing it.  Keep track of the current  *
   * scope now.						  *
   *------------------------------------------------------*/

  START_SCOPE(envmark);

# ifdef DEBUG
    if(trace_gen > 1) {
      trace_t(145, prim, instr, is_hermit, is_irregular, tro);
      trace_ty(e1_ty);  
      tracenl();
    }
# endif

  /*---------------------------------------------------------*
   * If e1 is a function, not (), then we must evaluate e2   *
   * first, then e1.					     *
   *---------------------------------------------------------*/

  if(!is_hermit) {

    /*--------------------------------------------------------------*
     * If the function is not a primitive, compile it and apply it. *
     * We need to treat a PRIM_QEQ primitive as a non-primitive     *
     * here, since we need to generate its function.		    *
     *--------------------------------------------------------------*/

    if(prim == 0 || prim == PRIM_QEQ) {
      gen_nonprim_appl_g(e1, e2, tro, is_irregular, prim);
    }

    /*-----------------------------------------------------------*
     * If the function is a primitive, just use its instruction. *
     *-----------------------------------------------------------*/

    else /* sse1 is a primitive */ {
      gen_prim_appl_g(e1, e2, sel_list, prim, instr, irregular_prim);
    }
  }

  /*--------------------------------------------*
   * If e1 has type (), then we must evaluate   *
   * e1 first.					*
   *--------------------------------------------*/
    
  else /* is_hermit */ {
    gen_forw_appl_g(e1, e2, tro);
  }

  /*------------------------------------------------------*
   * Clean up after the apply.  Pop the scope if closed,  *
   * and drop the ref count on result.			  *
   *------------------------------------------------------*/

  if(closed) pop_scope_g(envmark);
}


/****************************************************************
 *			BIND_TEAM_IDS_G				*
 ****************************************************************
 * Bind the identifiers defined in e to locations in the	*
 * environment. Return the number of identifiers bound.		*
 ****************************************************************/

PRIVATE int bind_team_ids_g(EXPR *e) 
{
  EXPR *p;
  EXPR_TAG_TYPE kind;
  int n;

  p    = skip_sames(e);
  kind = EKIND(p);
  n    = 0;
  while(kind == APPLY_E) {
    n   += bind_team_ids_g(p->E2);
    p    = skip_sames(p->E1);
    kind = EKIND(p);
  }
  if(kind == DEFINE_E) {
    p = skip_sames(p->E1);
    p->OFFSET = incr_env_g();
#   ifdef DEBUG
      if(trace_gen > 1) {
        trace_t(147, p, toint(p->OFFSET) - 1);
      }
#   endif

    note_id_g(p);
    return n+1;
  }
  else if(kind == CONST_E) return n;
  else {  /* not hermit */
    die(25, toint(EKIND(p)));
    return n;
  }
}
