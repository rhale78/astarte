/*****************************************************************
 * File:    generate/prim.c
 * Purpose: Provide support for handling primitive instructions.
 * Author:  Karl Abrahamson
 *****************************************************************/

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
 * This file contains functions for generating code for performing	*
 * primitive functions and creating primitive entities.			*
 ************************************************************************/

#include "../misc/misc.h"
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
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			INVERT_PRIM				*
 ****************************************************************
 * Invert_prim[prim] is the inverse of primitive prim.  For 	*
 * example, invert_prim[PRIM_WRAP] = PRIM_UNWRAP.  A value of 0 *
 * indicates no inverse. All primitives beyond PRIM_EXC_TEST 	*
 * are assumed to have an invert_prim value of 0. 		*
 ****************************************************************/

PRIVATE char invert_prim[] =
{
 /* 0 			*/	   0,
 /* PRIM_CAST		*/	   PRIM_CAST,
 /* PRIM_ENUM_CAST 	*/	   PRIM_CAST,
 /* PRIM_LONG_ENUM_CAST */ 	   PRIM_CAST,
 /* PRIM_SPECIES	*/	   0,
 /* PRIM_QWRAP		*/	   PRIM_QUNWRAP,
 /* PRIM_QUNWRAP	*/	   PRIM_QWRAP,
 /* PRIM_QTEST  	*/	   0,
 /* PRIM_DWRAP  	*/	   PRIM_DUNWRAP,
 /* PRIM_DUNWRAP 	*/	   PRIM_DWRAP,
 /* PRIM_EXC_WRAP 	*/ 	   PRIM_EXC_UNWRAP,
 /* PRIM_EXC_UNWRAP 	*/	   PRIM_EXC_WRAP,
 /* PRIM_EXC_TEST 	*/	   0,
 /* PRIM_WRAP 		*/	   PRIM_UNWRAP,
 /* PRIM_UNWRAP 	*/	   PRIM_WRAP,
 /* TY_PRIM_FUN 	*/	   0,
 /* TY_PRIM_OP  	*/	   0,
 /* TY_FUN  		*/  	   0
};


/****************************************************************
 *			INVERT_PRIM_VAL				*
 ****************************************************************
 * invert_prim_val(prim) returns the inverted value of prim.    *
 ****************************************************************/

int invert_prim_val(int prim) 
{
  if(prim > LAST_TY_PRIM) return 0;
  return invert_prim[prim];
}


/*****************************************************************
 *			GEN_PRIM_G				 *
 *****************************************************************
 * Generate instructions to apply a primitive function described *
 * by prim, with parameter instr, to the top of the stack.	 *
 * e1 is the expression being applied, -- it is needed for some	 *
 * kinds of primitives. 					 *
 *								 *
 * XREF:							 *
 *   Called in genexec.c to generate code for application of	 *
 *   a primitive function.					 *
 *								 *
 *   Called in genstd.c to generate primitives into standard	 *
 *   function code.						 *
 *****************************************************************/

void gen_prim_g(int prim, int instr, EXPR *e1)
{
  UBYTE cinstr = (UBYTE) instr;

# ifdef DEBUG
    if(trace_gen > 1) trace_t(155, prim, instr);
# endif

  switch(prim) {
    case HERM_FUN:

      /*----------------------------------------------------------------*
       * A HERM_FUN has parameter ().  It pops () before executing the  *
       * instruction.							*
       *----------------------------------------------------------------*/

      gen_g(POP_I);
      gen_g(cinstr);
      break;

    case PRIM_ENUM_CAST:
      gen_g(ENUM_CHECK_I);
      gen_g(cinstr);
      break;

    case PRIM_LONG_ENUM_CAST:
      genP_g(LONG_ENUM_CHECK_I, instr);
      break;

    case PRIM_FUN:

      /*-----------------------------------------------------------------*
       * Some primitive functions have output (), but don't push () onto *
       * the stack when they are run.  So we must follow them with 	 *
       * HERMIT_I.  This will typically be followed by POP_I.  The pair  *
       * HERMIT_I POP_I will be removed by the code improver.		 *
       *-----------------------------------------------------------------*/

      gen_g(cinstr);
      if (cinstr == PAUSE_I ||
	  cinstr == REPAUSE_I ||
          cinstr == MAKE_EMPTY_I ||
          cinstr == CUT_I) {
        gen_g(HERMIT_I);
      }
      break;

    /*------------------------------------------------------------*
     * A constant can only be applied if it is ().  Treat () like *
     * a cast.							  *
     *------------------------------------------------------------*/

    case PRIM_CONST:
    case PRIM_CAST:
      break;

    case PRIM_OP:
    case PRIM_NOT_OP:

      /*------------------------------------------------------------------*
       * Operators must split their parameter before running.  This often *
       * leads to the pair PAIR_I SPLIT_I being generated.  Such a pair   *
       * of instructions is removed by the code improver.		  *
       *								  *
       * Some instructions need to be followed by HERMIT_I, since they    *
       * do not push their result, ().					  *
       *------------------------------------------------------------------*/

      gen_g(SPLIT_I);
      gen_g(cinstr);
      if(cinstr == ASSIGN_I 
         || cinstr == FPRINT_I
	 || cinstr == RAW_SHOW_I
	 || cinstr == ASSIGN_INIT_I
         || cinstr == ASSIGN_NODEMON_I) {
        gen_g(HERMIT_I);
      }
      if(prim == PRIM_NOT_OP) gen_g(NOT_I);
      break;

    case PRIM_SELECT:
      gen_selection_g(toint(instr), e1);
      break;

    case PRIM_MODIFY:
      gen_modify_g(toint(instr), e1);
      break;

    case STD_OP:
      gen_g(SPLIT_I);
      gen_g(BINARY_PREF_I);
      gen_g(cinstr);
      break;

    case STD_FUN:
      gen_g(UNARY_PREF_I);
      gen_g(cinstr);
      break;

    case LIST_FUN:
      gen_g(LIST1_PREF_I);
      gen_g(cinstr);
      break;

    case LIST_OP:
      gen_g(SPLIT_I);
      gen_g(LIST2_PREF_I);
      gen_g(cinstr);
      break;

    case UNK_FUN:
      gen_g(UNK_PREF_I);
      gen_g(cinstr);
      break;
      
    case TY_PRIM_FUN:
    case TY_PRIM_OP:
    case TY_FUN:

      /*-------------------------------------------------------------------*
       * A TY-prim needs to have, as a parameter, the offset in the global *
       * environment of its own type.  However, for an irregular primitive,*
       * the codomain is forced to ().					   *
       *-------------------------------------------------------------------*/

      {TYPE *tt, *e1_ty;
       e1 = skip_sames(e1);
       bump_type(e1_ty = e1->ty);
       bump_type(tt = find_u(e1_ty));
       if(e1->irregular) {
	 SET_TYPE(tt, function_t(tt->TY1, hermit_type));
       }
       reset_frees(tt);

       /*----------------------------------------------------------------*
        * If the primitive has a type that contains no dynamically bound *
        * type variables, then its type can be put in the global 	 *
        * environment, and the offset used as the parameter to the 	 *
        * instruction.							 *
        *----------------------------------------------------------------*/

       if(find_u(tt)->free == 0) {
         int n = type_offset_g(tt);
         if(prim == TY_PRIM_OP) gen_g(SPLIT_I);
	 if(prim == TY_FUN) gen_g(TY_PREF_I);
         gen_g(cinstr);
         gen_g(n);
       }

       /*---------------------------------------------------------------*
        * If the type contains dynamically bound variables,  we need to *
        * do a dispatched call to the function for this primitive.      *
        *---------------------------------------------------------------*/

       else {
	 gen_global_id_g(e1, TRUE);
 	 gen_g(REV_APPLY_I);
       }
      
       drop_type(tt);
       drop_type(e1_ty);
      }

      /*------------------------------------------------------------*
       * A RAW_SHOW_I needs to have () pushed for it after it runs. *
       *------------------------------------------------------------*/

      if(cinstr == RAW_SHOW_I) gen_g(HERMIT_I);
      break;

    case PRIM_WRAP:
      gen_wrap_g(find_u(e1->ty)->TY1);
      break;

    case PRIM_QUNWRAP:
      gen_g(QUNWRAP_I);
      gen_g(cinstr);
      break;

    case PRIM_DUNWRAP:
      gen_g(QUNWRAP_I);
      gen_g(cinstr);
      /* No break */
    case PRIM_UNWRAP:
      gen_unwrap_g(find_u(e1->ty)->TY2, e1, e1->LINE_NUM);
      break;

    case PRIM_DWRAP:
      gen_wrap_g(find_u(e1->ty)->TY1);
      /* No break */
    case PRIM_QWRAP:
      gen_g(QWRAP_I);
      gen_g(cinstr);
      break;

    case PRIM_EXC_WRAP:
      genP_g(EXC_WRAP_I, instr);
      break;

    case PRIM_QTEST:
      gen_g(QTEST_I);
      gen_g(cinstr);
      break;

    case PRIM_EXC_TEST:
      genP_g(EXC_TEST_I, instr);
      break;

    case PRIM_EXC_UNWRAP:
      genP_g(EXC_UNWRAP_I, instr);
      break;

    case PRIM_BIND_UNK:
      gen_g(SPLIT_I);
      gen_g(BIND_UNKNOWN_I);
      gen_g(cinstr);
      gen_g(HERMIT_I);
      break;

    default: 
      semantic_error1(BAD_APPLY_PRIM_ERR, (char *) prim, e1->LINE_NUM);
      break;

  } /* end switch */
}


/******************************************************************
 *			GEN_ENT_PRIM_G				  *
 ******************************************************************
 * Generate instructions to put the entity described by primitive *
 * prim, with argument instr, on the stack.  Return TRUE on 	  *
 * success. If prim is not an entity primitive, then return FALSE.*
 ******************************************************************/

Boolean gen_ent_prim_g(int prim, int instr)
{
  switch(prim) {
    case PRIM_CONST:
      gen_g(instr);
      return TRUE;

    case STD_CONST:
      gen_g(CHAR_I); gen_g(instr);
      return TRUE;

    case STD_BOX:
      gen_g(STD_BOX_I); gen_g(instr);
      return TRUE;

    case EXCEPTION_CONST:
      genP_g(EXC_CONST_I, instr);
      return TRUE;

    default:
      return FALSE;
  }
}


/****************************************************************
 *			GEN_SPECIAL_G				*
 ****************************************************************
 * Generate special expression e. 				*
 *								*
 * The returned list is the list of pairs (V,A), where V became *
 * bound to A during this code.					*
 ****************************************************************/

void gen_special_g(EXPR *e)
{
  int kind = e->PRIMITIVE;
  switch(kind) {
    case PRIM_SPECIES:
      generate_type_g(e->E1->ty, 0);
      gen_g(SPECIES_AS_VAL_I);
      break;

    case PRIM_EXCEPTION:
      gen_g(EXCEPTION_I);
      break;

    case PRIM_STACK:
      break;

    case EXCEPTION_CONST:
      genP_g(EXC_CONST_I, e->SCOPE);
      break;

    case PRIM_TARGET:
      semantic_error(TARGET_ERR, e->LINE_NUM);
      break;

    case STD_BOX:
      gen_g(STD_BOX_I);
      goto gen_scope;

    case STD_CONST:
      gen_g(CHAR_I);
      /* No break - fall through to next case. */

    case PRIM_CONST:
    gen_scope:
      gen_g(e->SCOPE);
      break;    

    case PRIM_CAST:

      /*-----------------------------------------------*
       * Can just cast idf:()->() to be this function. *
       *-----------------------------------------------*/

      {EXPR *idf;
       TYPE *herm_idf_type;
       bump_type(herm_idf_type = function_t(hermit_type, hermit_type));
       bump_expr(idf = 
		 typed_global_sym_expr(IDF_ID, herm_idf_type, e->LINE_NUM));
       gen_global_id_g(idf, FALSE);
       drop_expr(idf);
       drop_type(herm_idf_type);
       break;
      }
       
    default:

      /*----------------------------------------------------------------*
       * If we have a special function, and it is not being applied.    *
       * then we need to build a function.				*
       *----------------------------------------------------------------*/

      {TYPE *t;
       package_index loc1, loc1_high;

       loc1_high = begin_labeled_instr_g();
       gen_g(FUNCTION_I);
       loc1 = gen_g(0);
       t = find_u(e->ty);
       generate_type_g(t->TY2, 1);
       gen_g(END_LET_I);
       gen_g(0);
       gen_prim_g(kind, e->SCOPE, e);
       gen_g(RETURN_I);
       label_g(loc1_high, loc1);
       break;
     }
  }
}


/****************************************************************
 *			GET_PRIM_G, GET_PAT_PRIM_G		*
 ****************************************************************
 * get_general_prim_g works as follows.  			*
 *								*
 * If e is a global id, SPECIAL_E node or CONST_E node that	*
 * is a primitive, get_general_prim_g returns the primitive 	*
 * code for e and sets 						*
 *								*
 *    *instr     = the instruction or argument that goes with   *
 *	           the primitive, if there is one.  This is	*
 *		   only relevant when use_pat_parts is FALSE.	*
 *								*
 *    *irregular = the irregular destructor bit.  This is only	*
 *		   relevant when use_pat_parts is FALSE.	*
 *								*
 *    *selection_list = the selection_list of the primitive.  	*
 *								*
 *    *part      = the PART, from the global id table, that	*
 *		   the primitive info comes from, if there is	*
 *		   one.  This is NULL if the information was	*
 *		   not obtained from a PART.			*
 *								*
 * The same is done if e is a cast of a primitive, such as	*
 * cast(idf).							*
 *								*
 * If e is not one of those kinds of expression, or is a global *
 * id that is not a primitive, get_general_prim_g returns 0.  	*
 *								*
 * If use_pat_parts is false, information about primitives is	*
 * searched for in the entparts chain.  If use_pat_parts is	*
 * true, information about primitives is searched for in the 	*
 * patparts chain.						*
 *								*
 * gen_prim_g is the same as get_general_prim_g, but with	*
 * use_pat_parts fixed false.					*
 *								*
 * gen_pat_prim_g is the same as get_general_prim_g, but with	*
 * use_pat_parts fixed true.					*
 ****************************************************************/

PRIVATE int
get_general_prim_g(EXPR *e, int *instr, ENTPART **part,
		   Boolean *irregular, LIST **selection_list,
		   Boolean use_pat_parts)
{
  GLOBAL_ID_CELL *gic;
  PART *p;
  EXPR_TAG_TYPE e_kind;
  int ov;
  int result = 0;
# ifdef DEBUG
    char *name;
# endif

  /*-------------------------------------------*
   * Defaults and test for NULL, just in case. *
   *-------------------------------------------*/

  *instr          = 0;
  *part           = NULL;
  *irregular      = 0;
  *selection_list = NIL;

  if(e == NULL) return 0;

# ifdef DEBUG
    name = NULL;
# endif

  e      = skip_sames(e);
  e_kind = EKIND(e);

  /*-------------------------------------------------------*
   * If e is a cast from another expression, then use that *
   * other expression. 					   *
   *-------------------------------------------------------*/

  while(e_kind == APPLY_E) {
    EXPR* e1 = skip_sames(e->E1);
    if(EKIND(e1) == SPECIAL_E && e1->PRIMITIVE == PRIM_CAST) {
      e = skip_sames(e->E2);
      e_kind = EKIND(e);
    }
    else return 0;
  }

  /*----------------------------*
   ****** SPECIAL_E nodes. ******
   *----------------------------*/

  if(e_kind == SPECIAL_E) {
    *instr          = e->SCOPE;
    result          = e->PRIMITIVE;
    *irregular      = e->irregular;
    *selection_list = e->EL3;
#   ifdef DEBUG
      name = e->STR;
#   endif
  }

  /*------------------------------*
   ****** GLOBAL_ID_E nodes. ******
   ****** PAT_FUN_E nodes    ******
   *------------------------------*/

  else if(e_kind == GLOBAL_ID_E || e_kind == PAT_FUN_E) {
#   ifdef DEBUG
      name = e->STR;
#   endif

#   ifdef DEBUG
      if(trace_gen > 1) {
        trace_t(242, nonnull(name));
	trace_ty(e->ty);
	tracenl();
      }
#   endif

    /*--------------------------------*
     * Get the parts chain to search. *
     *--------------------------------*/

    gic = e->GIC;
    if(gic == NULL) return 0;
    if(!use_pat_parts) p = gic->entparts;
    else {
      EXPAND_INFO* pi = gic->expand_info;
      p = (pi == NULL) ? NULL : pi->patfun_rules;
    }

    /*----------------------------------------------------------*
     * Search the chain for a primitive that has a polymorphic  *
     * type that contains the type of e.  If e is irregular,    *
     * then only check the domain.				*
     *----------------------------------------------------------*/

    for(; p != NULL; p = p->next) {
      if(p->primitive > 0) {
	if(p->irregular) {
	  TYPE* e_ty = find_u(e->ty);
	  TYPE* p_ty = find_u(p->ty);
	  if(TKIND(e_ty) != FUNCTION_T) continue;
	  ov = half_overlap_u(e_ty->TY1, p_ty->TY1);
	}
	else {
	  ov = half_overlap_u(e->ty, p->ty);
	}
        if(ov == EQUAL_OR_CONTAINED_IN_OV) {
	  *instr = p->arg;
	  *part  = p;
	  *irregular = p->irregular;
	  *selection_list = p->selection_info;
	  result = p->primitive;
	  break;
        }
      }
    }
  }

  /*--------------------------*
   ****** CONST_E nodes. ******
   *--------------------------*/

  else if(e_kind == CONST_E) {
    if(e->SCOPE == BOOLEAN_CONST || e->SCOPE == HERMIT_CONST) {
      result = PRIM_CONST;
      *instr = e->STR == NULL ? NIL_I : TRUE_I;
    }
  }

# ifdef DEBUG
    if(trace_gen > 1 && name != NULL) {
      trace_t(205, name);
      trace_ty(e->ty); 
      fprintf(TRACE_FILE," ::\n ");
      if(result == 0) trace_t(206);
      else {
	trace_t(25, result, *instr, toint(*irregular));
	print_str_list_nl(*selection_list);
      }
    }
# endif

  return result;
}  

/*---------------------------------------------------------------*/
    
int get_prim_g(EXPR *e, int *instr, ENTPART **part, Boolean *irregular,
	       INT_LIST **selection_list)
{
  return get_general_prim_g(e, instr, part, irregular, selection_list, 0);
}

/*---------------------------------------------------------------*/

int get_pat_prim_g(EXPR *e, int *instr, ENTPART **part, Boolean *irregular,
		   INT_LIST **selection_list)
{
  return get_general_prim_g(e, instr, part, irregular, selection_list, 1);
}



