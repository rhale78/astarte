/*********************************************************************
 * File:    standard/stdfuns.c
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

/*********************************************************************
 * This file defines the standard things, such as functions, for     *
 * the compiler.						     *
 *********************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../unify/unify.h"
#include "../evaluate/instruc.h"
#include "../lexer/modes.h"
#include "../parser/parser.h"
#include "../tables/tables.h"
#include "../standard/stdtypes.h"
#include "../standard/stdfuns.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../error/error.h"
#include "../dcls/dcls.h"
#include "../generate/prim.h"
#include "../generate/generate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			PUBLIC VARIABLES 			*
 ****************************************************************/

/****************************************************************
 *			std_fun_descr				*
 ****************************************************************
 * std_fun_descr is a chain of PAIR_E nodes that describe       *
 * the standard functions (and nonfunctions).  Each time a 	*
 * standard entity is declared, it is put in the std_fun_descr 	*
 * chain.  When generating the standard package "standard.asi", *
 * genstd.c generates definitions for the standard entities in	*
 * standard.aso.						*
 *								*
 * The std_fun_descr chain is linked through the E2 field of	*
 * the EXPR nodes.  The relevant parts of the EXPR nodes are	*
 *								* 
 * STR3		Name of the primitive entity.			*
 * ty		Species of the primitive entity.		*
 * PRIMITIVE	The primitive kind (such as PRIM_FUN)		*
 * LINE_NUM	The argument of the primitive.			*
 * SAME_E_DCL_MODE The mode of the declaration.			*
 * E1		An expr node that describes the identifier	*
 *		being defined.					*
 * E2		link to next item				*
 ****************************************************************/

EXPR* std_fun_descr = NULL;

/****************************************************************
 *			import_prim_fun_descr			*
 ****************************************************************
 * import_prim_fun_descr is a chain similar to std_fun_descr    *
 * of nodes that describe primitive entities.  The entities in  *
 * this chain are created at an import of the form 		*
 *								*
 *    Import "-name".						*
 *								*
 * where name is the name of some primitive group.  See		*
 * std_fun_descr on details of form.				*
 ****************************************************************/

EXPR* import_prim_fun_descr = NULL;


/****************************************************************
 *			PRIVATE VARIABLES 			*
 ****************************************************************/

/****************************************************************
 *			TESTEX_FAIL_ROLE			*
 *			SIDEEFFECT_FAIL_ROLE			*
 *			DOMAINEX_FAIL_ROLE			*
 *			EMPTYLISTEX_FAIL_ROLE			*
 *			NOFILEEX_FAIL_ROLE			*
 *			CONVERSIONEX_FAIL_ROLE			*
 ****************************************************************
 * These roles indicate potential failure of a function with    *
 * a given exception.						*
 *								*
 * All of these are given values in stdfuns.			*
 ****************************************************************/

PRIVATE ROLE *testEx_fail_role;
PRIVATE ROLE *sideEffect_role;
PRIVATE ROLE *domainEx_fail_role;
PRIVATE ROLE *emptyListEx_fail_role;
PRIVATE ROLE *noFileEx_fail_role;
PRIVATE ROLE *conversionEx_fail_role;


/****************************************************************
 * 			STD_FUNS				*
 ****************************************************************
 * Declare primitive functions and symbols.  			*
 *								*
 * NOTE: If you modify this function, you will almost certainly *
 * need to recompile standard.asi.				*
 *                                                              *
 * WARNING: the following will lose 'type' to the memory manager*
 * if it has a 0 ref cnt.                                       *
 *								*
 * The functions to use to define primitives are:		*
 *                                                              *
 *     operator_tm(name, op,mode)				*
 *        -- declare name to be a binary operator of kind op,	*
 *           with given declaration mode.			*
 *								*
 *     sym_op_tm(sym, op)					*
 *	  -- same as operator_tm(std_id[sym], op).		*
 *								*
 *     prim_unop_tm(name)					*
 *        -- declare name to be a unary operator.		*
 *								*
 *     sym_prim_unop_tm(sym)					*
 *        -- same as prim_unop_tm(sym, op)			*
 *                                                              *
 *     primitive_tm(name, type, kind, instr)                    *
 *        -- name:type is implemented as primitive 'kind', with *
 *	     arg 'instr'.					*
 *        -- name should be a string constant (statically 	*
 *	     allocated).					*
 *                                                              *
 *     sym_prim_tm(sym, type, kind, instr)                      *
 *        -- same as primitive_tm(std_id[sym], type, kind, 	*
 *	     instr).						*
 *                                                              *
 *     mode_primitive_tm(name, type, role, kind, instr, mode)   *
 *        -- similar to primitive_tm, but for case where a role *
 *	     and/or mode other than the default is desired.  	*
 *								*
 *     sym_mode_prim_tm(sym, type, role, kind, instr, mode)	*
 *	  -- same as mode_primitive_tm(std_id[sym], type, role,	*
 *	     kind, instr, mode).				*
 *                                                              *
 *     prim_const_tm(name, type, val)                           *
 *        -- name:type is a constant represented by integer val.*
 *								*
 *     sym_prim_const_tm(sym, type, val)			*
 *        -- same as prim_const_tm(std_id[sym], type, val).	*
 *                                                              *
 *     expect_sym_prim_tm(sym, type, mode, kind)              	*
 *        -- issue an expectation for std_id[sym]:type, with 	*
 *	     given mode. This is				*
 *	       an expectation  if kind = EXPECT_ATT,		*
 *	       an anticipation if kind = ANTICIPATE_ATT		*
 *             should not be noted as either an anticipation or *
 *	       an expectation if kind = 0.			*
 *                                                              *
 *     issue_missing_tm(name, type, mode)			*
 *	  -- declare name:t as missing. mode is the mode of	*
 *           the declaration.					*
 ****************************************************************/

void std_funs(void)
{
  /*------------------------------------------------------------*
   * The following are used throughout.  Don't use variables 	*
   * any_type, etc. here from type.c, because variables used 	*
   * here are not copied, and using the variables from type.c 	*
   * will result in the same variable node being used where 	*
   * two different variable nodes are required. 		*
   *------------------------------------------------------------*/

  TYPE *s_any_type, *s_any_type2, *s_any_list, *s_any_box;
  TYPE *s_any_EQ, *s_any_pair, *s_any_pair_rev;
  MODE_TYPE *irregular_mode, *underrides_mode, *strong_mode;

  bmp_type(s_any_type     = var_t(NULL));
  bmp_type(s_any_type2    = var_t(NULL));
  bmp_type(s_any_list     = list_t(s_any_type));
  bmp_type(s_any_box      = box_t(s_any_type));
  bmp_type(s_any_pair     = pair_t(s_any_type, s_any_type2));
  bmp_type(s_any_pair_rev = pair_t(s_any_type2, s_any_type));
  bmp_type(s_any_EQ	  = var_t(EQ_ctc));

  bump_role(testEx_fail_role = basic_role("pot__fail__testEx"));
  bump_role(sideEffect_role = basic_role(pot_side_effect));
  bump_role(domainEx_fail_role = basic_role("pot__fail__domainEx"));
  bump_role(emptyListEx_fail_role = basic_role("pot__fail__emptyListEx"));
  bump_role(noFileEx_fail_role = basic_role("pot__fail__noFileEx"));
  bump_role(conversionEx_fail_role = basic_role("pot__fail__conversionEx"));

  bump_mode(underrides_mode = simple_mode(UNDERRIDES_MODE));
  bump_mode(irregular_mode  = simple_mode(IRREGULAR_MODE));
  bump_mode(strong_mode     = simple_mode(STRONG_MODE));

# ifdef DEBUG
    if(trace) fprintf(TRACE_FILE,"starting std_funs\n");
# endif

  /***************** Standard Binary Operators *************************/

  sym_op_tm(L9_OP_ID,		L9_OP);			/* highest prec. */

  sym_op_tm(R9_OP_ID,		R9_OP);
  sym_op_tm(COMPOSE_ID,	  	R9_OP);

  sym_op_tm(L8_OP_ID,		L8_OP);
  sym_op_tm(SHARP_SYM,          L8_OP);
  sym_op_tm(DBL_SHARP_SYM, 	L8_OP);
  sym_op_tm(AS_ID,  		L8_OP);

  sym_op_tm(R8_OP_ID,		R8_OP);
  sym_op_tm(HAT_SYM,		R8_OP);
  sym_op_tm(HAT_SLASH_SYM,	R8_OP);

  sym_op_tm(L7_OP_ID,		L7_OP);
  sym_op_tm(STAR_SYM,		L7_OP);
  sym_op_tm(SLASH_SYM,		L7_OP);
  sym_op_tm(DBL_SLASH_SYM,	L7_OP);
  sym_op_tm(MOD_ID, 		L7_OP);
  sym_op_tm(DIV_ID, 		L7_OP);

  sym_op_tm(R7_OP_ID,		R7_OP);

  sym_op_tm(L6_OP_ID,		L6_OP);
  sym_op_tm(PLUS_SYM,		L6_OP);
  sym_op_tm(MINUS_SYM,		L6_OP);
  sym_op_tm(DBL_MINUS_SYM,      L6_OP);

  sym_op_tm(R6_OP_ID,		R6_OP);

  sym_op_tm(L5_OP_ID,		L5_OP);
  sym_op_tm(DBL_DOLLAR_SYM,	L5_OP);
  sym_op_tm(UPTO_ID,		L5_OP);
  sym_op_tm(DOWNTO_ID,		L5_OP);

  sym_op_tm(R5_OP_ID,		R5_OP);
  sym_op_tm(CONS_SYM, 		R5_OP);

  sym_op_tm(L4_OP_ID,		L4_OP);

  sym_op_tm(R4_OP_ID,		R4_OP);
  sym_op_tm(DBL_PLUS_SYM,    	R4_OP);

  sym_op_tm(EQ_SYM,		COMPARE_OP);
  sym_op_tm(NE_SYM,		COMPARE_OP);
  sym_op_tm(LT_SYM,		COMPARE_OP);
  sym_op_tm(LE_SYM,		COMPARE_OP);
  sym_op_tm(GT_SYM,		COMPARE_OP);
  sym_op_tm(GE_SYM,		COMPARE_OP);
  sym_op_tm(EQ_ID,		COMPARE_OP);
  sym_op_tm(NEQ_ID,		COMPARE_OP);

  sym_op_tm(L3_OP_ID,		L3_OP);

  sym_op_tm(R3_OP_ID,		R3_OP);
  sym_op_tm(ANDF_ID,		R3_OP);

  sym_op_tm(L2_OP_ID,		L2_OP);

  sym_op_tm(R2_OP_ID,		R2_OP);
  sym_op_tm(ORF_ID,		R2_OP);

  sym_op_tm(L1_OP_ID,		L1_OP);
  sym_op_tm(WHERE_ID,		L1_OP);
  sym_op_tm(ALSO_ID,		L1_OP);

  sym_op_tm(R1_OP_ID,		R1_OP);
  sym_op_tm(IMPLIES_SYM,	R1_OP);

  sym_op_tm(DBL_BAR_SYM,        BAR_OP);
  sym_op_tm(BAR_ID,      	BAR_OP);

  sym_op_tm(COLON_EQUAL_SYM,	COLON_EQUAL_OP); 	 /* lowest prec. */


  /***************** Standard Unary Operators *************************/

  sym_prim_unop_tm(DOLLAR_SYM);
  sym_prim_unop_tm(PLUS_SYM);
  sym_prim_unop_tm(MINUS_SYM);
  sym_prim_unop_tm(SLASH_SYM);
  sym_prim_unop_tm(DBL_SLASH_SYM);
  sym_prim_unop_tm(TILDE_SYM);
  sym_prim_unop_tm(DBL_HAT_SYM);
  sym_prim_unop_tm(AT_SYM);
  sym_prim_unop_tm(NOT_ID);


  /******************** Functions, Application *************************/

  {TYPE *dollar_type, *any_fun, *force_type, *short_apply_type;

   bmp_type(any_fun     = function_t(s_any_type, s_any_type2));
   bmp_type(dollar_type = function_t(any_fun, string_type));
   bmp_type(force_type  = function_t(s_any_type, s_any_type));
   bmp_type(short_apply_type = function_t(pair_t(any_fun, s_any_type),
					  s_any_type2));

   sym_prim_tm(EAGERFUNCTIONTOSTRING_ID,dollar_type,STD_FUN,TO_STRING1_STDF);
   sym_prim_tm(IDF_ID,            idf_type, PRIM_CAST, 0);
   sym_prim_tm(CLOSE_ID,          idf_type, PRIM_CAST, 0);
   sym_prim_tm(FORCE_ID,          force_type, STD_FUN, FORCE_STDF);
   sym_prim_tm(TOPFORCE_ID,	  force_type, PRIM_FUN, TOPFORCE_I);
   sym_prim_tm(SHORTAPPLY_ID,     short_apply_type, PRIM_OP, SHORT_APPLY_I);

   if(compiling_standard_asi) {
     TYPE *getType_type;
     no_gen_prim = TRUE;
     bmp_type(getType_type = function_t(s_any_type, hermit_type));

     primitive_tm("ZZ__GetType", getType_type, TY_PRIM_FUN, GET_TYPE_I);

     no_gen_prim = FALSE;
     drop_type(getType_type);
   }

   drop_type(any_fun);
   drop_type(dollar_type);
   drop_type(force_type);
   drop_type(short_apply_type);

  } /* end functions/application */

  /************************* Conversion to string *********************/

  {TYPE *dollar_type;

   bmp_type(dollar_type   = function_t(s_any_type, string_type));

   expect_sym_prim_tm(DOLLAR_SYM,   dollar_type, 0, ANTICIPATE_ATT);

   sym_mode_prim_tm(LAZYDOLLAR_ID, dollar_type, testEx_fail_role,
				   PRIM_FUN, LAZY_DOLLAR_I, NULL);

   drop_type(dollar_type);
  } /* end conversions to string */

  /************************* Threads *******************************/

  {TYPE *pause_type;

   bmp_type(pause_type = function_t(hermit_type, hermit_type));

   sym_prim_tm(PAUSE_ID,   pause_type, PRIM_FUN, PAUSE_I);
   sym_prim_tm(REPAUSE_ID, pause_type, PRIM_FUN, REPAUSE_I);
   sym_prim_tm(CUT_ID,     pause_type, PRIM_FUN, CUT_I);

   drop_type(pause_type);
  }

  /********************* Arithmetic **************************/

  {TYPE *sring_var, *sfield_var,
	*real_var, *real_var2, *real_var3, 
        *rational_var, *int_var, *int_var2,
	*wnum_var, *wsfield_var, *ps_real_var;

   TYPE *sring_pair, 		*sfield_pair;

   TYPE *real_pair, 		*real_real_pair, 	*int_pair,
	*int_int_pair,		*real_nat_pair, 	*int_nat_pair,
       	*nat_pair,		*realt_2nat_pair,
        *realt_3nat_pair,
	*sfield_intt_pair,	*nat_sfield_pair;

   TYPE *real_pair_to_real_fun, 	*nat_to_intt_fun,
	*sring_pair_to_sring_fun, 	*sring_to_sring_fun,
	*nat_pair_to_nat_fun, 		*sfield_pair_to_sfield_fun,
	*sfield_to_sfield_fun, 		*int_int_to_ratt_fun,
	*int_to_ratt_fun,         	*real_to_real_fun,
	*sfield_intt_to_sfield_fun, 	*compat_type,
	*int_nat_to_int_nat_fun, 	*int_nat_to_int_fun,
	*int_nat_to_nat_fun, 		*int_pair_to_int_fun,
 	*hermit_to_realt_fun,
	*int_to_bool_fun, 		*real_to_realt_fun,
	*real_to_real2_fun,		*real_nat_to_real_fun,
	*real_to_string_fun,		*nat_to_real_fun,
	*real_nat_to_string_fun,	*realt_2nat_to_string_fun,
	*realt_3nat_to_string_fun, 	*real_to_intt_fun,
	*real_real2_to_real2_fun,	*real_to_ratt_fun,
       	*nat_to_int_fun,
	*int_to_intt_fun,		*int_to_nat_fun,
	*ratt_to_nat_fun,		*ratt_to_intt_fun,
	*real_to_bool_fun,		*nat_to_nat_fun,
	*nat_pair_to_intt_fun,		*nat_sfield_to_sfield_fun,
	*ratt_to_realt_fun,		*int_to_sfield_fun,
	*ratt_to_ratt_fun,		*rat_to_ratt_fun,
	*nat_to_ratt_fun,		*nat_to_realt_fun,
	*ratt_to_sfield_fun,		*ratt_to_wsfield_fun,
        *real_to_wnum_fun,		*nat_to_wnum_fun;


   ROLE *divide_role;

   sring_var    = var_t(RRING_ctc);
   sfield_var   = var_t(RFIELD_ctc);
   real_var     = var_t(REAL_ctc);
   real_var2    = var_t(REAL_ctc);
   real_var3    = var_t(REAL_ctc);
   ps_real_var  = var_t(REAL_ctc);
   ps_real_var->PREFER_SECONDARY = 1;
   rational_var = var_t(RATIONAL_ctc);
   int_var      = var_t(INTEGER_ctc);
   int_var2     = var_t(INTEGER_ctc);
   wnum_var     = wrap_var_t(REAL_ctc);
   wsfield_var  = wrap_var_t(RFIELD_ctc);

   sring_pair     = pair_t(sring_var, 		sring_var);
   sfield_pair    = pair_t(sfield_var, 		sfield_var);
   sfield_intt_pair= pair_t(sfield_var,		integer_type);
   nat_sfield_pair= pair_t(natural_type,	sfield_var);

   real_pair      = pair_t(real_var,     	real_var);
   real_real_pair = pair_t(real_var, 		real_var2);
   int_pair       = pair_t(int_var,             int_var);
   int_int_pair   = pair_t(int_var,		int_var2);
   real_nat_pair  = pair_t(real_var,		natural_type);
   int_nat_pair   = pair_t(int_var,		natural_type);
   nat_pair       = pair_t(natural_type, 	natural_type);
   realt_2nat_pair = pair_t(real_type,          nat_pair);
   realt_3nat_pair = pair_t(real_type,         pair_t(natural_type, nat_pair));

   bmp_type(real_pair_to_real_fun 	= function_t(real_pair, real_var));
   bmp_type(real_to_real_fun		= function_t(real_var, real_var));
   bmp_type(real_nat_to_real_fun	= function_t(real_nat_pair, real_var));

   bmp_type(real_to_real2_fun 	    = function_t(real_var, ps_real_var));
   bmp_type(real_real2_to_real2_fun = function_t(real_real_pair, real_var2));
   bmp_type(real_to_string_fun	    = function_t(real_var, string_type));
   bmp_type(real_nat_to_string_fun  = function_t(real_nat_pair, string_type));
   bmp_type(realt_2nat_to_string_fun =
	     function_t(realt_2nat_pair,string_type));
   bmp_type(realt_3nat_to_string_fun =
	     function_t(realt_3nat_pair,string_type));

   bmp_type(rat_to_ratt_fun   = function_t(rational_var, rational_type));
   bmp_type(real_to_realt_fun = function_t(real_var, real_type));
   bmp_type(real_to_intt_fun  = function_t(real_var, integer_type));
   bmp_type(real_to_ratt_fun  = function_t(real_var, rational_type));
   bmp_type(real_to_bool_fun  = function_t(real_var, boolean_type));

   bmp_type(sring_pair_to_sring_fun = function_t(sring_pair, sring_var));
   bmp_type(sring_to_sring_fun      = function_t(sring_var, sring_var));

   bmp_type(sfield_pair_to_sfield_fun 	= function_t(sfield_pair, sfield_var));
   bmp_type(sfield_to_sfield_fun 	= function_t(sfield_var, sfield_var));

   bmp_type(sfield_intt_to_sfield_fun =
	     function_t(sfield_intt_pair, sfield_var));

   bmp_type(ratt_to_intt_fun = function_t(rational_type, integer_type));
   bmp_type(ratt_to_nat_fun  = function_t(rational_type, natural_type));

   bmp_type(int_int_to_ratt_fun = function_t(int_int_pair, rational_type));
   bmp_type(int_to_ratt_fun	= function_t(int_var, rational_type));
   bmp_type(int_to_intt_fun	= function_t(int_var, integer_type));
   bmp_type(int_to_nat_fun	= function_t(int_var, natural_type));
   bmp_type(int_to_bool_fun 	= function_t(int_var, boolean_type));

   bmp_type(nat_to_nat_fun	= function_t(natural_type, natural_type));
   bmp_type(nat_to_intt_fun	= function_t(natural_type, integer_type));
   bmp_type(nat_pair_to_nat_fun = function_t(nat_pair, natural_type));
   bmp_type(nat_to_real_fun     = function_t(natural_type, real_var));
   bmp_type(nat_to_int_fun 	= function_t(natural_type, int_var));
   bmp_type(ratt_to_ratt_fun	= function_t(rational_type, rational_type));

   bmp_type(int_nat_to_int_nat_fun =
	     function_t(int_nat_pair, int_nat_pair));
   bmp_type(int_nat_to_int_fun   = function_t(int_nat_pair, int_var));
   bmp_type(int_nat_to_nat_fun   = function_t(int_nat_pair, natural_type));
   bmp_type(int_pair_to_int_fun  = function_t(int_pair, int_var));
   bmp_type(nat_pair_to_intt_fun = function_t(nat_pair, integer_type));
   bmp_type(hermit_to_realt_fun  = function_t(hermit_type, real_type));
   bmp_type(nat_sfield_to_sfield_fun =
	     function_t(nat_sfield_pair, sfield_var));
   bmp_type(int_to_sfield_fun  = function_t(int_var, sfield_var));
   bmp_type(ratt_to_realt_fun  = function_t(rational_type, real_type));

   bmp_type(nat_to_ratt_fun = function_t(natural_type, rational_type));
   bmp_type(nat_to_realt_fun = function_t(natural_type, real_type));

   bmp_type(ratt_to_sfield_fun =
	    function_t(rational_type, sfield_var));

   bmp_type(nat_to_wnum_fun  = function_t(natural_type, wnum_var));
   bmp_type(real_to_wnum_fun = function_t(real_var, wnum_var));
   bmp_type(ratt_to_wsfield_fun = function_t(rational_type, wsfield_var));

   bmp_type(compat_type = function_t(real_real_pair, 
				     pair_t(real_var3, real_var3)));

   bump_role(divide_role = fun_role(
		    pair_role(basic_role("dividend"),
			      basic_role("divisor")),
		    pair_role(basic_role("quotient"),
			      basic_role("remainder"))));
   SET_ROLE(divide_role, meld_roles(domainEx_fail_role, divide_role));

   sym_prim_const_tm(NORMALPRECISION_ID, natural_type, DBL_DIG);

   /*------------*
    * Operations *
    *------------*/

   sym_prim_tm(PLUS_SYM,       real_pair_to_real_fun, STD_OP, PLUS_STDF);
   sym_prim_tm(MINUS_SYM,      sring_pair_to_sring_fun, STD_OP, MINUS_STDF);
   sym_prim_tm(MINUS_SYM,      sring_to_sring_fun, STD_FUN, NEGATE_STDF);
   sym_prim_tm(DBL_MINUS_SYM,  real_pair_to_real_fun, STD_OP, ABSDIFF_STDF);
   sym_prim_tm(STAR_SYM,       real_pair_to_real_fun, STD_OP, TIMES_STDF);

   sym_mode_prim_tm(SLASH_SYM,     sfield_pair_to_sfield_fun,
				   domainEx_fail_role, STD_OP,
				   DIVIDE_STDF, NULL);
   sym_mode_prim_tm(SLASH_SYM,     sfield_to_sfield_fun,domainEx_fail_role,
				   STD_FUN, RECIPROCAL_STDF, NULL);
   sym_mode_prim_tm(DBL_SLASH_SYM, int_to_ratt_fun, domainEx_fail_role,
				   STD_FUN,MAKE_RAT1_STDF, NULL);
   sym_mode_prim_tm(DBL_SLASH_SYM, int_int_to_ratt_fun, domainEx_fail_role,
				   STD_OP, MAKE_RAT2_STDF, NULL);
   sym_mode_prim_tm(HAT_SYM, 	   real_nat_to_real_fun, domainEx_fail_role,
				   STD_OP, POW_STDF, NULL);
   sym_mode_prim_tm(HAT_SLASH_SYM, sfield_intt_to_sfield_fun,
				   domainEx_fail_role, STD_OP, POW_STDF, NULL);
   sym_prim_tm(ABS_ID,	  real_to_real_fun,     STD_FUN, 	ABS_STDF);
   sym_prim_tm(SIGN_ID,	  real_to_intt_fun,	STD_FUN,	SIGN_STDF);
   sym_prim_tm(ODDQ_ID,	  int_to_bool_fun,	STD_FUN,	ODD_STDF);
   sym_prim_tm(ZEROQ_ID,  real_to_bool_fun,     STD_FUN,        ZEROP_STDF);

   sym_mode_prim_tm(DIVIDE_ID,  int_nat_to_int_nat_fun, divide_role,
				PRIM_OP, INT_DIVIDE_I, NULL);
   sym_mode_prim_tm(DIV_ID, 	int_nat_to_int_fun,
				domainEx_fail_role, STD_OP, DIV_STDF, NULL);
   sym_mode_prim_tm(MOD_ID, 	int_nat_to_nat_fun, domainEx_fail_role,
				STD_OP, MOD_STDF, NULL);
   sym_mode_prim_tm(GCD_ID, 	int_pair_to_int_fun, domainEx_fail_role,
				STD_OP, GCD_STDF, NULL);

   /*-------------*
    * Conversions *
    *-------------*/

   sym_prim_tm(TILDE_SYM,      	real_to_realt_fun, STD_FUN, REAL_STDF);
   sym_prim_tm(EAGERNUMTOSTRING_ID, real_to_string_fun, 
	       			STD_FUN, TO_STRING1_STDF);
   sym_prim_tm(DBL_DOLLAR_SYM,  real_nat_to_string_fun,
				STD_OP, TO_STRING2_STDF);
   sym_prim_tm(DBL_DOLLAR_SYM, 	realt_2nat_to_string_fun,
				STD_OP,TO_STRING2_STDF);
   sym_prim_tm(DBL_DOLLAR_SYM, 	realt_3nat_to_string_fun,
				STD_OP,TO_STRING2_STDF);
   sym_mode_prim_tm(AS_ID,	real_real2_to_real2_fun,
				conversionEx_fail_role,
				TY_PRIM_OP, AS_I, NULL);
   sym_prim_tm(FLOOR_ID,     	real_to_intt_fun, STD_FUN, FLOOR_STDF);
   sym_prim_tm(CEILING_ID,   	real_to_intt_fun, STD_FUN, CEILING_STDF);

   sym_prim_tm(RATIONAL_ID,  	real_to_ratt_fun, STD_FUN, RATIONAL_STDF);
   sym_prim_tm(INTEGER_ID,	int_to_intt_fun, STD_FUN, INTEGER_STDF);
   sym_mode_prim_tm(NATURAL_ID, int_to_nat_fun,	conversionEx_fail_role,
				STD_FUN, NATURAL_STDF, NULL);

   sym_prim_tm(NABS_ID,	     	int_to_nat_fun, STD_FUN, ABS_STDF);
   sym_prim_tm(NUMERATOR_ID,  	ratt_to_intt_fun, PRIM_FUN, LEFT_I);
   sym_prim_tm(DENOMINATOR_ID,	ratt_to_nat_fun, PRIM_FUN, RIGHT_I);

   expect_sym_prim_tm(PLUS_SYM, real_to_real2_fun, 0, EXPECT_ATT);
   issue_missing_tm(std_id[PLUS_SYM], real_to_real2_fun, 0);
   sym_prim_tm(PLUS_SYM,	nat_to_nat_fun,    PRIM_CAST, 0);
   sym_prim_tm(PLUS_SYM,	int_to_intt_fun,   STD_FUN, INTEGER_STDF);
   sym_prim_tm(PLUS_SYM,	rat_to_ratt_fun,   STD_FUN, RATIONAL_STDF);
   sym_prim_tm(PLUS_SYM,	real_to_realt_fun, STD_FUN, REAL_STDF);
   sym_prim_tm(PLUS_SYM,	real_to_wnum_fun,  TY_FUN, WRAP_NUM_STDF);

   expect_sym_prim_tm(NATCONST_ID, nat_to_real_fun, 0, EXPECT_ATT);
   sym_prim_tm(NATCONST_ID, nat_to_int_fun,      PRIM_CAST, 0);
   sym_prim_tm(NATCONST_ID, nat_to_ratt_fun,     STD_FUN, RATIONAL_STDF);
   sym_prim_tm(NATCONST_ID, nat_to_realt_fun,    STD_FUN, REAL_STDF);
   sym_prim_tm(NATCONST_ID, nat_to_wnum_fun,	 TY_FUN,  WRAP_NUM_STDF);

   expect_sym_prim_tm(RATCONST_ID, ratt_to_sfield_fun, 0, EXPECT_ATT);
   sym_prim_tm(RATCONST_ID, ratt_to_ratt_fun,     PRIM_CAST, 0);
   sym_prim_tm(RATCONST_ID, ratt_to_realt_fun,    STD_FUN, REAL_STDF);
   sym_prim_tm(RATCONST_ID, ratt_to_wsfield_fun,  TY_FUN, WRAP_NUM_STDF);

   sym_prim_box_tm(PUTCOMMASINNUMBERS_ID, box_t(boolean_type), 
		   PUT_COMMAS_BOX_VAL);

   sym_mode_prim_tm(COMPATIFY_ID, compat_type, NULL, TY_FUN, COMPAT_STDF,
		    irregular_mode);

   drop_type(real_pair_to_real_fun);
   drop_type(real_to_real_fun);
   drop_type(real_nat_to_real_fun);
   drop_type(real_to_real2_fun);
   drop_type(real_real2_to_real2_fun);
   drop_type(real_to_string_fun);
   drop_type(real_nat_to_string_fun);
   drop_type(realt_2nat_to_string_fun);
   drop_type(realt_3nat_to_string_fun);
   drop_type(real_to_realt_fun);
   drop_type(real_to_intt_fun);
   drop_type(real_to_ratt_fun);
   drop_type(real_to_bool_fun);
   drop_type(sring_pair_to_sring_fun);
   drop_type(sring_to_sring_fun);
   drop_type(sfield_pair_to_sfield_fun);
   drop_type(sfield_to_sfield_fun);
   drop_type(sfield_intt_to_sfield_fun);
   drop_type(ratt_to_intt_fun);
   drop_type(ratt_to_nat_fun);
   drop_type(int_int_to_ratt_fun);
   drop_type(int_to_ratt_fun);
   drop_type(int_to_intt_fun);
   drop_type(int_to_nat_fun);
   drop_type(int_to_bool_fun);
   drop_type(nat_to_nat_fun);
   drop_type(nat_to_intt_fun);
   drop_type(nat_to_real_fun);
   drop_type(nat_to_int_fun);
   drop_type(ratt_to_ratt_fun);
   drop_type(nat_pair_to_nat_fun);
   drop_type(int_nat_to_int_nat_fun);
   drop_type(int_nat_to_int_fun);
   drop_type(int_nat_to_nat_fun);
   drop_type(int_pair_to_int_fun);
   drop_type(nat_pair_to_intt_fun);
   drop_type(hermit_to_realt_fun);
   drop_type(nat_sfield_to_sfield_fun);
   drop_role(divide_role);
   drop_type(int_to_sfield_fun);
   drop_type(ratt_to_realt_fun);
   drop_type(rat_to_ratt_fun);
   drop_type(nat_to_ratt_fun);
   drop_type(nat_to_realt_fun);
   drop_type(nat_to_wnum_fun);
   drop_type(real_to_wnum_fun);
   drop_type(ratt_to_wsfield_fun);

  } /* end arithmetic */

  /******************** Boxes ***************************/

  {TYPE *emptyq_type;
   TYPE *boxflavor_cnstr_type, *copyflavor_cnstr_type, *flavor_type;
   TYPE *emptyBox_type, *swap_type;
   TYPE *dollar_type, *bprank_type, *emptybang_type, *bool_to_herm_type;
   ROLE *assign_role, *emptyBox_role;

   bmp_type(emptyq_type     = function_t(s_any_box, boolean_type));
   bmp_type(emptyBox_type   = function_t(hermit_type, s_any_box));
   bmp_type(swap_type       = function_t(pair_t(s_any_box, s_any_box),
					  hermit_type));
   bmp_type(dollar_type     = function_t(s_any_box, string_type));
   bmp_type(bprank_type     = function_t(s_any_box, natural_type));
   bmp_type(emptybang_type  = function_t(s_any_box, hermit_type));
   bmp_type(bool_to_herm_type = function_t(boolean_type, hermit_type));
   bmp_type(boxflavor_cnstr_type = function_t(natural_type,boxflavor_type));
   bmp_type(copyflavor_cnstr_type = function_t(natural_type,copyflavor_type));
   bmp_type(flavor_type     = function_t(s_any_box, boxflavor_type));

   bump_role(assign_role     = fun_role(colon_equal_role, NULL));
   SET_ROLE(assign_role, meld_roles(assign_role, sideEffect_role));
   bump_role(emptyBox_role   = basic_role("pot__fail__emptyBoxEx"));

   sym_prim_tm(EMPTYBOXQ_ID,      emptyq_type, PRIM_FUN, EMPTY_BOX_I);
   sym_prim_tm(RANKBOX_ID,        bprank_type, STD_FUN, RANK_BOX_STDF);
   sym_prim_tm(FREEZEBOXRANKS_ID, bool_to_herm_type, STD_FUN,
				  SUPPRESS_COMPACTIFY_STDF);
   sym_prim_tm (EAGERBOXTOSTRING_ID, dollar_type, TY_FUN, BOX_TO_STR_STDF);

   sym_prim_const_tm(NONSHARED_ID,   boxflavor_type, 0);
   sym_prim_const_tm(SHARED_ID,      boxflavor_type, 1);
   sym_prim_const_tm(LARGEST_ID,     boxflavor_type, 1);
   sym_prim_tm(FLAVOR_ID,    flavor_type, STD_FUN, FLAVOR_STDF);
   sym_prim_tm(BOXFLAVOR_ID, boxflavor_cnstr_type, PRIM_ENUM_CAST, 2);

   sym_prim_const_tm(NONSHARED_ID,   copyflavor_type, 0);
   sym_prim_const_tm(SHARED_ID,      copyflavor_type, 1);
   sym_prim_const_tm(SAME_ID,	     copyflavor_type, 2);
   sym_prim_const_tm(LARGEST_ID,     copyflavor_type, 2);
   sym_prim_tm(COPYFLAVOR_ID, copyflavor_cnstr_type, PRIM_ENUM_CAST, 3);

   sym_prim_tm(EMPTYNONSHAREDBOX_ID,	 emptyBox_type, HERM_FUN, BOX_I);
   sym_prim_tm(EMPTYSHAREDBOX_ID, 	 emptyBox_type, HERM_FUN, PLACE_I);
   sym_prim_tm(EMPTYBOX_ID, 	         emptyBox_type, HERM_FUN, PLACE_I);

   sym_mode_prim_tm(AT_SYM, 	     content_type, emptyBox_role,
				     STD_FUN,  CONTENT_STDF, NULL);
   sym_mode_prim_tm(TESTAT_ID, 	     content_type,  testEx_fail_role,
				     STD_FUN, CONTENT_TEST_STDF, NULL);
   sym_mode_prim_tm(EMPTYBOXPROC_ID, emptybang_type, sideEffect_role,
				     PRIM_FUN, MAKE_EMPTY_I, NULL);
   sym_mode_prim_tm(EMPTY_BOX_BANG_ID, emptybang_type, sideEffect_role,
		    		     PRIM_FUN, MAKE_EMPTY_NODEMON_I, NULL);
   sym_mode_prim_tm(ASSIGN_ID,	     assign_type, assign_role,
				     PRIM_OP, ASSIGN_I,NULL);
   sym_mode_prim_tm(ASSIGNINIT_ID,   assign_type, assign_role,
				     PRIM_OP,ASSIGN_INIT_I, NULL);
   sym_mode_prim_tm(ASSIGNBANG_ID,   assign_type,assign_role,PRIM_OP,
				     ASSIGN_NODEMON_I,NULL);

   sym_prim_box_tm(PRECISION_ID,  box_t(natural_type), PRECISION_BOX_VAL);

   primitive_tm(".at",	      content_type,  STD_FUN,  PRCONTENT_STDF);

   if(compiling_standard_asi) {
     TYPE *onassign_type;
     no_gen_prim = TRUE;
     bmp_type(onassign_type = function_t(s_any_type, hermit_type));
     primitive_tm("ZZ__BasicOnAssign", onassign_type, STD_OP, ONASSIGN_STDF);
     no_gen_prim = FALSE;
     drop_type(onassign_type);
   }

   drop_type(emptyq_type);
   drop_type(emptyBox_type);
   drop_type(swap_type);
   drop_type(dollar_type);
   drop_type(bprank_type);
   drop_type(emptybang_type);
   drop_role(assign_role);
   drop_role(emptyBox_role);
   drop_type(boxflavor_cnstr_type);
   drop_type(copyflavor_cnstr_type);
   drop_type(flavor_type);
  } /* end boxes and places */


  /***************** Coproducts, Char, Enumerated *******************/

  {TYPE *ranked_var, *wranked_var, *enum_var, *ranked_pair, *enum_pair;
   TYPE *rank_type, *unrank_type, *upto_type, *succ_type;
   TYPE *eq_type, *compare_type, *largest_type, *unrank_missing_type;
   TYPE *nat_to_nat_type, *nat_to_char_type, *nat_to_hermit_type,
        *nat_to_bool_type, *nat_to_comparison_type, *nat_to_filemode_type,
        *nat_to_boxflavor_type, *nat_to_copyflavor_type;
   TYPE *dollar_type1, *dollar_type3, *dollar_type4,
	*dollar_type5, *dollar_type6, *dollar_type7, *dollar_type8;
   char *unrank_name;

   ranked_var   = var_t(RANKED_ctc);
   wranked_var  = wrap_var_t(RANKED_ctc);
   enum_var     = var_t(ENUMERATED_ctc);
   ranked_pair  = pair_t(ranked_var, ranked_var);
   enum_pair    = pair_t(enum_var, enum_var);

   bmp_type(largest_type = enum_var);
   bmp_type(rank_type    = function_t(ranked_var, natural_type));
   bmp_type(unrank_type  = function_t(natural_type, ranked_var));
   bmp_type(unrank_missing_type = function_t(natural_type, wranked_var));
   bmp_type(succ_type    = function_t(ranked_var, ranked_var));
   bmp_type(eq_type      = function_t(ranked_pair, boolean_type));
   bmp_type(upto_type    = function_t(enum_pair, list_t(enum_var)));
   bmp_type(compare_type = function_t(ranked_pair, comparison_type));
   bmp_type(nat_to_char_type = function_t(natural_type, char_type));
   bmp_type(nat_to_nat_type = function_t(natural_type, natural_type));
   bmp_type(nat_to_hermit_type = function_t(natural_type, hermit_type));
   bmp_type(nat_to_bool_type   = function_t(natural_type, boolean_type));
   bmp_type(nat_to_boxflavor_type = function_t(natural_type, boxflavor_type));
   bmp_type(nat_to_copyflavor_type = function_t(natural_type,copyflavor_type));
   bmp_type(nat_to_comparison_type =
	    function_t(natural_type, comparison_type));
   bmp_type(nat_to_filemode_type =
	    function_t(natural_type, fileMode_type));
   bmp_type(dollar_type1 = function_t(boolean_type, string_type));
   bmp_type(dollar_type3 = function_t(hermit_type, string_type));
   bmp_type(dollar_type4 = function_t(exception_type, string_type));
   bmp_type(dollar_type5 = function_t(comparison_type, string_type));
   bmp_type(dollar_type6 = function_t(fileMode_type, string_type));
   bmp_type(dollar_type7 = function_t(boxflavor_type, string_type));
   bmp_type(dollar_type8 = function_t(copyflavor_type, string_type));

   sym_prim_tm(RANK_ID,      rank_type,        PRIM_CAST, 0);

   unrank_name = std_id[UNRANK_ID];
   expect_sym_prim_tm(UNRANK_ID, unrank_type, 0, ANTICIPATE_ATT);
   issue_missing_tm(unrank_name, unrank_missing_type, strong_mode);
   primitive_tm(unrank_name, nat_to_nat_type,  PRIM_CAST, 0);   
   mode_primitive_tm(unrank_name, nat_to_hermit_type, conversionEx_fail_role,
				  PRIM_ENUM_CAST, 1, NULL);
   mode_primitive_tm(unrank_name, nat_to_bool_type, conversionEx_fail_role,
				  PRIM_ENUM_CAST, 2, NULL);
   mode_primitive_tm(unrank_name, nat_to_char_type, conversionEx_fail_role,
				  PRIM_LONG_ENUM_CAST, 256, NULL);
   mode_primitive_tm(unrank_name, nat_to_comparison_type, 
				  conversionEx_fail_role,
				  PRIM_ENUM_CAST, 3, NULL);
   mode_primitive_tm(unrank_name, nat_to_boxflavor_type, 
				  conversionEx_fail_role,
				  PRIM_ENUM_CAST, 2, NULL);
   mode_primitive_tm(unrank_name, nat_to_copyflavor_type, 
				  conversionEx_fail_role,
				  PRIM_ENUM_CAST, 3, NULL);
   mode_primitive_tm(unrank_name, nat_to_filemode_type, 
				  conversionEx_fail_role,
				  PRIM_ENUM_CAST, 3, NULL);

   sym_prim_tm(UPTO_ID,   upto_type,        STD_OP,    UPTO_STDF);
   sym_prim_tm(DOWNTO_ID, upto_type,        STD_OP,    DOWNTO_STDF);

   sym_mode_prim_tm(PRED_ID,   	succ_type,  domainEx_fail_role,
				STD_FUN,   PRED_STDF, NULL);
   sym_mode_prim_tm(CHAR_ID,   	nat_to_char_type, conversionEx_fail_role,
				PRIM_LONG_ENUM_CAST, 256, NULL);

   expect_sym_prim_tm(LARGEST_ID,  largest_type, 0, ANTICIPATE_ATT);
   sym_prim_const_tm(SMALLEST_ID,  largest_type, 0);
   sym_prim_const_tm(LARGEST_ID,   char_type,    255);
   sym_prim_const_tm(LARGEST_ID,   hermit_type,  0);
   
   /*-------------------------------------------------------------------*
    * Force def_global_tm to install the following as primitives, even  *
    * though they are underrides, and hence defaults.			*
    *-------------------------------------------------------------------*/

   force_prim = TRUE;
   sym_mode_prim_tm(COMPARE_ID, compare_type, NULL, STD_OP,
				COMPARE_STDF, underrides_mode);
   sym_mode_prim_tm(EQ_SYM, 	eq_type, NULL, PRIM_OP,
				EQ_I, underrides_mode);
   sym_mode_prim_tm(NE_SYM, 	eq_type, NULL, STD_OP,
				NE_STDF, underrides_mode);
   sym_mode_prim_tm(LT_SYM, 	eq_type, NULL, STD_OP,
				LT_STDF, underrides_mode);
   sym_mode_prim_tm(LE_SYM, 	eq_type, NULL, STD_OP,
				LE_STDF, underrides_mode);
   sym_mode_prim_tm(GT_SYM, 	eq_type, NULL, STD_OP,
				GT_STDF, underrides_mode);
   sym_mode_prim_tm(GE_SYM, 	eq_type, NULL, STD_OP,
				GE_STDF, underrides_mode);
   force_prim = FALSE;

   sym_prim_tm(DOLLAR_SYM, dollar_type3, 
	       STD_FUN, HERMIT_TO_STRING_STDF);
   sym_prim_tm(EAGERBOOLEANTOSTRING_ID, dollar_type1, 
	       STD_FUN, BOOL_TO_STRING_STDF);
   sym_prim_tm(EAGEREXCEPTIONTOSTRING_ID, dollar_type4, 
	       STD_FUN, EXCEPTION_TO_STRING_STDF);
   sym_prim_tm(EXCEPTIONDESCRIPTION_ID, dollar_type4,
	       STD_FUN, EXCEPTION_STRING_STDF);
   sym_prim_tm(EAGERCOMPARISONTOSTRING_ID, dollar_type5, 
	       STD_FUN, COMPARISON_TO_STRING_STDF);
   sym_prim_tm(EAGERFILEMODETOSTRING_ID, dollar_type6, 
	       STD_FUN, FILEMODE_TO_STRING_STDF);
   sym_prim_tm(EAGERBOXFLAVORTOSTRING_ID, dollar_type7, 
	       STD_FUN, BOXFLAVOR_TO_STRING_STDF);
   sym_prim_tm(EAGERCOPYFLAVORTOSTRING_ID, dollar_type8, 
	       STD_FUN, COPYFLAVOR_TO_STRING_STDF);

   drop_type(rank_type);
   drop_type(unrank_type);
   drop_type(unrank_missing_type);
   drop_type(succ_type);
   drop_type(eq_type);
   drop_type(upto_type);
   drop_type(compare_type);
   drop_type(nat_to_char_type);
   drop_type(nat_to_nat_type);
   drop_type(nat_to_hermit_type);
   drop_type(nat_to_bool_type);
   drop_type(nat_to_comparison_type);
   drop_type(nat_to_filemode_type);
   drop_type(nat_to_boxflavor_type);
   drop_type(nat_to_copyflavor_type);
   drop_type(dollar_type1);
   drop_type(dollar_type3);
   drop_type(dollar_type4);
   drop_type(dollar_type5);
   drop_type(dollar_type6);
   drop_type(dollar_type7);
   drop_type(dollar_type8);
  } /* end coproducts */

  /************************* Boolean *********************************/

  {TYPE *not_type, *bool_cnstr_type;

   bmp_type(not_type = function_t(boolean_type, boolean_type));
   bmp_type(bool_cnstr_type = function_t(natural_type, boolean_type));

   sym_prim_tm(TRUE_ID,    boolean_type,     PRIM_CONST, TRUE_I);
   sym_prim_tm(FALSE_ID,   boolean_type,     PRIM_CONST, NIL_I);
   sym_prim_tm(NOT_ID,     not_type,         PRIM_FUN,   NOT_I);
   sym_prim_tm(BOOLEAN_ID, bool_cnstr_type,  PRIM_ENUM_CAST, 2);
   sym_prim_tm(LARGEST_ID, boolean_type,     PRIM_CONST, TRUE_I);

   drop_type(not_type);
   drop_type(bool_cnstr_type);
  }

  /********************** Input and Output *************************/

  {TYPE *read_type, *rawshow_type;
   TYPE *string2nat_type, *string2int_type, *string2rat_type;
   TYPE *string2real_type, *string_mode_pair;
   TYPE *fprint_type, *fclose_type, *fopen_type, *string_box_type;
   TYPE *strlist_type, *outfile_dollar_type, *outfile_box_type;
   TYPE *nat2string_type;

   bmp_type(string2nat_type = function_t(string_type, natural_type));
   bmp_type(string2int_type = function_t(string_type, integer_type));
   bmp_type(string2rat_type = function_t(string_type, rational_type));
   bmp_type(string2real_type = function_t(string_type, real_type));
   bmp_type(nat2string_type = function_t(natural_type, string_type));

   bmp_type(string_mode_pair = pair_t(string_type, list_t(fileMode_type)));

   bmp_type(strlist_type  = list_t(string_type));
   bmp_type(fprint_type   =
	     function_t(pair_t(outfile_type, strlist_type), hermit_type));
   bmp_type(fclose_type   = function_t(outfile_type, hermit_type));
   bmp_type(fopen_type    = function_t(string_mode_pair, outfile_type));
   bmp_type(read_type     = function_t(string_mode_pair, string_type));
   bmp_type(rawshow_type =
	     function_t(pair_t(outfile_type, s_any_type), hermit_type));
   bmp_type(outfile_dollar_type =
	     function_t(fam_mem_t(outfile_fam, s_any_type), string_type));
   bmp_type(outfile_box_type = box_t(outfile_type));
   bmp_type(string_box_type = box_t(string_type));

   sym_prim_tm(EAGEROUTFILETOSTRING_ID, outfile_dollar_type, 
	       STD_FUN, OUTFILE_DOLLAR_STDF);

   sym_mode_prim_tm(OUTFILE_ID, fopen_type, noFileEx_fail_role,
				STD_OP, FOPEN_STDF, NULL);
   sym_mode_prim_tm(FILE_ID,   	read_type,  noFileEx_fail_role,
				STD_OP,  INPUT_STDF, NULL);

   sym_prim_const_tm(VOLATILEMODE_ID,  fileMode_type, VOLATILE_INDEX_FM);
   sym_prim_const_tm(APPENDMODE_ID,    fileMode_type, APPEND_INDEX_FM);
   sym_prim_const_tm(BINARYMODE_ID,    fileMode_type, BINARY_INDEX_FM);
   sym_prim_const_tm(LARGEST_ID, fileMode_type, LARGEST_INDEX_FM);

   sym_prim_box_tm(STDIN_ID,     string_box_type,  STDIN_BOX_VAL);
   sym_prim_box_tm(TRUESTDIN_ID, string_box_type,  TRUE_STDIN_BOX_VAL);
   sym_prim_box_tm(STDOUT_ID,    outfile_box_type, STDOUT_BOX_VAL);
   sym_prim_box_tm(STDERR_ID, 	 outfile_box_type, STDERR_BOX_VAL);

   sym_prim_tm(RAWSHOW_ID,           rawshow_type, PRIM_OP, RAW_SHOW_I);
   sym_prim_tm(SHOWENVIRONMENT_ID,   fclose_type, STD_FUN, SHOW_ENV_STDF);
   sym_prim_tm(SHOWCONFIGURATION_ID, fclose_type, STD_FUN, SHOW_CONFIG_STDF);
   sym_prim_tm(FLUSHFILE_ID,	     fclose_type, STD_FUN, FLUSH_FILE_STDF);

   sym_prim_tm(HEXTONATURAL_ID,     string2nat_type,
	       			    STD_FUN, HEX_TO_NAT_STDF);
   sym_prim_tm(NATURALTOHEX_ID,	    nat2string_type,
				    STD_FUN, NAT_TO_HEX_STDF);
   sym_prim_tm(STRINGTONATURAL_ID,  string2nat_type,
				    STD_FUN, STRING_TO_NAT_STDF);
   sym_prim_tm(STRINGTOINTEGER_ID,  string2int_type,
				    STD_FUN, STRING_TO_INT_STDF);
   sym_prim_tm(STRINGTORATIONAL_ID, string2rat_type,
				    STD_FUN, STRING_TO_RAT_STDF);
   sym_prim_tm(STRINGTOREAL_ID,     string2real_type,
				    STD_FUN, STRING_TO_REAL_STDF);

   if(compiling_standard_asi) {
     TYPE* begin_view_type;
     no_gen_prim = TRUE;
     bmp_type(begin_view_type = function_t(hermit_type, hermit_type));
     sym_prim_tm(BEGINVIEW_ID, begin_view_type, PRIM_FUN, BEGIN_VIEW_I);
     sym_prim_tm(ENDVIEW_ID,   begin_view_type, PRIM_FUN, END_VIEW_I);
     no_gen_prim = FALSE;
     primitive_tm("ZZ__ExternalFWrite", fprint_type, PRIM_OP, FPRINT_I);
     primitive_tm("ZZ__ExternalFClose", fclose_type, STD_FUN, FCLOSE_STDF);
     drop_type(begin_view_type);
   }

   drop_type(string2nat_type);
   drop_type(string2int_type);
   drop_type(string2rat_type);
   drop_type(string2real_type);
   drop_type(nat2string_type);
   drop_type(strlist_type);
   drop_type(fprint_type);
   drop_type(fclose_type);
   drop_type(fopen_type);
   drop_type(read_type);
   drop_type(rawshow_type);
   drop_type(outfile_dollar_type);
   drop_type(string_mode_pair);
   drop_type(string_box_type);
   drop_type(outfile_box_type);
  } /* end input and output */

  /********************* Comparisons *********************/

  {TYPE *order_var, *real_var, *wrap_real_var, *wrap_real_var2;
   TYPE *order_pair, *real_pair;
   TYPE *eq_pair, *box_pair, *l_any_box, *starred_any_EQ_var;
   TYPE *eq_type, *compare_type, *real_cmp;
   TYPE *lt_type1, *lt_type, *eq_type1, *wneq_type;
   TYPE *comparison_cnstr_type;
   char *compare_name, *lt_name, *le_name, *gt_name, *ge_name, *eq_name,
	*ne_name;

   order_var      = new_type(MARK_T, var_t(ORDER_ctc));
   starred_any_EQ_var = new_type(MARK_T, var_t(EQ_ctc));
   real_var	  = var_t(REAL_ctc);
   wrap_real_var  = wrap_var_t(REAL_ctc);
   wrap_real_var2 = wrap_var_t(REAL_ctc);
   l_any_box      = fam_mem_t(box_fam, s_any_type);

   eq_pair	  = pair_t(starred_any_EQ_var,	starred_any_EQ_var);
   real_pair	  = pair_t(real_var,	        real_var);
   order_pair 	  = pair_t(order_var,           order_var);
   box_pair       = pair_t(l_any_box, 		l_any_box);

   bmp_type(real_cmp       = function_t(real_pair,     comparison_type));
   bmp_type(compare_type   = function_t(order_pair,    comparison_type));
   bmp_type(eq_type	   = function_t(eq_pair,       boolean_type));
   bmp_type(eq_type1       = function_t(box_pair,      boolean_type));
   bmp_type(lt_type        = function_t(order_pair,    boolean_type));
   bmp_type(lt_type1       = function_t(real_pair,     boolean_type));
   bmp_type(comparison_cnstr_type = function_t(natural_type,comparison_type));
   bmp_type(wneq_type      = function_t(pair_t(wrap_real_var, wrap_real_var2),
					boolean_type));

   compare_name = std_id[COMPARE_ID];
   lt_name      = std_id[LT_SYM];
   le_name      = std_id[LE_SYM];
   gt_name      = std_id[GT_SYM];
   ge_name      = std_id[GE_SYM];
   eq_name      = std_id[EQ_SYM];
   ne_name      = std_id[NE_SYM];

   expect_sym_prim_tm(COMPARE_ID, compare_type, 0, ANTICIPATE_ATT);
   expect_sym_prim_tm(LT_SYM,      lt_type,      0, EXPECT_ATT);
   expect_sym_prim_tm(LE_SYM,      lt_type,      0, EXPECT_ATT);
   expect_sym_prim_tm(GT_SYM,      lt_type,      0, EXPECT_ATT);
   expect_sym_prim_tm(GE_SYM,      lt_type,      0, EXPECT_ATT);
   expect_sym_prim_tm(EQ_SYM,      eq_type,      0, ANTICIPATE_ATT);
   expect_sym_prim_tm(NE_SYM,      eq_type,      0, EXPECT_ATT);

   sym_prim_const_tm(EQUAL_ID,   comparison_type, 0);
   sym_prim_const_tm(GREATER_ID, comparison_type, 1);
   sym_prim_const_tm(LESS_ID,    comparison_type, 2);
   sym_prim_const_tm(LARGEST_ID, comparison_type, 2);
   sym_prim_tm(COMPARISON_ID, comparison_cnstr_type, PRIM_ENUM_CAST, 3);

   primitive_tm(compare_name,    real_cmp,   STD_OP,  COMPARE_STDF);
   primitive_tm(lt_name,         lt_type1,   STD_OP,  LT_STDF);
   primitive_tm(le_name,         lt_type1,   STD_OP,  LE_STDF);
   primitive_tm(gt_name,         lt_type1,   STD_OP,  GT_STDF);
   primitive_tm(ge_name,         lt_type1,   STD_OP,  GE_STDF);
   primitive_tm(ne_name,         lt_type1,   STD_OP,  NE_STDF);
   primitive_tm(eq_name,         lt_type1,   PRIM_OP, EQ_I);
   sym_prim_tm(EQ_ID,            eq_type1,   PRIM_OP, EQ_I);
   sym_prim_tm(NEQ_ID,           eq_type1,   STD_OP,  NE_STDF);
   sym_prim_tm(WRAPPEDNUMEQUAL_ID, wneq_type, PRIM_OP, EQ_I);

   drop_type(real_cmp);
   drop_type(compare_type);
   drop_type(eq_type);
   drop_type(eq_type1);
   drop_type(lt_type);
   drop_type(lt_type1);
   drop_type(comparison_cnstr_type);
   drop_type(wneq_type);
  } /* end comparisons */

  /******************* Products ******************************/

  {TYPE *left_type, *right_type;

   bmp_type(left_type  = function_t(s_any_pair, s_any_type));
   bmp_type(right_type = function_t(s_any_pair, s_any_type2));

   sym_prim_tm(LAZYLEFT_ID,  left_type,  LIST_FUN, LAZY_LEFT_STDF);
   sym_prim_tm(LAZYRIGHT_ID, right_type, LIST_FUN, LAZY_RIGHT_STDF);
   sym_prim_tm(LEFT_ID,      left_type,  PRIM_FUN, LEFT_I);
   sym_prim_tm(RIGHT_ID,     right_type, PRIM_FUN, RIGHT_I);

   attach_property(select_left_role, std_id[LEFT_ID], left_type);
   attach_property(select_left_role, std_id[LAZYLEFT_ID], left_type);
   attach_property(select_right_role, std_id[RIGHT_ID], right_type);
   attach_property(select_right_role, std_id[LAZYRIGHT_ID], right_type);

   drop_type(left_type);
   drop_type(right_type);
  } /* end products */


  /******************* Lists ******************************/

  {TYPE *head_type, *tail_type, *sharp_type, *length_type;
   TYPE *append_type, *nilq_type, *split_type, *pair, *array_type;
   TYPE *box_list, *dbl_sharp_type;
   TYPE *int_var1, *int_var2, *upto_type, *downto_type;
   TYPE *string_to_string_type;

   int_var1    = var_t(INTEGER_ctc);
   int_var2    = var_t(INTEGER_ctc);
   pair        = pair_t(s_any_type, s_any_list);
   box_list    = list_t(s_any_box);

   bmp_type(split_type  = function_t(s_any_list, pair));
   bmp_type(head_type   = function_t(s_any_list, s_any_type));
   bmp_type(tail_type   = function_t(s_any_list, s_any_list));
   bmp_type(sharp_type  =
	     function_t(pair_t(s_any_list, natural_type), s_any_type));
   bmp_type(length_type = function_t(s_any_list, natural_type));
   bmp_type(append_type = function_t(pair_t(s_any_list, s_any_list),
				      s_any_list));
   bmp_type(string_to_string_type = function_t(string_type, string_type));
   bmp_type(nilq_type      = function_t(s_any_list, boolean_type));
   bmp_type(array_type     = function_t(natural_type, box_list));
   bmp_type(upto_type      =
	     function_t(pair_t(int_var1, int_var2), list_t(int_var1)));
   bmp_type(downto_type    =
	     function_t(pair_t(int_var1, int_var2), list_t(int_var2)));
   bmp_type(dbl_sharp_type = function_t(pair_t(s_any_list,
				      pair_t(natural_type, natural_type)),
			       s_any_list));

   sym_prim_tm(CONS_SYM, 	cons_type,      PRIM_CAST, 0);
   sym_prim_tm(DBL_SHARP_SYM,	dbl_sharp_type, PRIM_OP, SUBLIST_I);
   sym_prim_tm(DBL_PLUS_SYM,    append_type,    LIST_OP, APPEND_STDF);
   sym_prim_tm(NIL_ID,          s_any_list,     PRIM_CONST, NIL_I);
   sym_prim_tm(LENGTH_ID,       length_type,    LIST_FUN, LENGTH_STDF);
   sym_prim_tm(REVERSE_ID,	tail_type,	LIST_FUN, REVERSE_STDF);
   sym_prim_tm(NILQ_ID,         nilq_type,      PRIM_FUN, NILQ_I);
   sym_prim_tm(AGGRESSIVENILQ_ID, nilq_type,	PRIM_FUN,  NIL_FORCE_I);
   sym_prim_tm(PACK_ID,          tail_type,     LIST_FUN,  PACK_STDF);
   sym_prim_tm(UPTO_ID,          upto_type,     STD_OP,    UPTO_STDF);
   sym_prim_tm(DOWNTO_ID,        downto_type,   STD_OP,    DOWNTO_STDF);
   sym_prim_tm(INTERNSTRING_ID,  string_to_string_type,
	       LIST_FUN, INTERN_STRING_STDF);

   sym_mode_prim_tm(SHARP_SYM,  sharp_type, domainEx_fail_role,
				PRIM_OP,   SUBSCRIPT_I, NULL);
   sym_mode_prim_tm(HEAD_ID,    head_type, emptyListEx_fail_role,
				PRIM_FUN,   HEAD_I, NULL);
   sym_mode_prim_tm(TAIL_ID,    tail_type, emptyListEx_fail_role,
				PRIM_FUN,   TAIL_I, NULL);
   sym_mode_prim_tm(AGGRESSIVEHEAD_ID, head_type, emptyListEx_fail_role,
				PRIM_FUN,   LEFT_I, NULL);
   sym_mode_prim_tm(AGGRESSIVETAIL_ID, tail_type, emptyListEx_fail_role,
				PRIM_FUN,   RIGHT_I, NULL);
   sym_mode_prim_tm(LAZYHEAD_ID, head_type, emptyListEx_fail_role,
				LIST_FUN,   LAZY_HEAD_STDF, NULL);
   sym_mode_prim_tm(LAZYTAIL_ID, tail_type, emptyListEx_fail_role,
				LIST_FUN,   LAZY_TAIL_STDF, NULL);
   sym_mode_prim_tm(AGGRESSIVELAZYHEAD_ID, head_type, emptyListEx_fail_role,
				LIST_FUN,  LAZY_LEFT_STDF, NULL);
   sym_mode_prim_tm(AGGRESSIVELAZYTAIL_ID, tail_type, emptyListEx_fail_role,
				LIST_FUN,  LAZY_RIGHT_STDF, NULL);
   sym_mode_prim_tm(LISTTOPAIR_ID, split_type, emptyListEx_fail_role,
				PRIM_FUN,   NONNIL_TEST_I, NULL);
   sym_mode_prim_tm(AGGRESSIVELISTTOPAIR_ID,split_type,emptyListEx_fail_role,
				PRIM_FUN, NONNIL_FORCE_I, NULL);

   sym_prim_tm(SHAREDARRAY_ID,     array_type, STD_FUN, MAKE_PARRAY_STDF);
   sym_prim_tm(NONSHAREDARRAY_ID,  array_type, STD_FUN, MAKE_ARRAY_STDF);

   drop_type(split_type);
   drop_type(head_type);
   drop_type(tail_type);
   drop_type(sharp_type);
   drop_type(length_type);
   drop_type(append_type);
   drop_type(nilq_type);
   drop_type(array_type);
   drop_type(upto_type);
   drop_type(downto_type);
   drop_type(dbl_sharp_type);
   drop_type(string_to_string_type);

   if(compiling_standard_asi) {
     TYPE *scanfor_type, *sfarg_type, *sfresult_type, *bv_setbits_type;
     sfarg_type     = pair_t(string_type, 
			  pair_t(char_type,
			         pair_t(integer_type, boolean_type)));
     sfresult_type  = pair_t(natural_type, pair_t(string_type, char_type));
     bmp_type(scanfor_type = function_t(sfarg_type, sfresult_type));
     bmp_type(bv_setbits_type = 
	       function_t(pair_t(integer_type, 
			         pair_t(list_t(char_type), 
				        boolean_type)), 
			  integer_type));

     sym_prim_tm(SCANFORCHARPRIM_ID, scanfor_type, LIST_OP, SCAN_FOR_STDF);
     sym_prim_tm(MAKECHARSETPRIM_ID, bv_setbits_type, STD_OP,BV_SETBITS_STDF);

     drop_type(scanfor_type);
     drop_type(bv_setbits_type);
   }

  } /* end lists */

  bump_expr(nilstr_expr = 
	    typed_global_sym_expr(NIL_ID, string_type, 0));
  SET_EXPR(nilstr_expr, skip_sames(nilstr_expr));


  /******************** Unknowns *******************************/

  {TYPE *herm_any_fun, *any_bool_fun, *any_bool_op, *l_any_pair;
   TYPE *herm_any_wk_fun, *l_any_any2_pair, *any2_bool_op;
   TYPE *any_herm_op, *bind_prot_type, *unknownKey_to_string_type;
   ROLE *bind_role;

   l_any_pair = pair_t(s_any_type, s_any_type);
   l_any_any2_pair = pair_t(s_any_type, s_any_type2);
   bmp_type(herm_any_fun = function_t(hermit_type, s_any_type));
   bmp_type(herm_any_wk_fun = function_t(hermit_type,
					 pair_t(s_any_type, unknownKey_type)));
   bmp_type(any_bool_fun = function_t(s_any_type, boolean_type));
   bmp_type(any_bool_op = function_t(l_any_pair, boolean_type));
   bmp_type(any2_bool_op = function_t(l_any_any2_pair, boolean_type));
   bmp_type(any_herm_op = function_t(l_any_pair, hermit_type));
   bmp_type(bind_prot_type =
     function_t(pair_t(pair_t(s_any_type, unknownKey_type),
		       s_any_type),
		hermit_type));
   bmp_type(unknownKey_to_string_type =
     function_t(unknownKey_type, string_type));
   bump_role(bind_role = basic_role("pot__fail__bindUnknownEx"));
   SET_ROLE(bind_role, meld_roles(bind_role, sideEffect_role));

   sym_prim_tm(NONSHAREDUNKNOWN_ID, herm_any_fun,
				  STD_FUN, PRIVATE_UNKNOWN_STDF);
   sym_prim_tm(UNKNOWN_ID,        herm_any_fun,
				  STD_FUN, PRIVATE_UNKNOWN_STDF);
   sym_prim_tm(SHAREDUNKNOWN_ID,  herm_any_fun,
				  STD_FUN, PUBLIC_UNKNOWN_STDF);
   sym_prim_tm(PROTECTEDNONSHAREDUNKNOWN_ID, herm_any_wk_fun,
				  STD_FUN, PROT_PRIV_UNKNOWN_STDF);
   sym_prim_tm(PROTECTEDSHAREDUNKNOWN_ID, herm_any_wk_fun,
				  STD_FUN, PROT_PUB_UNKNOWN_STDF);
   sym_prim_tm(UNKNOWNQ_ID,       any_bool_fun,
				  UNK_FUN, UNKNOWNQ_STDF);
   sym_prim_tm(PROTECTEDUNKNOWNQ_ID, any_bool_fun,
				  UNK_FUN, PROTECTED_UNKNOWNQ_STDF);
   sym_prim_tm(UNPROTECTEDUNKNOWNQ_ID, any_bool_fun,
				  UNK_FUN, UNPROTECTED_UNKNOWNQ_STDF);
   sym_prim_tm(SAMEUNKNOWNQ_ID,   any_bool_op,
				  UNK_FUN, SAME_UNKNOWN_STDF);
   sym_prim_tm(SAMEUNKNOWNQQ_ID,  any2_bool_op,
				  UNK_FUN, SAME_UNKNOWN_STDF);
   sym_mode_prim_tm(BINDUNKNOWN_ID, any_herm_op, bind_role,
				  PRIM_BIND_UNK, 0, NULL);
   sym_mode_prim_tm(BINDPROTECTEDUNKNOWN_ID, bind_prot_type, bind_role,
				  PRIM_BIND_UNK, 1, NULL);

   drop_type(herm_any_fun);
   drop_type(herm_any_wk_fun);
   drop_type(any_bool_fun);
   drop_type(any_bool_op);
   drop_type(any_herm_op);
   drop_type(bind_prot_type);
   drop_type(unknownKey_to_string_type);
   drop_role(bind_role);
  } /* end unknowns */


  /***************** Exceptions and failure. *******************/

  /*--------------------------------------------------------------------*
   * When a new exception is added,					*
   *									*
   *  (1) Add its name to machdata/except.c:init_exception_names	*
   *									*
   *  (2) If the exception is to be trapped, add a line for it to	*
   *	  machdata/except.c:init_exceptions				*
   *									*
   *  (3) See tables/primtbl.c:sym_mode_prim_tm.  Might want to update	*
   *	  which exceptions the compiler thinks are trapped.		*
   *--------------------------------------------------------------------*/

  {TYPE *fail_type, *failb_type;

   bmp_type(fail_type = function_t(exception_type, s_any_type));
   bmp_type(failb_type = function_t(exception_type, hermit_type));

   sym_prim_tm(FAIL_ID, 	fail_type, PRIM_FUN, FAIL_I);
   sym_prim_tm(FAILPROC_ID,	failb_type, PRIM_FUN, FAIL_I);
   sym_prim_tm(WILLTRAP_ID,     exc_to_bool_type, STD_FUN, WILLTRAP_STDF);

   prim_exception_const_tm(TESTX_ID, 		TEST_EX);
   sym_exc_trap_tm(TESTX_ID, 0);
   prim_exception_const_tm(MEMX_ID,  		MEM_EX);
   sym_exc_trap_tm(MEMX_ID, 1);
   prim_exception_const_tm(ENDTHREADX_ID, 	ENDTHREAD_EX);
   sym_exc_trap_tm(ENDTHREADX_ID, 0);
   prim_exception_const_tm(EMPTYBOXX_ID, 	EMPTY_BOX_EX);
   sym_exc_trap_tm(EMPTYBOXX_ID, 1);
   prim_exception_const_tm(EMPTYLISTX_ID,	EMPTY_LIST_EX);
   sym_exc_trap_tm(EMPTYLISTX_ID, 1);
   prim_exception_const_tm(SUBSCRIPTX_ID,	SUBSCRIPT_EX);
   sym_exc_trap_tm(SUBSCRIPTX_ID, 1);
   prim_exception_const_tm(TERMINATEX_ID,	TERMINATE_EX);
   sym_exc_trap_tm(TERMINATEX_ID, 0);
   prim_exception_const_tm(CONVERSIONX_ID, 	CONVERSION_EX);
   sym_exc_trap_tm(CONVERSIONX_ID, 0);
   prim_exception_const_tm(SIZEX_ID, 		SIZE_EX);
   sym_exc_trap_tm(SIZEX_ID, 1);
   prim_exception_const_tm(NOCASEX_ID, 		NO_CASE_EX);
   sym_exc_trap_tm(NOCASEX_ID, 1);
   prim_exception_const_tm(SPECIESX_ID, 	SPECIES_EX);
   sym_exc_trap_tm(SPECIESX_ID, 0);
   prim_exception_const_tm(INFINITELOOPX_ID, 	INF_LOOP_EX);
   sym_exc_trap_tm(INFINITELOOPX_ID, 1);
   prim_exception_const_tm(INTERRUPTX_ID, 	INTERRUPT_EX);
   sym_exc_trap_tm(INTERRUPTX_ID, 1);
   prim_exception_const_tm(CANNOTSEEKX_ID, 	CANNOT_SEEK_EX);
   sym_exc_trap_tm(CANNOTSEEKX_ID, 1);
   prim_exception_const_tm(SYSTEMX_ID, 		DO_COMMAND_EX);
   sym_exc_trap_tm(SYSTEMX_ID, 1);
   prim_exception_const_tm(TOOMANYFILESX_ID, 	TOO_MANY_FILES_EX);
   sym_exc_trap_tm(TOOMANYFILESX_ID, 1);
   prim_exception_const_tm(BADINPUTX_ID, 	INPUT_EX);
   sym_exc_trap_tm(BADINPUTX_ID, 0);
   prim_exception_const_tm(LIMITX_ID, 		LIMIT_EX);
   sym_exc_trap_tm(LIMITX_ID, 1);
   prim_exception_const_tm(BINDUNKNOWNX_ID,	BIND_EX);
   sym_exc_trap_tm(BINDUNKNOWNX_ID, 0);
   prim_exception_const_tm(CLOSEDFILEX_ID,	CLOSED_FILE_EX);
   sym_exc_trap_tm(CLOSEDFILEX_ID, 0);
   prim_exception_const_tm(LISTEXHAUSTEDX_ID, 	LISTEXHAUSTED_EX);
   sym_exc_trap_tm(LISTEXHAUSTEDX_ID, 0);

   prim_exception_fun_tm(DOMAINX_ID, 		DOMAIN_EX);
   sym_exc_trap_tm(DOMAINX_ID, 1);
   prim_exception_fun_tm(NODEFX_ID, 	 	GLOBAL_ID_EX);
   sym_exc_trap_tm(NODEFX_ID, 1);
   prim_exception_fun_tm(NOFILEX_ID, 	 	NO_FILE_EX);
   sym_exc_trap_tm(NOFILEX_ID, 0);
   prim_exception_fun_tm(NOFONTX_ID,		NO_FONT_EX);
   sym_exc_trap_tm(NOFONTX_ID, 1);
   prim_exception_fun_tm(ENSUREX_ID,		ENSURE_EX);
   sym_exc_trap_tm(ENSUREX_ID, 1);
   prim_exception_fun_tm(REQUIREX_ID,		REQUIRE_EX);
   sym_exc_trap_tm(REQUIREX_ID, 1);

   drop_type(fail_type);
   drop_type(failb_type);
  } /* end exceptions/failure */


  /************************ Wrapped Species ********************/

  {TYPE *l_wrapped_var, *l_wrapped_var4;
   TYPE *l_wrapped_var5, *l_primary_var, *l_primary_var2, *l_any_var;
   TYPE *wrap_type, *unwrap_type, *tryunwrap_type;
   HEAD_TYPE hd;

   l_any_var      = var_t(NULL);
   l_primary_var  = primary_var_t(NULL);
   l_primary_var2 = primary_var_t(NULL);
   l_wrapped_var  = wrap_var_t(NULL);
   l_wrapped_var4 = wrap_var_t(NULL);
   l_wrapped_var5 = wrap_var_t(NULL);
   bump_list(l_wrapped_var->LOWER_BOUNDS  = type_cons(l_any_var, NIL)); 
   bump_list(l_wrapped_var4->LOWER_BOUNDS = type_cons(l_primary_var, NIL)); 
   bump_list(l_wrapped_var5->LOWER_BOUNDS = type_cons(l_primary_var2, NIL)); 
   hd.type = l_wrapped_var5;
   bump_list(l_primary_var2->LOWER_BOUNDS = general_cons(hd, NIL, TYPE1_L));

   bmp_type(wrap_type      = function_t(l_any_var, l_wrapped_var));
   bmp_type(tryunwrap_type = function_t(l_wrapped_var4, l_primary_var));
   bmp_type(unwrap_type    = function_t(l_wrapped_var5, l_primary_var2));

   sym_prim_tm(WRAP_ID, wrap_type, TY_FUN, WRAP_STDF);
   sym_prim_tm(TRYWRAP_ID, wrap_type, TY_FUN, WRAP_STDF);
   sym_mode_prim_tm(UNWRAP_ID, unwrap_type, NULL, PRIM_UNWRAP, 0, 
		    irregular_mode);
   sym_mode_prim_tm(TRYUNWRAP_ID, tryunwrap_type, NULL, PRIM_UNWRAP, 0, 
		    irregular_mode);

   drop_type(wrap_type);
   drop_type(unwrap_type);
   drop_type(tryunwrap_type);
  }


  /************************ Misc *******************************/

  {TYPE *set_limit_type, *get_limit_type, *ignore_type;
   TYPE *species_dollar_type, *outerIdVal_type;
   TYPE *bool_herm_type, *herm_str_type;
   TYPE *load_package_type, *lazyq_type;
   ROLE *noDefEx_fail_role;

   bmp_type(set_limit_type = function_t(natural_type, hermit_type));
   bmp_type(get_limit_type = function_t(hermit_type, natural_type));
   bmp_type(ignore_type    = function_t(s_any_type, hermit_type));
   bmp_type(species_dollar_type = function_t(aspecies_type, string_type));
   bmp_type(outerIdVal_type = function_t(string_type, s_any_type));
   bmp_type(bool_herm_type = function_t(boolean_type, hermit_type));
   bmp_type(herm_str_type  = function_t(hermit_type, string_type));
   bmp_type(load_package_type = function_t(string_type, hermit_type));
   bmp_type(lazyq_type     = function_t(s_any_type, boolean_type));
   bump_role(noDefEx_fail_role = basic_role("pot__fail__noDefEx"));

   /*--------------------------------------------------------------*
    * commandLine is set up when initializing the interpreter.  We *
    * just need to put it into the table - it has no primitive     *
    * info.							   *
    *--------------------------------------------------------------*/

   {EXPR *cmdline;
    bump_expr(cmdline = new_expr1t(GLOBAL_ID_E, NULL,
				   list_t(string_type), 0));

    cmdline->STR = stat_id_tb("commandLine");
    define_prim_id_tm(cmdline, 0);
    drop_expr(cmdline);
   }

   /*-------------------------------------------------------------------*
    * forget is an irregular function with nominal type `a -> `b, and   *
    * restriction type `a -> `a.					*
    *-------------------------------------------------------------------*/

   sym_mode_prim_tm(FORGET_ID, forget_type, NULL, PRIM_CAST,0,irregular_mode);
   add_restriction_tm(std_id[FORGET_ID], idf_type);

   sym_prim_tm(DOLLAR_SYM, species_dollar_type, STD_FUN, SPECIES_DOLLAR_STDF);
   sym_prim_tm(VALUE_ID,	    idf_type, PRIM_CAST, 0);
   sym_prim_tm(NOTLAZYQ_ID,	    lazyq_type, PRIM_FUN, TEST_LAZY_I);
   sym_prim_tm(PRIMITIVETRACE_ID,   set_limit_type, STD_FUN, PRIMTRACE_STDF);
   sym_prim_tm(PROFILE_ID, 	    bool_herm_type,STD_FUN, PROFILE_STDF);
   sym_prim_tm(CONTINUATIONNAME_ID, herm_str_type, STD_FUN, CONT_NAME_STDF);
   sym_prim_tm(GETSTACKDEPTH_ID, get_limit_type,
	       STD_FUN, GET_STACK_DEPTH_STDF);
   sym_prim_tm(SETSTACKLIMIT_ID, set_limit_type,STD_FUN,SET_STACK_LIMIT_STDF);
   sym_prim_tm(GETSTACKLIMIT_ID, get_limit_type,STD_FUN,GET_STACK_LIMIT_STDF);
   sym_prim_tm(SETHEAPLIMIT_ID,  set_limit_type,STD_FUN,SET_HEAP_LIMIT_STDF);
   sym_prim_tm(GETHEAPLIMIT_ID,  get_limit_type,STD_FUN,GET_HEAP_LIMIT_STDF);
   sym_prim_tm(IGNORE_ID,        ignore_type, HERM_FUN, HERMIT_I);
   sym_mode_prim_tm(OUTERIDVAL_ID, outerIdVal_type, noDefEx_fail_role,
		     TY_PRIM_FUN, GLOBAL_ID_VAL_I, NULL);
   sym_prim_tm(LOADPACKAGE_ID,   load_package_type,
				 STD_FUN, LOAD_PACKAGE_STDF);

   expect_sym_prim_tm(UNDEFINEDPATTERNFUNCTION_ID, forget_type, 0, EXPECT_ATT);
   issue_missing_tm(std_id[UNDEFINEDPATTERNFUNCTION_ID], forget_type, 
		    strong_mode);

   if(compiling_standard_asi) {
     TYPE *acquire_box_type;
     bmp_type(acquire_box_type =
       function_t(s_any_type2, pair_t(s_any_box, boolean_type)));
     no_gen_prim = TRUE;
     primitive_tm("zz__acquireBox", acquire_box_type, 
		  STD_FUN, ACQUIRE_BOX_STDF);
     primitive_tm("zz__cast", forget_type, PRIM_CAST, 0);
     no_gen_prim = FALSE;
     drop_type(acquire_box_type);
   }

   drop_type(set_limit_type);
   drop_type(get_limit_type);
   drop_type(ignore_type);
   drop_type(species_dollar_type);
   drop_type(outerIdVal_type);
   drop_type(bool_herm_type);
   drop_type(herm_str_type);
   drop_type(lazyq_type);
   drop_role(noDefEx_fail_role);
  } /* end Misc */

  /*---------------------------*
   * Drop the shared variables *
   *---------------------------*/

  drop_type(s_any_type);
  drop_type(s_any_type2);
  drop_type(s_any_list);
  drop_type(s_any_box);
  drop_type(s_any_pair);
  drop_type(s_any_pair_rev);
  drop_type(s_any_EQ);
  drop_mode(underrides_mode);
  drop_mode(irregular_mode);
  drop_mode(strong_mode);
}


/******************************************************************
 * 			DO_PRIM_IMPORT				  *
 ******************************************************************
 * Declare the primitives indicated by name which.  If we are     *
 * in the export part of a package, then generate definitions     *
 * for the primitives as well.					  *
 ******************************************************************/

void do_prim_import(char *which)
{
  /*************************** System *******************************/

  if(strcmp(which, "system") == 0) {

    TYPE *doCmd_type, *getenv_type, *chdir_type, *date_type, *seconds_type;
    TYPE *fstat_type, *perm_type, *rename_type, *dirlist_type;
    TYPE *environ_type;
    ROLE *doCmd_role, *chdir_role, *systemEx_fail_role;

    bmp_type(doCmd_type =
	 function_t(pair_t(list_t(string_type), string_type), string_type));
    bmp_type(getenv_type = function_t(string_type, string_type));
    bmp_type(chdir_type  = function_t(string_type, hermit_type));
    bmp_type(date_type   = function_t(hermit_type, string_type));
    bmp_type(seconds_type = function_t(hermit_type, natural_type));
    bmp_type(rename_type =
	     function_t(pair_t(string_type, string_type), hermit_type));
    bmp_type(dirlist_type = function_t(string_type, list_t(string_type)));
    bmp_type(environ_type = function_t(hermit_type, 
				    list_t(pair_t(string_type, string_type))));
    bmp_type(perm_type =
	     pair_t(boolean_type, pair_t(boolean_type, boolean_type)));
    bmp_type(fstat_type = function_t(string_type,
			     pair_t(boolean_type,
				 pair_t(natural_type,
				   pair_t(natural_type,
				     pair_t(perm_type,
					 pair_t(natural_type,
					        natural_type)))))));
    bump_role(systemEx_fail_role = basic_role("pot__fail__systemEx"));
    bump_role(doCmd_role = systemEx_fail_role);
    SET_ROLE(doCmd_role, meld_roles(doCmd_role, noFileEx_fail_role));
    SET_ROLE(doCmd_role, meld_roles(doCmd_role, emptyListEx_fail_role));
    bump_role(chdir_role = noFileEx_fail_role);
    SET_ROLE(chdir_role, meld_roles(chdir_role, sideEffect_role));

    import_sym_mode_prim_tm(DOCMD_ID, 	doCmd_type, doCmd_role,
			    LIST_OP, DOCMD_STDF, 0);
    import_sym_mode_prim_tm(CHANGEDIRECTORY_ID, chdir_type, sideEffect_role,
			    STD_FUN, CHDIR_STDF, 0);
    import_sym_mode_prim_tm(GETCWD_ID, date_type, NULL, 
			    STD_FUN, GETCWD_STDF, 0);
    import_sym_mode_prim_tm(GETENV_ID,  getenv_type, domainEx_fail_role,
			    STD_FUN, GETENV_STDF, 0);
    import_sym_prim_tm(DATETIME_ID, date_type, STD_FUN, DATE_STDF);
    import_sym_prim_tm(SECONDSSINCETIMEZERO_ID, seconds_type, 
		       STD_FUN, SECONDS_STDF);
    
    import_sym_mode_prim_tm(BASICFILESTATUS_ID, fstat_type, noFileEx_fail_role,
			    STD_FUN, FSTAT_STDF, 0);
    import_sym_mode_prim_tm(REMOVEFILE_ID, 	chdir_type, chdir_role,
			    STD_FUN, RM_STDF, 0);
    import_sym_mode_prim_tm(REMOVEDIRECTORY_ID, chdir_type, chdir_role,
			    STD_FUN, RMDIR_STDF, 0);
    import_sym_mode_prim_tm(MAKEDIRECTORY_ID, 	chdir_type, sideEffect_role,
			    STD_FUN, MKDIR_STDF, 0);
    import_sym_mode_prim_tm(RENAMEFILE_ID, 	rename_type, chdir_role,
			    STD_OP, RENAME_STDF, 0);
    import_sym_mode_prim_tm(DIRLIST_ID,		dirlist_type, NULL, STD_FUN, 
			    DIRLIST_STDF, 0);
    import_sym_mode_prim_tm(ENVIRON_ID, 	environ_type, NULL, STD_FUN,
			    OS_ENV_STDF, 0);

    drop_type(doCmd_type);
    drop_type(getenv_type);
    drop_type(chdir_type);
    drop_type(date_type);
    drop_type(seconds_type);
    drop_type(rename_type);
    drop_type(dirlist_type);
    drop_type(perm_type);
    drop_type(fstat_type);
    drop_type(environ_type);
    drop_role(doCmd_role);
    drop_role(chdir_role);
    drop_role(systemEx_fail_role);
  }  


  /******************** Strings ***********************************/

  else if(strcmp(which, "string") == 0) {

    /*---------------------------------------------------------------*
     * This import should only be done after importing denseset.ast. *
     * Package string.ast has already done that for us.              *
     *---------------------------------------------------------------*/

    TYPE *scanfor_type, *charset_type, *arg_type, *result_type;

    charset_type = fam_mem_t(fam_id_t("DenseSet"), char_type);
    arg_type     = pair_t(string_type, 
			  pair_t(char_type,
			         pair_t(charset_type, boolean_type)));
    result_type  = pair_t(natural_type, pair_t(string_type, char_type));
    bmp_type(scanfor_type = function_t(arg_type, result_type));

    import_sym_prim_tm(SCANFORCHAR_ID, scanfor_type, LIST_OP, SCAN_FOR_STDF);

    drop_type(scanfor_type);
  }

  /******************** Bit vectors ********************************/

  else if(strcmp(which, "bitvec") == 0) {
    TYPE *bv_not_type, *bv_or_type, *bv_sh_type, *bv_min_type, *bv_mask_type;
    TYPE *bv_setbits_type, *bvfield_type;

    bmp_type(bv_not_type = function_t(integer_type, integer_type));
    bmp_type(bv_or_type  =
	     function_t(pair_t(integer_type, integer_type),integer_type));
    bmp_type(bv_sh_type  =
	     function_t(pair_t(integer_type, natural_type), integer_type));
    bmp_type(bv_min_type =
	     function_t(integer_type, pair_t(natural_type, integer_type)));
    bmp_type(bv_mask_type = function_t(natural_type, integer_type));
    bmp_type(bv_setbits_type = 
	     function_t(pair_t(integer_type, 
			       pair_t(list_t(natural_type), 
				      boolean_type)), 
			integer_type));
    bmp_type(bvfield_type = 
	     function_t(pair_t(integer_type,
			       pair_t(natural_type, natural_type)),
			integer_type));

    import_sym_prim_tm(NBVAND_ID,  bv_or_type,  STD_OP, BV_AND_STDF);
    import_sym_prim_tm(NBVOR_ID,   bv_or_type,  STD_OP, BV_OR_STDF);
    import_sym_prim_tm(NBVXOR_ID,  bv_or_type,  STD_OP, BV_XOR_STDF);
    import_sym_prim_tm(NBVSHL_ID,  bv_sh_type,  STD_OP, BV_SHL_STDF);
    import_sym_prim_tm(NBVSHR_ID,  bv_sh_type,  STD_OP, BV_SHR_STDF);
    import_sym_prim_tm(NBVMIN_ID,  bv_min_type, STD_FUN, BV_MIN_STDF);
    import_sym_prim_tm(NBVMAX_ID,  bv_min_type, STD_FUN, BV_MAX_STDF);
    import_sym_prim_tm(NBVMASK_ID, bv_mask_type, STD_FUN, BV_MASK_STDF);
    import_sym_prim_tm(NBVSETBITS_ID, bv_setbits_type, STD_OP,BV_SETBITS_STDF);
    import_sym_prim_tm(NBVFIELD_ID, bvfield_type, STD_OP, BV_FIELD_STDF);

    drop_type(bv_not_type);
    drop_type(bv_or_type);
    drop_type(bv_sh_type);
    drop_type(bv_min_type);
    drop_type(bv_mask_type);
    drop_type(bv_setbits_type);
    drop_type(bvfield_type);
  }

  /*********************** Math ***********************************/

  else if(strcmp(which, "math") == 0) {

    TYPE *realt_to_realt_fun, *realt_to_intt_pair_fun;

    bmp_type(realt_to_realt_fun = function_t(real_type, real_type));
    bmp_type(realt_to_intt_pair_fun = 
		function_t(real_type, pair_t(integer_type, integer_type)));


    import_sym_prim_tm(EXP_ID,	  realt_to_realt_fun, STD_FUN,	EXP_STDF);
    import_sym_prim_tm(SIN_ID,	  realt_to_realt_fun, STD_FUN,	SIN_STDF);
    import_sym_prim_tm(INVTAN_ID, realt_to_realt_fun, STD_FUN,	INVTAN_STDF);
    import_sym_prim_tm(PULLAPARTREAL_ID, realt_to_intt_pair_fun, 
				  STD_FUN, PULL_APART_REAL_STDF);
    import_sym_mode_prim_tm(SQRT_ID, realt_to_realt_fun, domainEx_fail_role,
				STD_FUN, SQRT_STDF, 0);
    import_sym_mode_prim_tm(LN_ID, realt_to_realt_fun, 	domainEx_fail_role,
				STD_FUN, LN_STDF, 0);

    drop_type(realt_to_realt_fun);
    drop_type(realt_to_intt_pair_fun);
  }

  else { /* unknown import name */
    syntax_error1(UNKNOWN_PRIM_IMPORT_ERR, which, 0);
    return;
  }

  /*------------------------------------------------------------*
   * These definitions need to be generated into the .aso file. *
   * After generating, get rid of them to prevent them from     *
   * being generated again.					*
   *------------------------------------------------------------*/

  gen_standard_funs(&import_prim_fun_descr);
  SET_EXPR(import_prim_fun_descr, NULL);
}

/******************************************************************
 * 			OV_STD_FUNS				  *
 ******************************************************************
 * Primitive functions that override definitions in standard.asi. *
 ******************************************************************/

void ov_std_funs()
{
  TYPE *string_pair, *string_eq;
# ifdef NEVER
    TYPE* string_cp;
# endif

  /*---------------------------------------------------------------*
   * Don't define these for the standard package, since otherwise  *
   * conflict with definitions there. 				   *
   *---------------------------------------------------------------*/

  if(!compiling_standard_asi) {
    bmp_type(string_pair   = pair_t(string_type,	  string_type));
    bmp_type(string_eq     = function_t(string_pair,     boolean_type));

#   ifdef NEVER
      bmp_type(string_cp     = function_t(string_pair,     comparison_type));
      sym_mode_prim_tm(COMPARE_ID, string_cp, NULL, STD_OP,
				 COMPARE_STDF, FORCE_MODE);
      drop_type(string_cp);
#   endif
    sym_mode_prim_tm(EQ_SYM, 	 string_eq, NULL, PRIM_OP,
				 EQ_I, FORCE_MODE);
    sym_mode_prim_tm(NE_SYM, 	 string_eq, NULL, PRIM_NOT_OP,
				 EQ_I, FORCE_MODE);
    drop_type(string_pair);
    drop_type(string_eq);
    if(reports != NULL) {
      free_report_record(reports);
      reports = NULL;
    }
  }
}


/****************************************************************
 * 			SPECIAL_STD_FUNS			*
 ****************************************************************
 * Primitives that should appear as if defined in standard.asi. *
 ****************************************************************/

void special_std_funs()
{
  TYPE *t, *eq_list;
  EXPR *ex;
  PART *p;

  if(!compiling_standard_asi) {

    /*------------------------------------------------------------*
     * Make == a QEQ primitive for lists.  This improves handling *
     * of nil.							  *
     *------------------------------------------------------------*/

    eq_list = list_t(any_EQ);
    bmp_type(t = function_t(pair_t(eq_list, eq_list), boolean_type));
    SET_TYPE(t, copy_type(t,0));
    bump_expr(ex = typed_global_sym_expr(EQ_SYM, t, 0));
    SET_EXPR(ex, skip_sames(ex));
    if(EKIND(ex) == GLOBAL_ID_E && ex->GIC != NULL) {
      for(p = ex->GIC->entparts;
	  p != NULL && overlap_u(t,p->ty) != EQUAL_OV;
	  p = p->next);
      if(p != NULL) p->primitive = PRIM_QEQ;
    }
    drop_type(t);
    drop_expr(ex);
  }
}


