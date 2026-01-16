/******************************************************************
 * File:    generate/gendcl.c
 * Purpose: generate code for declarations
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

/************************************************************************
 * This file contains functions that generate byte-code for various	*
 * kinds of declarations.						*
 ************************************************************************/

#include "../misc/misc.h"
#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../parser/tokens.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../utils/rdwrt.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../dcls/dcls.h"
#include "../exprs/expr.h"
#include "../error/error.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../generate/generate.h"
#include "../parser/parser.h"
#include "../ids/open.h"
#include "../lexer/modes.h"
#include "../unify/unify.h"
#include "../infer/infer.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE void generate_free_variables_g(EXPR *def);

/****************************************************************
 *			glob_bound_vars				*
 *			other_vars				*
 ****************************************************************
 * List glob_bound_vars is a list of variables that are part of *
 * the type of the identifier being defined at a let		*
 * declaration, excluding lower bounds.  These variables are	*
 * bound to specific species when the species of the identifier *
 * being used is known.						*
 *								*
 * List other_vars holds all variables in a declaration that	*
 * are not in glob_bound_vars.					*
 * 								*
 * Lists glob_bound_vars and other_vars are kept at NIL except	*
 * when being used in processing a declaration.			*
 *								*
 * See generate/gendcl.c, genexec.c, genglob.c, genstd.c, 	*
 * genvar.c, tables/globtbl.c and classes/t_tyutil.c for	*
 * uses.							*
 ****************************************************************/

TYPE_LIST* glob_bound_vars = NIL;
TYPE_LIST* other_vars      = NIL;

/****************************************************************
 *			DCL_INSTR_G				*
 ****************************************************************
 * Return the instruction that declares a previously existing   *
 * thing with a code entry of code in the class table cell.     *
 * Return 0 if code is unknown.					*
 ****************************************************************/

PRIVATE int dcl_instr_g(int code)
{
  switch(code) {
    case MAKE_CODE(GENUS_ID_TOK):
      return GENUS_DCL_I;

    case MAKE_CODE(COMM_ID_TOK):
      return COMMUNITY_DCL_I;

    case MAKE_CODE(TYPE_ID_TOK):
      return SPECIES_DCL_I;

    case MAKE_CODE(FAM_ID_TOK):
      return FAMILY_DCL_I;

    case PAIR_CODE:
      return FAMILY_DCL_I;

    default: return 0;
  }
}


/****************************************************************
 *			GENERATE_EXCEPTION_G			*
 ****************************************************************
 * Generate an EXCEPTION_DCL_I declaration for exception name,  *
 * where the type of name is domain -> ExceptionSpecies.        *
 *								*
 * If trap is TRUE, then this exception is initially trapped.   *
 * This exception has description descr in the declaration.     *
 *								*
 * Return the global label where this exception is declared.    *
 ****************************************************************/

int generate_exception_g(char* name, TYPE* domain, Boolean trap, char* descr)
{
  int n = glabel_g();

  init_global_code_array();
  select_global_code_array();
  generate_type_g(domain, 2);

  write_g(EXCEPTION_DCL_I);
  write_g(trap);

  /*------------------------------------------------------------------*
   * Write the exception name. Note that the 			      *
   * name of the exception is printed with a prefix of the current    *
   * package name and a slash, so that exception myEx in package      *
   * mypack is written as mypack/myEx.  This is used to recognize an  *
   * error situation where the same exception is declared in two      *
   * different packages.					      *
   *------------------------------------------------------------------*/

  fprintf(genf, "%s/%s", current_package_name, name);
  write_g(0);  /* Null terminate the name. */

  /*---------------------------------------------------*
   * Write the description length and the description. *
   *---------------------------------------------------*/

  if(descr != NULL) {
    write_int_m(strlen(descr), genf);
    fprintf(genf, "%s", descr);
    write_g(0);   /* Null terminate the description. */
  }
  else write_int_m(0, genf);

  write_global_code_array();
  write_g(END_LET_I);

  return n;
}


/****************************************************************
 *			GENERATE_STR_DCL_G			*
 ****************************************************************
 * Generate a global label instruction followed by a string 	*
 * declaration for string s.  Return the global label.		*
 *								*
 * If we are importing a package or gencode is false, then	*
 * suppress this generation, and return NO_VALUE.		*
 *								*
 * String declarations can have several kinds: ID_DCL_I or	*
 * STRING_DCL_I or SPECIES_DCL_I, etc.  A declaration of one    *
 * kind will not be substituted for another kind, except that   * 
 * an OLD_ instruction can be handled by a former non-OLD one   * 
 * for a species, genus, etc.					*
 *								*
 * Parameter kind tells what kind to do.  			*
 *								*
 * In the case of REAL_DCL_I and INT_DCL_I, an error might 	*
 * occur (if the string is too long).  In that case, the error  *
 * is reported at line 'line'.					*
 ****************************************************************/

int generate_str_dcl_g(int kind, char *s, int line)
{
  int result, slen;

  if(!gen_code) return NO_VALUE;

  slen = strlen(s);

  /*------------------------------------------------------------*
   * Numbers must not be too long, since the package reader     *
   * (intrprtr/package.c) will not handle them.  So complain    *
   * if a number is too long.					*
   *------------------------------------------------------------*/

  if((kind == REAL_DCL_I && strlen(s) > MAX_REAL_CONST_LENGTH) ||
     (kind == INT_DCL_I && strlen(s) > MAX_INT_CONST_LENGTH)) {
    syntax_error(CONST_TOO_LONG_ERR, line);
    return 0;
  }

  /*----------------------------------------*
   * Generate the global label instruction. *
   *----------------------------------------*/

  result = glabel_g();

# ifdef DEBUG
    if(trace_gen) trace_t(165, s, slen);
# endif

  /*----------------------------------------------------------------*
   * Generate the string declaration, including s, null terminated. *
   * A STRING_DCL_I instruction must be followed by the length of   *
   * the string.						    *
   *----------------------------------------------------------------*/

  write_g(kind);
  if(kind == STRING_DCL_I) write_int_m(slen, genf);
  if(gen_code) fprintf(genf, "%s", s);
  write_g(0);

  return result;
}


/****************************************************************
 *			GENERATE_IRREGULAR_DCL_G		*
 ****************************************************************
 * Generate a IRREGULAR_DCL_I declaration, provided we are      *
 * generating code now.  The declaration should indicate that   *
 * name:ty is irregular.					*
 ****************************************************************/

void generate_irregular_dcl_g(char *name, TYPE *ty)
{
  if(gen_code) {
    int id_index = id_loc(new_name(name,TRUE), ID_DCL_I);
    init_global_code_array();
    select_global_code_array();
    generate_type_g(ty, 2);
    write_g(IRREGULAR_DCL_I);
    write_int_m(id_index, genf);
    write_global_code_array();
    write_g(END_LET_I);
  }
}


/****************************************************************
 *			GENERATE_DEFINE_DCL_G			*
 ****************************************************************
 * Generate define declaration def, for each of the types in	*
 * list types.  MODE is the mode of this declaration.  It is a  *
 * safe pointer: it does not live longer than this function     *
 * call.							*
 *--------------------------------------------------------------*
 * The plan for this function is as follows.			*
 *								*
 * 1. Create glob_bound_vars, holding only the top-level	*
 *    globally bound variables.					*
 *								*
 * 2. Do defaults.  This					*
 *    a. binds variables that need to be bound;			*
 *    b. marks all expressions that might bind variables 	*
 *       dynamically, so the generate_exec_g can recognize them.*
 *    c. complains if any dangerous defaults are done.		*
 *								*
 * 3. Generate the types from list types in the global code	*
 *    array.							*
 *								*
 * 4. Set aside a byte to hold the number of globals needed.	*
 *    We do not yet know how many globals there will be, but	*
 *    will backpatch later.					*
 *								*
 * 5. Do a take_apart.  This					*
 *    a. sets aside space at the start of the global env for 	*
 *       variables in glob_bound_vars;				*
 *    b. sets the TOFFSETs of the globally bound variables;	*
 *    c. binds the globally bound variables.			*
 *								*
 * 6. Scan the entire expression, accumulating all variables 	*
 *    that are not globally bound.  For each,			*
 *    a. set aside space in the global environment;		*
 *    b. set the TOFFSET field;					*
 *    c. generate code to create the variable, as a raw		*
 *       variable (no constraints)				*
 *    d. add the variable to list other_vars.			*
 *								*
 * 7. Generate all constraints on variables.  (Traverse 	*
 *    glob_bound_vars and other_vars.)				*
 *								*
 * 8. Generate the code, using generate_exec_g.  This will put  *
 *    types and global ids in global_list.			*
 *								*
 * 9. Generate the globals in global_list.  Also update the 	*
 *    byte holding the number of globals.  			*
 *								*
 * 10. Write the define declaration to the code array, by	*
 *     a. writing the declaration start;			*
 *     b. writing the global code;				*
 *     c. writing the executabel code;				*
 *     d. finishing the declaration.				*
 ****************************************************************/

void generate_define_dcl_g(EXPR *def, TYPE_LIST *types, MODE_TYPE *mode)
{
  char *name;
  package_index global_env_byte;
  int id_index, ok, num_locals;
  EXPR *id;
  LIST *umark;

  /*-------------------------------------------------*
   * Don't generate code if there was a prior error. *
   *-------------------------------------------------*/

  if(local_error_occurred) return;
  bump_expr(def);

  /*------------------------------------------------------------*
   * Get the name to be defined, and declare it so that it can	*
   * be referred to via a global label.				*
   *------------------------------------------------------------*/

  id       = skip_sames(def->E1);
  name     = new_name(id->STR, TRUE);
  id_index = id_loc(name, ID_DCL_I);

# ifdef DEBUG
    if(trace_gen) {
      trace_t(166, name, get_define_mode(mode));
      print_type_list_separate_lines(types, "  ");
      if(trace_exprs) print_expr(def, 0);
    }
# endif

  /*------------------------------------------------------------*
   * Initialize for the declaration.				*
   *  a. Start the local labels for this declaration at 0. 	*
   *  b. Compute the scope offsets. 				*
   *------------------------------------------------------------*/

  next_llabel = 0;
  fix_scopes_array_g();

  /*------------------------------------*
   * Step 1. Set up glob_bound_vars. 	*
   *------------------------------------*/

  bind_glob_bound_vars(id->ty, mode);

  /*----------------------------------------------------*
   * Step 2. Default any type or family variables that	*
   * need to be bound.	After doing defaults, do a show	*
   * if requested.  If a show is done earlier, it will	*
   * not reflect default handling.			*
   *							*
   * We need to be able to back out of default bindings *
   * since there might be another use of this 		*
   * expression to follow (if this is an overloaded	*
   * declaration.)					*
   *----------------------------------------------------*/

  bump_list(umark = finger_new_binding_list());
  process_defaults_tc(def, mode, TRUE, TRUE);

  /*------------------------------------------------------------*
   * Step 3. Generate the types of this identifier in the 	*
   * glob_code_array. All but the last are followed by		*
   * END_LET_I instructions.  The last is followed by ENTER_I.	*
   *------------------------------------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(167);
# endif

  init_global_code_array();
  select_global_code_array();

  {TYPE_LIST *q;
   for(q = types; q != NULL; q = q->tail) {
     generate_type_g(q->head.type, 2);
     if(q->tail == NULL) gen_g(ENTER_I);
     else                gen_g(END_LET_I);
   }
  }

  /*--------------------------------------------------------------*
   * Step 4. Generate a byte that will hold the index in env_size *
   * of the number of globals needed.  This byte is set to 0	  *
   * here, but will be backpatched later.			  *
   *--------------------------------------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(168);
# endif

  num_globals     = 0;
  global_env_byte = gen_g(0);   /* In glob_code_array */

  /*----------------------------------------------------*
   * Step 5. Generate code to bind the variables that 	*
   * are in glob_bound_vars to their correct values.	*
   * If id is irregular, then only bind the variables	*
   * in the domain.					*
   *----------------------------------------------------*/

  take_apart_g(id->ty, mode);

  /*----------------------------------------------------*
   * Step 6. Find all variables that are not globally	*
   * bound, and generate them as raw variables in the 	*
   * global_code_array.	  Also put them in list		*
   * other_vars.					*
   *----------------------------------------------------*/

  generate_free_variables_g(def);

  /*----------------------------------------------------*
   * Step 7. Generate all constraints on variables.	*
   * Note that the variables are all now in the global	*
   * global environment, so they can be fetched.	*
   *----------------------------------------------------*/

  generate_constraints_g(glob_bound_vars);
  generate_constraints_g(other_vars);
   
  /*----------------------------------------------------*
   * Step 8. Generate the body of the definition in the	*
   * exec_code_array. 					*
   *----------------------------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(169);
# endif

  init_exec_code_array();
  select_exec_code_array();
  irregular_gen = has_mode(mode, IRREGULAR_MODE) ? -1 : 1;
  num_locals    = generate_code_g(&(def->E2), 1, ID_DCL_I, name);

  /*----------------------------------------------------*
   * Step 9. Update the byte holding the index in	*
   * env_size of the number of globals needed. 		*	
   * Generate global identifier extraction instructions *
   * in the global code array.				*
   *----------------------------------------------------*/

  glob_code_array[global_env_byte] = un_env_size_g(num_globals, FALSE);

# ifdef DEBUG
    if(trace_gen) trace_t(170);
# endif

  select_global_code_array();
  ok = generate_globals_g();
  gen_g(STOP_G_I);
  if(!ok) goto out;

  /*----------------------------------------------------*
   * Step 10. Now copy the information from the code	*
   * arrays  to the file.				*
   * First, start the define declaration. 		*
   *----------------------------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(171);
# endif

  write_g(DEFINE_DCL_I);
  write_int_m(id_index, genf);
  write_g((UBYTE)(get_define_mode(mode) & 0xFF));

  /*---------------------------*
   * Put the global part here. *
   *---------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(172);
# endif

  code_out_g(glob_code_array, &glob_genloc);

  /*--------------------------------------*
   * Generate the number of locals needed.*
   *--------------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(173);
# endif

  write_g(un_env_size_g(num_locals, TRUE));

  /*---------------------------------*
   * Place the executable code here. *
   *---------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(174);
# endif

  code_out_g(exec_code_array, &exec_genloc);

  /*----------------------*
   * End the declaration. *
   *----------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(175);
# endif

  write_g(END_I);

# ifdef DEBUG
    if(trace_gen) trace_t(176, name);
# endif

out:
  undo_bindings_u(umark);
  drop_list(umark);
  SET_LIST(glob_bound_vars, NIL);
  SET_LIST(other_vars, NIL);
  clear_locs_t(id->ty);
  drop_expr(def);
}


/****************************************************************
 *			GENERATE_EXECUTE_DCL_G			*
 ****************************************************************
 * Generate an execute declaration with body e.	 MODE is a      *
 * safe pointer: it does not live longer than this call.	*
 *--------------------------------------------------------------*
 * The plan for this function is as follows.			*
 *								*
 * 1. Create glob_bound_vars, holding only the top-level	*
 *    globally bound variables.					*
 *								*
 * 2. Do defaults.  This					*
 *    a. binds variables that need to be bound;			*
 *    b. marks all expressions that might bind variables 	*
 *       dynamically, so the generate_exec_g can recognize them.*
 *    c. complains if any dangerous defaults are done.		*
 *								*
 * 3. Generate the types from list types in the global code	*
 *    array.							*
 *								*
 * 4. Set aside a byte to hold the number of globals needed.	*
 *    We do not yet know how many globals there will be, but	*
 *    will backpatch later.					*
 *								*
 * 5. Do a take_apart.  This					*
 *    a. sets aside space at the start of the global env for 	*
 *       variables in glob_bound_vars;				*
 *    b. sets the TOFFSETs of the globally bound variables;	*
 *    c. binds the globally bound variables.			*
 *								*
 * 6. Scan the entire expression, accumulating all variables 	*
 *    that are not globally bound.  For each,			*
 *    a. set aside space in the global environment;		*
 *    b. set the TOFFSET field;					*
 *    c. generate code to create the variable, as a raw		*
 *       variable (no constraints)				*
 *    d. add the variable to list other_vars.			*
 *								*
 * 7. Generate all constraints on variables.  (Traverse 	*
 *    glob_bound_vars and other_vars.)				*
 *								*
 * 8. Generate the code, using generate_exec_g.  This will put  *
 *    types and global ids in global_list.			*
 *								*
 * 9. Generate the globals in global_list.  Also update the 	*
 *    byte holding the number of globals.  			*
 *								*
 * 10. Write the define declaration to the code array, by	*
 *     a. writing the declaration start;			*
 *     b. writing the global code;				*
 *     c. writing the executabel code;				*
 *     d. finishing the declaration.				*
 ****************************************************************/

PRIVATE int execute_num = 0;  /* Number in this package, used to name. */
PRIVATE int hidden_execute_num = 0;

void generate_execute_dcl_g(EXPR *e, MODE_TYPE *mode)
{
  int num_locals, n_glob;
# ifdef SMALL_STACK
    char *name;
# else
    char name[MAX_PACKAGE_NAME_LENGTH + 10];
# endif

  /*-------------------------------------------------*
   * Don't generate code if there was a prior error. *
   *-------------------------------------------------*/

  if(local_error_occurred) return;

# ifdef SMALL_STACK
    name = (char *) BAREMALLOC(MAX_PACKAGE_NAME_LENGTH + 10);
# endif

  bump_expr(e);

# ifdef DEBUG
    if(trace_gen) {
      trace_t(177);
#     ifdef NEVER
        print_expr(e,1);
#     endif
    }
# endif

  /*------------------------------------------------------------*
   * Initialize for the declaration.				*
   *  a. Start the local labels for this declaration at 0. 	*
   *  b. Compute the scope offsets. 				*
   *------------------------------------------------------------*/

  next_llabel = 0;
  fix_scopes_array_g();

  /*----------------------------------------------------*
   * Step 1. Default any type or family variables that	*
   * need to be bound.	After doing defaults, do a show	*
   * if requested.  If the show is done earlier, it	*
   * will not reflect default handling.			*
   *----------------------------------------------------*/

  glob_bound_vars = NIL;
  process_defaults_tc(e, 0, TRUE, TRUE);

  /*----------------------------------------------------*
   * Step 2. Find all type variables that are used	*
   * and generate them as raw variables in the 		*
   * global_code_array.	  Also put them in list		*
   * other_vars.					*
   *----------------------------------------------------*/

  num_globals = 0;
  init_global_code_array();
  select_global_code_array();

  generate_free_variables_g(e);

  /*----------------------------------------------------*
   * Step 3. Generate all constraints on variables.	*
   * Note that the variables are all now in the global	*
   * global environment, so they can be fetched.	*
   *----------------------------------------------------*/

  generate_constraints_g(other_vars);
   
  /*----------------------------------------------------*
   * Step 4. Generate the expression, placing it in the *
   * exec_code_array. 					*
   *----------------------------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(178, exec_code_array);
# endif

  init_exec_code_array();
  select_exec_code_array();
  if(has_mode(mode, HIDE_MODE)) {
    hidden_execute_num++;
    sprintf(name, "Hidden-exec-%s-%d", current_package_name, 
	    hidden_execute_num);
  }
  else {
    execute_num++;
    sprintf(name, "Exec-%s-%d", current_package_name, execute_num);
  }
  irregular_gen = 1;
  num_locals = generate_code_g(&e, 1, STRING_DCL_I, name);

  /*----------------------------------------------------*
   * Step 5. Generate global identifier extraction	*
   * instructions in the global code array. 		*
   *----------------------------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(179);
# endif

  select_global_code_array();
  n_glob  = un_env_size_g(num_globals, FALSE);
  generate_globals_g();
  gen_g(STOP_G_I);

  /*-----------------------------------------------*
   * Step 6. Now copy the code from the arrays to  *
   * the file. 					   *
   * First start the declaration. 		   *
   *-----------------------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(180);
# endif

  write_g(has_mode(mode, HIDE_MODE) ? HIDDEN_EXECUTE_I : EXECUTE_I);
  write_g(n_glob);

  /*------------------------------*
   * Put the global id part here. *
   *------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(181);
# endif

  code_out_g(glob_code_array, &glob_genloc);

  /*-----------------------------------*
   * Generate number of locals needed. *
   *-----------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(182);
# endif

  write_g(un_env_size_g(num_locals, TRUE));

  /*--------------------------------------*
   * Copy the code array to the out file. *
   *--------------------------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(174);
# endif

  code_out_g(exec_code_array, &exec_genloc);

  /*----------------------*
   * End the declaration. *
   *----------------------*/

# ifdef DEBUG
    if(trace_gen) trace_t(175);
# endif

  write_g(END_I);
  drop_expr(e);

# ifdef SMALL_STACK
    FREE(name);
# endif
}


/****************************************************************
 *			GENERATE_IMPORT_DCL_G			*
 ****************************************************************
 * Generate a declaration to import file fname.			*
 ****************************************************************/

void generate_import_dcl_g(char *fname)
{
  write_g(IMPORT_I);
  fprintf(genf, "%s", reduced_aso_name(fname));
  write_g(0); /* Null terminate the name. */
}  


/****************************************************************
 *			GENERATE_DEFAULT_DCL_G			*
 ****************************************************************
 * Generate declaration Default C => t.  If t is NULL, then     *
 * generate declaration Default C => ?.				*
 ****************************************************************/

void generate_default_dcl_g(CLASS_TABLE_CELL *C, TYPE *t)
{
  int C_instr = dcl_instr_g(C->code);
  int C_index = id_loc(C->name, C_instr);

# ifdef DEBUG
    if(trace_gen) trace_t(234, C->name);
# endif

  init_global_code_array();
  select_global_code_array();
  genP_g(DEFAULT_DCL_I, C_index);
  gen_g(C->closed);
  if(t != NULL) generate_type_g(t, 2);
  gen_g(END_LET_I);
  write_global_code_array();
}


/****************************************************************
 *			GENERATE_RELATE_DCL_G			*
 ****************************************************************
 * Generate a declaration stating that a is an ancestor of b.   *
 ****************************************************************/

void generate_relate_dcl_g(CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b)
{
  int a_index, b_index, a_instr, b_instr;

# ifdef DEBUG
    if(trace_gen) trace_t(195, a->name, b->name);
# endif

  /*----------------------------------------------------*
   * Get the global labels where a and b are declared.  *
   * (If a and/or b have not yet been declared, declare *
   * them now.)						*
   *----------------------------------------------------*/

  a_instr = dcl_instr_g(a->code);
  a_index = id_loc(a->name, a_instr);
  b_instr = dcl_instr_g(b->code);
  b_index = id_loc(b->name, b_instr);

  /*---------------------------*
   * Generate the declaration. *
   *---------------------------*/

  write_g(RELATE_DCL_I);
  write_int_m(b_index, genf);
  write_int_m(a_index, genf);
}


/****************************************************************
 *			GENERATE_MEET_DCL_G			*
 ****************************************************************
 * Generate a declaration stating that the intersection of A 	*
 * and B is C, where						*
 *    A is given either by a (if instr = MEET_DCL_I) or aname   *
 *      (if instr = AHEAD_MEET_DCL_I)				*
 *    B is given either by b (if instr = MEET_DCL_I) or bname   *
 *      (if instr = AHEAD_MEET_DCL_I)				*
 *    C is given either by c (if instr = MEET_DCL_I) or cname   *
 *      (if instr = AHEAD_MEET_DCL_I)				*
 *								*
 * instr is the instruction to use.  It must be one of		*
 * MEET_DCL_I and AHEAD_MEET_DCL_I.				*
 ****************************************************************/

void generate_meet_dcl_g(CLASS_TABLE_CELL *a, char *aname, 
			 CLASS_TABLE_CELL *b, char *bname,
			 CLASS_TABLE_CELL *c, char *cname, int instr)
{
  int a_index, b_index, c_index, a_instr, b_instr, c_instr;

  if(instr == MEET_DCL_I) {
    aname = a->name;
    bname = b->name;
    cname = c->name;
  }

# ifdef DEBUG
    if(trace_gen) trace_t(12, aname, bname, cname);
# endif

  /*----------------------------------------------------*
   * Get the global labels where a and b are declared.  *
   * (If a and/or b have not yet been declared, declare *
   * them now.)						*
   *----------------------------------------------------*/

  if(instr == MEET_DCL_I) {
    a_instr = dcl_instr_g(a->code);
    b_instr = dcl_instr_g(b->code);
    c_instr = dcl_instr_g(c->code);
  }
  else {
    a_instr = b_instr = c_instr = NAME_DCL_I;
  }
  a_index = id_loc(aname, a_instr);
  b_index = id_loc(bname, b_instr);
  c_index = id_loc(cname, c_instr);

  /*---------------------------*
   * Generate the declaration. *
   *---------------------------*/

  write_g(instr);
  write_int_m(a_index, genf);
  write_int_m(b_index, genf);
  write_int_m(c_index, genf);
}


/****************************************************************
 *			BIND_GLOB_BOUND_VARS			*
 ****************************************************************
 * Set up glob_bound_vars for processing defaults in a		*
 * declaration.  Type t is the type of the identifier that is	*
 * being defined, and MODE is the mode of the declaration.	*
 *								*
 * The variables in t are globally bound, unless this		*
 * is an irregular definition, in which case only the 		*
 * variables in the domain of t are globally bound. Put the 	*
 * globally bound variables into glob_bound_vars.  		*
 *								*
 * MODE is a safe pointer.  It only lives as long as this	*
 * function call.						*
 ****************************************************************/

void bind_glob_bound_vars(TYPE *t, MODE_TYPE *mode)
{
  TYPE* gb_type = has_mode(mode, IRREGULAR_MODE) ? find_u(t)->TY1 : t;
  bump_list(glob_bound_vars  = copy_vars_to_list_t(gb_type, NIL, NIL));
} 


/****************************************************************
 *			GENERATE_FREE_VARIABLES_G		*
 ****************************************************************
 * Find all variables in types in def that are not in		*
 * glob_bound_vars.  For each,					*
 *  a. add it to list other_vars;				*
 *  b. set aside space in the global environment for it.	*
 *  c. set its TOFFSET field.					*
 *  d. generate it as a raw variable				*
 ****************************************************************/

PRIVATE Boolean generate_free_variables_help(EXPR **e)
{
  gen_free_vars((*e)->ty);
  return 0;
}

/*---------------------------------------------------------*/

PRIVATE void generate_free_variables_g(EXPR *def)
{
  scan_expr(&def, generate_free_variables_help, FALSE);
}

