 /**************************************************************
 * File:    classes/type.c
 * Purpose: Implement operations for TYPE expressions that build
 *          TYPE nodes, and provide basic TYPE structures for the
 *          compiler and interpreter.
 * Author:  Karl Abrahamson
 **************************************************************/

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

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../parser/tokens.h"
#include "../alloc/allocate.h"
#include "../clstbl/classtbl.h"
#include "../clstbl/meettbl.h"
#include "../clstbl/stdtytbl.h"
#include "../clstbl/abbrev.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../unify/unify.h"
#ifdef TRANSLATOR
# include "../error/error.h"
# include "../dcls/dcls.h"
#endif
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 *			SOME TYPES				*
 ****************************************************************/

TYPE *any_type;		/* `a (used below in other types)  */
TYPE *any_box;		/* Box(`a)	           	   */

#ifdef TRANSLATOR
TYPE *any_type2;	/* `b (used below in others)  */
TYPE *any_list;		/* [`a]		              */
TYPE *any_EQ;		/* EQ`a		     	      */
TYPE *any_pair;		/* (`a, `b)		      */
TYPE *any_pair_rev;	/* (`b, `a)		      */
TYPE *cons_type;        /* (`a, [`a]) -> [`a] 	      */
TYPE *content_type;     /* Box(`a) -> `a              */
TYPE *assign_type;	/* (Box(`a), `a) -> ()        */
TYPE *idf_type;		/* `a -> `a		      */
TYPE *forget_type;	/* `a -> `b		      */
TYPE *bad_type;		/* used for erroneous types   */
#endif

/*----------------------------------------------------------------*
 * The following are used in the interpreter.  They must be       *
 * entered into the type table in tables/typehash.c:init_type_tb. *
 *----------------------------------------------------------------*/

TYPE *outfile_type;     /* Outfile(Char) 		*/
TYPE *string_type;	/* String (= [Char]) 		*/

/*-------------------------------------------------------------------*
 * The following are not used in the interpreter.  If they are, then *
 * they need to be put into the type table in tables/typehash.c.     *
 *-------------------------------------------------------------------*/

#ifdef TRANSLATOR
TYPE *show_apply_type;  /* (String, `a) -> String */
TYPE *exc_to_bool_type; /* Exception -> Boolean 	*/
TYPE *exc_to_str_type;  /* Exception -> String 		*/
TYPE *str_to_exc_type;  /* String -> Exception 		*/
#endif

/*--------------*
 * Sets of tags *
 *--------------*/

intset fam_tkind_set, fam_type_tkind_set, fam_var_tkind_set;
intset wrap_tkind_set, primary_tkind_set;

/****************************************************************
 *			OTHER VARIABLES				*
 ****************************************************************/

LPAIR  NULL_LP;
LPAIR  NOCHECK_LP;
RTYPE  NULL_RT;


/******************************************************************
 *				TKINDF				  *
 ******************************************************************
 * Return the kind field of t.  Only used in allocation test mode *
 * (GCTEST).							  *
 ******************************************************************/

#ifdef GCTEST
TYPE_TAG_TYPE tkindf(TYPE *t)
{
  if(t->ref_cnt < 0 && !force_ok_kind) {
    force_ok_kind = 1;
#   ifdef DEBUG
      fprintf(TRACE_FILE, "\n\nBad type ref count\n\n");
      long_print_ty(t, 0);
#   endif
    die(8);
  }
  return t->kind;
}
#endif


/********************************************************
 * 			NEW_TYPE			*
 ********************************************************
 * Return a new type node with kind k and TY1 field ty1.*
 * Handles ref counts, etc.				*
 ********************************************************/

TYPE* new_type(TYPE_TAG_TYPE kind, TYPE *ty1)
{
  register TYPE* t = allocate_type();
  t->kind = kind;
  bump_type(t->ty1 = ty1);

  if(kind >= FIRST_BOUND_T) t->copy = 1;
  else if(kind >= BAD_T) t->copy = 0;
  else if(ty1 == NULL_T || ty1->copy == 1) t->copy = 1;
  return t;
}


/********************************************************
 * 			NEW_TYPE2			*
 ********************************************************
 * Return a new type node with given values for kind,	*
 * ty1 and ty2.  Handles ref counts, etc.		*
 ********************************************************/

TYPE* new_type2(TYPE_TAG_TYPE kind, TYPE *ty1, TYPE *ty2)
{
  register TYPE* t = new_type(kind, ty1);
  bump_type(t->TY2 = ty2);

  if (ty2 == NULL_T || ty2->copy == 1) t->copy = 1;
  return t;
}


/********************************************************
 * 			BOX_T				*
 ********************************************************
 * Return type Box(t).					*
 ********************************************************/

TYPE* box_t(TYPE *t)
{
  return new_type2(FAM_MEM_T, t, box_fam);
}


/********************************************************
 * 			FAM_MEM_T			*
 ********************************************************
 * Return type fam(t).					*
 ********************************************************/

TYPE* fam_mem_t(TYPE *fam, TYPE *t)
{
  return new_type2(FAM_MEM_T, t, fam);
}


/********************************************************
 * 			LIST_T				*
 ********************************************************
 * Return type [t].					*
 ********************************************************/

TYPE* list_t(TYPE *t)
{
  return new_type2(FAM_MEM_T, t, list_fam);
}


/********************************************************
 * 			PAIR_T				*
 ********************************************************
 * Return type (s,t).					*
 ********************************************************/

TYPE* pair_t(TYPE *s, TYPE *t)
{
  return new_type2(PAIR_T, s, t);
}


/********************************************************
 * 			FUNCTION_T			*
 ********************************************************
 * Return type s -> t.					*
 ********************************************************/

TYPE* function_t(TYPE *s, TYPE *t)
{
  return new_type2(FUNCTION_T, s, t);
}


/********************************************************
 *			VAR_T				*
 ********************************************************
 * Return a type or family variable with domain		*
 * given by class-table-cell ctc.			*
 ********************************************************/

TYPE* var_t(CLASS_TABLE_CELL *ctc)
{
  register TYPE_TAG_TYPE kind = (ctc != NULL && 
				 ctc->code == COMM_ID_CODE)
			 	   ? FAM_VAR_T
     				   : TYPE_VAR_T;
  register TYPE* t = new_type(kind, NULL);
  if(ctc == ctcs[0]) ctc = NULL;
  t->ctc = ctc;
  return t;
}


/********************************************************
 *			PRIMARY_VAR_T			*
 ********************************************************
 * Return a primary type or family variable with domain	*
 * given by class-table-cell ctc.			*
 ********************************************************/

TYPE* primary_var_t(CLASS_TABLE_CELL *ctc)
{
  register TYPE_TAG_TYPE kind = (ctc != NULL && 
				 ctc->code == COMM_ID_CODE) 
			 	   ? PRIMARY_FAM_VAR_T
     				   : PRIMARY_TYPE_VAR_T;
  register TYPE* t = new_type(kind, NULL);
  if(ctc == ctcs[0]) ctc = NULL;
  t->ctc = ctc;
  return t;
}

/********************************************************
 *			WRAP_VAR_T			*
 ********************************************************
 * Return a wrap type or family variable with domain	*
 * given by class-table-cell ctc.			*
 ********************************************************/

TYPE* wrap_var_t(CLASS_TABLE_CELL *ctc)
{
  register TYPE_TAG_TYPE kind = (ctc != NULL && 
				 ctc->code == COMM_ID_CODE) 
			 	   ? WRAP_FAM_VAR_T
     				   : WRAP_TYPE_VAR_T;
  register TYPE* t = new_type(kind, NULL);
  if(ctc == ctcs[0]) ctc = NULL;
  t->ctc = ctc;
  return t;
}


/****************************************************************
 * 			TYPE_VAR_T				*
 ****************************************************************
 * type_var_t returns a new type variable that ranges over	*
 * genus name.  Name need not be in the string table.  If name	*
 * is NULL, type_var_t returns a type var that ranges over ANY.	*
 *								*
 * Note: It is assumed that name is the name of a genus -- that *
 * is not checked.						*
 ****************************************************************/

TYPE* type_var_t(char *name)
{
  register TYPE* tt = new_type(TYPE_VAR_T, NULL_T);
  if(name != NULL_S && strcmp(name, std_type_id[ANYG_TYPE_ID]) == 0) {
    name = NULL_S;
  }
  if(name != NULL_S) {
    tt->ctc = get_ctc_tm(name);
#   ifdef TRANSLATOR
      if(tt->ctc == NULL) semantic_error1(NO_CG_ERR, display_name(name), 0);
#   endif
  }
  return tt;
}


/********************************************************
 * 			FAM_VAR_T			*
 ********************************************************
 * fam_var_t returns a new family variable that ranges	*
 * over community name.  name need not be in the string *
 * table.						*
 *							*
 * Note: It is assumed that name is the name of a 	*
 * community -- that is not checked.			*
 ********************************************************/

TYPE* fam_var_t(char *name)
{
  register TYPE* tt = new_type(FAM_VAR_T, NULL_T);
  tt->ctc = get_ctc_tm(name);
# ifdef TRANSLATOR
    if(tt->ctc == NULL) semantic_error1(NO_CG_ERR, display_name(name), 0);
# endif
  return tt;
}


/********************************************************
 *			PRIMARY_TYPE_VAR_T		*
 * 			PRIMARY_FAM_VAR_T		*
 ********************************************************
 * primary_type_var_t returns a new primary type 	*
 * variable that ranges over genus name.  		*
 *							*
 * primary_fam_var_t returns a new primary family	*
 * variable that ranges over community name.  		*
 *							*
 * name need not be in the string table.		*
 ********************************************************/

TYPE* primary_type_var_t(char *name)
{
  register TYPE* result = type_var_t(name);
  result->kind = PRIMARY_TYPE_VAR_T;
  return result;
}

/*----------------------------------------------------------*/

TYPE* primary_fam_var_t(char *name)
{
  register TYPE* result = fam_var_t(name);
  result->kind = PRIMARY_FAM_VAR_T;
  return result;
}


/********************************************************
 *			WRAP_TYPE_VAR_T			*
 * 			WRAP_FAM_VAR_T			*
 ********************************************************
 * wrap_type_var_t returns a new wrapped type variable  *
 * that ranges over genus name.  			*
 *							*
 * wrap_fam_var_t returns a new wrapped family variable *
 * that ranges over community name.  			*
 *							*
 * name need not be in the string table.		*
 ********************************************************/

TYPE* wrap_type_var_t(char *name)
{
  register TYPE* result = type_var_t(name);
  result->kind = WRAP_TYPE_VAR_T;
  return result;
}

/*----------------------------------------------------------*/

TYPE* wrap_fam_var_t(char *name)
{
  register TYPE* result = fam_var_t(name);
  result->kind = WRAP_FAM_VAR_T;
  return result;
}


/********************************************************
 * 			TYPE_ID_T			*
 ********************************************************
 * Return species name.  Return NULL if there 		*
 * is no such species.					*
 *							*
 * Note: It is assumed that name is the name of a 	*
 * species as opposed to a genus, for example. 		*
 ********************************************************/

TYPE* type_id_t(char *name)
{
  register CLASS_TABLE_CELL* ctc = get_ctc_tm(name);
  register TYPE *t;

  if(ctc == NULL) return NULL;
  t = new_type(TYPE_ID_T, NULL);
  t->ctc = ctc;
  return t;
}


/********************************************************
 * 			FAM_ID_T			*
 ********************************************************
 * Return family name.  Return NULL if there is 	*
 * no such family.					*
 *							*
 * Note: It is assumed that name is the name of a	*
 * family.  That is not checked.			*
 ********************************************************/

TYPE* fam_id_t(char *name)
{
  register CLASS_TABLE_CELL* ctc = get_ctc_tm(name);
  register TYPE *t;

  if(ctc == NULL) return NULL;
  t = new_type(FAM_T, NULL);
  t->ctc = ctc;
  return t;
}


/****************************************************************
 * 			WRAP_TF					*
 ****************************************************************
 * Return a wrapped type or family with ctc entry ctc..		*
 * If ctc is not a genus or community, then return a primary	*
 * type instead.						*
 ****************************************************************/

TYPE* wrap_tf(CLASS_TABLE_CELL *ctc)
{
  register int code = (ctc == NULL) ? GENUS_ID_CODE : ctc->code;
  register TYPE_TAG_TYPE 
               kind = (code == GENUS_ID_CODE) ? WRAP_TYPE_T :
		      (code == COMM_ID_CODE)  ? WRAP_FAM_T  :
                      (code == TYPE_ID_CODE)  ? TYPE_ID_T
			                      : FAM_T;
  TYPE* t = new_type(kind, NULL);
  if(ctc == ctcs[0]) ctc = NULL;
  t->ctc = ctc;
  return t;
}


/****************************************************************
 * 			FICTITIOUS_TF				*
 ****************************************************************
 * Return a new fictitious primary species or family with ctc	*
 * entry ctc and number num.					*
 ****************************************************************/

TYPE* fictitious_tf(CLASS_TABLE_CELL *ctc, int num)
{
  register int code = (ctc == NULL) ? GENUS_ID_CODE : ctc->code;
  register TYPE_TAG_TYPE 
               kind = (code == GENUS_ID_CODE) 
			? FICTITIOUS_TYPE_T 
			: FICTITIOUS_FAM_T;
  TYPE* t = new_type(kind, NULL);
  t->ctc  = ctc;
  t->TNUM = num;
  return t;
}


/****************************************************************
 * 			NEW_FICTITIOUS_TF			*
 ****************************************************************
 * Return a new fictitious primary species or family with ctc	*
 * entry ctc and new number.					*
 ****************************************************************/

PRIVATE LONG next_fictitious_num = 3;

TYPE* new_fictitious_tf(CLASS_TABLE_CELL *ctc)
{
  return fictitious_tf(ctc, next_fictitious_num++);
}


/****************************************************************
 * 			FICTITIOUS_WRAP_TF			*
 ****************************************************************
 * Return a new fictitious wrapped species or family with ctc	*
 * entry ctc and number num.					*
 ****************************************************************/

TYPE* fictitious_wrap_tf(CLASS_TABLE_CELL *ctc, int num)
{
  register int code = (ctc == NULL) ? GENUS_ID_CODE : ctc->code;
  register TYPE_TAG_TYPE 
               kind = (code == GENUS_ID_CODE) 
			? FICTITIOUS_WRAP_TYPE_T 
			: FICTITIOUS_WRAP_FAM_T;
  TYPE* t = new_type(kind, NULL);
  t->ctc  = ctc;
  t->TNUM = num;
  return t;
}


/****************************************************************
 * 			NEW_FICTITIOUS_WRAP_TF			*
 ****************************************************************
 * Return a new fictitious secondary species or family with ctc	*
 * entry ctc and new number.					*
 ****************************************************************/

TYPE* new_fictitious_wrap_tf(CLASS_TABLE_CELL *ctc)
{
  return fictitious_wrap_tf(ctc, next_fictitious_num++);
}


/****************************************************************
 * 			TF_OR_VAR_T				*
 ****************************************************************
 * If ctc is the class table entry for a species or family X,   *
 * then return X.  If it is the table entry for a genus		*
 * or community X, then return X`a, a new variable.		*
 ****************************************************************/

TYPE* tf_or_var_t(CLASS_TABLE_CELL *ctc)
{
  if(ctc == NULL) return var_t(NULL);
  else return copy_type(ctc->ty, 0);
}


/****************************************************************
 * 			PRIMARY_TF_OR_VAR_T			*
 ****************************************************************
 * If ctc is the class table entry for a species or family X,   *
 * then return X.  If it is the table entry for a genus		*
 * or community X, then return X`*a, a new variable.		*
 ****************************************************************/

TYPE* primary_tf_or_var_t(CLASS_TABLE_CELL *ctc)
{
  int code = ctc->code;
  if(code == GENUS_ID_CODE || code == COMM_ID_CODE) {
    return primary_var_t(ctc);
  }
  else return ctc->ty;
}


/****************************************************************
 *			 MAKE_LPAIR_T				*
 ****************************************************************
 * Return an lpair (label1, label2).				*
 ****************************************************************/

LPAIR make_lpair_t(int label1, int label2)
{
  LPAIR result;

  result.label1 = label1;
  result.label2 = label2;
  return result;
}


/********************************************************************
 *			 MAKE_ORDERED_LPAIR_T			    *
 ********************************************************************
 * Return an lpair (a,b) where a = min(l1, l2) and b = max(l1, l2). *
 ********************************************************************/

LPAIR make_ordered_lpair_t(int l1, int l2)
{
  return (l1 <= l2) ? make_lpair_t(l1, l2) : make_lpair_t(l2, l1);
}


/****************************************************************
 * 			INIT_TYPES				*
 ****************************************************************/

void init_types()
{
  fam_tkind_set =   (1L << FAM_T) 	
                  | (1L << WRAP_FAM_T)
		  | (1L << FAM_VAR_T)	
  		  | (1L << WRAP_FAM_VAR_T)
	          | (1L << PRIMARY_FAM_VAR_T)
		  | (1L << FICTITIOUS_FAM_T) 
		  | (1L << FICTITIOUS_WRAP_FAM_T);

  fam_type_tkind_set = (1L << FAM_T) 	
                     | (1L << WRAP_FAM_T)
		     | (1L << FICTITIOUS_FAM_T) 
		     | (1L << FICTITIOUS_WRAP_FAM_T);

  fam_var_tkind_set = (1L << FAM_VAR_T)	
  		    | (1L << WRAP_FAM_VAR_T)
	            | (1L << PRIMARY_FAM_VAR_T);

  wrap_tkind_set =  (1L << WRAP_TYPE_T)     
                  | (1L << WRAP_FAM_T)  
		  | (1L << FICTITIOUS_WRAP_TYPE_T)
		  | (1L << FICTITIOUS_WRAP_FAM_T)
                  | (1L << WRAP_TYPE_VAR_T) 
		  | (1L << WRAP_FAM_VAR_T);

  primary_tkind_set =   (1L << TYPE_ID_T) | (1L << FAM_T) 
		      | (1L << FICTITIOUS_TYPE_T) 
		      | (1L << FICTITIOUS_FAM_T)
		      | (1L << PRIMARY_TYPE_VAR_T) 
		      | (1L << PRIMARY_FAM_VAR_T);


  /*-------------------------------*
   * NULL_LP, NOCHECK_LP, NULL_RT. *
   *-------------------------------*/

  NULL_LP.label1    = NULL_LP.label2    = 0;
  NOCHECK_LP.label1 = NOCHECK_LP.label2 = -1;
  NULL_RT.type = NULL;
  NULL_RT.role = NULL;

  /*--------------------------*
   * any_type (needed early). *
   *--------------------------*/

  bmp_type(any_type = var_t(NULL));

  /*----------------------------------*
   * Pair and functions constructors. *
   *----------------------------------*/

  Pair_ctc     = standard_constr(PAIR_CODE, std_type_id[PAIR_TYPE_ID]);
  Function_ctc = standard_constr(FUN_CODE,  std_type_id[FUNCTION_TYPE_ID]);

  /*-----------------*
   * Standard types. *
   *-----------------*/

  std_types();  

  /*-------------------------------------*
   * Types for compiler and interpreter. *
   *-------------------------------------*/

  bmp_type(outfile_type = fam_mem_t(outfile_fam, char_type));
  bmp_type(string_type  = list_t(char_type));
  bmp_type(any_box      = box_t(any_type));

  /*---------------------*
   * Types for compiler. *
   *---------------------*/

# ifdef TRANSLATOR
  bmp_type(any_type2    = var_t(NULL));
  bmp_type(any_list     = list_t(any_type));
  bmp_type(any_pair     = pair_t(any_type, any_type2));
  bmp_type(any_pair_rev = pair_t(any_type2, any_type));
  bmp_type(any_EQ	= var_t(EQ_ctc));
  bmp_type(bad_type     = new_type(BAD_T, NULL_T));
  bad_type->ctc         = allocate_ctc();
  bad_type->ctc->code   = 0;

  bmp_type(exc_to_bool_type = function_t(exception_type, boolean_type));
  bmp_type(str_to_exc_type  = function_t(string_type, exception_type));
  bmp_type(exc_to_str_type  = function_t(exception_type, string_type));

  abbrev_tm(std_type_id[STRING_TYPE_ID], 
		  TYPE_ABBREV_TOK, string_type, NULL, TRUE);

  bmp_type(cons_type    = function_t(pair_t(any_type, any_list), any_list));
  bmp_type(content_type = function_t(any_box, any_type));
  bmp_type(assign_type  = function_t(pair_t(any_box, any_type), hermit_type));
  bmp_type(idf_type     = function_t(any_type, any_type));
  bmp_type(forget_type  = function_t(any_type, any_type2));
  bmp_type(show_apply_type = function_t(pair_t(string_type, any_type), 
					string_type));
# endif

  /*--------------------------------------------*
   * Close the standard classes and build	*
   * the initial join table.			*
   *--------------------------------------------*/

  close_classes_p();
}
