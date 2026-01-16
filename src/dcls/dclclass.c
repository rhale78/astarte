/*****************************************************************
 * File:    dcls/dclclass.c
 * Purpose: Declare species, genera, etc.
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
 * This file manages the declaration of species, families, genera	*
 * and communities.  							*
 *									*
 * The tables that hold information about the things declared here	*
 * are kept and managed in clstbl/classtbl.c.				*
 *									*
 * Declaration of entities such as constructors and destructors that	*
 * are automatically created when a species or family is declared are	*
 * managed in dclcnstr.c.						*
 ************************************************************************/

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#include "../exprs/expr.h"
#include "../classes/classes.h"
#include "../standard/stdids.h"
#include "../standard/stdtypes.h"
#include "../dcls/dcls.h"
#include "../evaluate/instruc.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../error/error.h"
#include "../patmatch/patmatch.h"
#include "../unify/unify.h"
#include "../generate/prim.h"
#include "../generate/generate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/*****************************************************************
 *			MASSAGE_FOR_DEFN_T			 *
 *****************************************************************
 * A species or family definition is being given.  If it is a    *
 * species definition, then t is NULL and the declaration	 *
 * it looks like						 *
 *								 *
 *   Species X = L.						 *
 *								 *
 * where L is a list of class-union-cells that gives the right-  *
 * hand side of the definition (as described in parser.y under	 *
 * classUnion).  				 		 *
 *								 *
 * If this is a	family definition, then it is			 *
 *								 *
 *   Species F(t) = L.						 *
 *								 *
 * where L is a list of class-union-cells.  In this case, opaque *
 * is true if F is an opaque family.				 *
 *								 *
 * Replace null variables in t and all types that occur in L.    *
 *								 *
 * Issue an error if any of the non-irregular parts of L still   *
 * have unbound variables.  Also issue an error if any of the    *
 * variables occur in a left context in L, or in t, for a	 *
 * transparent family.		 				 *
 *								 *
 * Also issue an error if any of the parts of L have a tok	 *
 * value of anything but TYPE_ID_TOK or TYPE_LIST_TOK.		 *
 *								 *
 * Helper massage_help performs actions and tests on this_type.	 *
 * It is one of the representation types in list L.		 *
 *****************************************************************/

PRIVATE void 
massage_help(TYPE *this_type, HASH1_TABLE *tbl, int irr, Boolean opaque)
{
  replace_null_vars(&this_type);

  /*-----------------------------------------------------*
   * If not irregular then check for unbound variables.	 *
   *-----------------------------------------------------*/
    
  if(irr == 0) {
    TYPE* badvar = find_var_t(this_type, tbl);
    if(badvar != NULL) {
      semantic_error(BAD_VAR_ERR, 0);
      return;
    }
  }

  /*------------------------------------------------------------*
   * If not opaque, then do not allow a variable to occur	*
   * on the left-hand side of a function type in the 		*
   * representation type of the family.				*
   *								*
   * Note that if this is not a family definition and		*
   * there is any variable at all in this_type, then there	*
   * will have been an error in the preceding case, and		*
   * we will not get here.					*
   *------------------------------------------------------------*/

  if(!opaque && var_in_context(this_type, 0)) {
    semantic_error(VAR_IN_LEFT_CONTEXT_ERR, 0);
  }
}

/*-----------------------------------------------------*/

PRIVATE void massage_for_defn_t(TYPE *t, LIST *L, Boolean opaque)
{
  LIST *p;
  CLASS_UNION_CELL *cuc;

  HASH1_TABLE* tbl = NULL;

  /*---------------------------------------------------------------*
   * Replace nulls in t.  Put those variable in the binding table. *
   * Complain if there is a variable in a left context in t, when  *
   * this is a transparent family.				   *
   *---------------------------------------------------------------*/

  if(t != NULL) {
    replace_null_vars(&t);
    if(!opaque && var_in_context(t, 0)) {
      semantic_error(VAR_IN_LEFT_CONTEXT_ERR, 0);
    }
    put_vars_t(t, &tbl);
  }

  /*------------------------------------------------------------*
   * Replace nulls and box variables in types in L, and report	*
   * error if any unbound variables remain, or if there is	*
   * a variable in a left context in a transparent family	*
   * definition.						*
   *------------------------------------------------------------*/

  for(p = L; p != NIL; p = p->tail) {
    cuc = p->head.cuc;
    if(cuc->tok == TYPE_LIST_TOK) {
      TYPE_LIST *q;
      for(q = cuc->CUC_TYPES; q != NIL; q = q->tail) {
	massage_help(q->head.type, tbl, cuc->special, opaque);
      } 
    }
    else if(cuc->tok == TYPE_ID_TOK) {
      massage_help(cuc->CUC_TYPE, tbl, cuc->special, opaque);
    }
    else {
      semantic_error(BAD_REPRESENTATION_ERR, cuc->line);
    }
  }

  free_hash1(tbl);
}


/****************************************************************
 *			DO_CLASS_PREAMBLE			*
 ****************************************************************
 * Process the beginning of a class definition for class ID, 	*
 * with superclass SUPERCLASS.  Parameter ARG is the argument   *
 * type when the class is parameterized, or is NULL if it is    *
 * not parameterized.						*
 *								*
 * Declare ID as a new genus and put it beneath SUPERCLASS (or	*
 * beneath OBJECT when SUPERCLASS is NULL).  Also expect the	*
 * basic species of the class.					*
 ****************************************************************/

void do_class_preamble(char *id, TYPE *arg, char *superclass)
{
  CLASS_TABLE_CELL *this_class_ctc, *superclass_ctc, *basic_type_ctc;
  char *basic_type_name;
  int tok;
  TYPE *extend_arg;

  /*---------------------------*
   * Check out the superclass. *
   *---------------------------*/

  superclass_ctc = get_ctc_tm(superclass);
  if(superclass_ctc == NULL) {
    semantic_error1(BAD_SUPERCLASS_ERR, superclass, 0);
    return;
  }

  /*------------------------------------*
   * How should the extensions be done? *
   *------------------------------------*/

  if(arg == NULL) {
    tok        = GENUS_ID_TOK;
    extend_arg = NULL; 
  }
  else {
    tok        = COMM_ID_TOK;
    extend_arg = (superclass_ctc->code == COMM_ID_CODE) ? NULL : arg;
  }  

  /*--------------------------------------------*
   * Create the genus of the class, and put it	*
   * beneath the superclass genus.		*
   *--------------------------------------------*/

  this_class_ctc = declare_cg_p(id, tok, 2, &this_mode);
  extend1ctc_tm(this_class_ctc, extend_arg, superclass_ctc, 
		TRUE, &this_mode);

  /*--------------------------------------------*
   * Expect the basic species of the class, and *
   * put it beneath the genus of the class.	*
   *--------------------------------------------*/

  basic_type_name = new_concat_id(id, "!", FALSE);
  expect_tf_p(basic_type_name, arg, &this_mode, FALSE);
  basic_type_ctc = get_ctc_tm(basic_type_name);
  if(basic_type_ctc != NULL) {
    extend1ctc_tm(basic_type_ctc, NULL, this_class_ctc, TRUE, &this_mode);
  } 
}


/****************************************************************
 *		ADD_TYPES_TO_CLASS_TYPE				*
 ****************************************************************
 * Add the types in list L to type/role RT as products, and	*
 * return the result.  The members of L are added in reverse	*
 * order.  For example, if L contains types [A,B,C], in that	*
 * order, then the result is (C,B,A,RT.type).			*
 *								*
 * List L is a list of EXPR nodes, each holding a type and a	*
 * role that is to be added.					*
 ****************************************************************/

PRIVATE RTYPE 
add_types_to_class_type(RTYPE rt, EXPR_LIST *L)
{
  EXPR_LIST *p;
  RTYPE result = rt;

  for(p = L; p != NIL; p = p->tail) {
    EXPR* this_e    = p->head.expr;
    TYPE* this_t    = this_e->ty;
    char* this_name = this_e->STR;
    if(this_t == NULL) {
      semantic_error(NO_TYPE_IN_CLASS_COMPONENT, this_e->LINE_NUM);
      return result;
    }
    else {
      result.type = pair_t(this_t, result.type);
      result.role = pair_role(basic_role(this_name), result.role);
      bump_mode(result.role->mode = this_e->SAME_E_DCL_MODE);
    }
  }
  return result;
}


/****************************************************************
 *			DEFINE_BASIC_CLASS_CONSTRUCTOR		*
 ****************************************************************
 * Class C is being defined.  Suppose that it has constants	*
 * called a, b and c, and variables x, y and z.  Also, suppose  *
 * that the variables are initialized using expression I.	*
 * (Expression I will actually define x-c, y-c and z-c.)	*
 *								*
 * Issue definition						*
 *								*
 *  Let ;construct-C!(?my-a,?my-b,?my-c,?super) =		*
 *    Context Super super =>					*
 *        I							*
 *    %Context							*
 *    Value ;make-C!(my-a,my-b,my-c,my-x,my-y,my-z,super).	*
 *								*
 * where Super is the name of the superclass.			*
 *								*
 * Parameter basic_type_name must be the name of the basic type *
 * (C!), and parameter superclass is the name of the superclass.*
 ****************************************************************/

PRIVATE void
define_basic_class_constructor(char *basic_type_name, char *superclass,
			       int line)
{
  char* constr_name = attach_prefix_to_id(";construct-", basic_type_name, 1);
  char* basic_constr_name = attach_prefix_to_id(";make-", basic_type_name, 1);
  EXPR *pat, *arg, *funbody, *body, *def, *initializer;
  MODE_TYPE *mode;

  /*-------------------------------------------------------------------*
   * pat will become expression (?my-a,?my-b,?my-c,?super).	       *
   * arg will become expression (my-a,my-b,my-c,my-x,my-y,my-z,super). *
   *-------------------------------------------------------------------*/

  pat = new_pat_var("super", line);
  arg = pat->E1;
  add_ids(&arg, class_vars, line);
  add_ids_and_pats(&arg, &pat, class_consts, line);

  /*---------------------------------------------*
   * Build the initializer, inside two contexts. *
   *---------------------------------------------*/

  initializer = make_context_expr(superclass, pat->E1, initializer_for_class, 
				  line, FALSE);

  /*------------------------------------*
   * Build the definition and issue it. *
   *------------------------------------*/

  funbody = apply_expr(initializer, 
		       apply_expr(id_expr(basic_constr_name, line), arg, line),
		       line);
  body = new_expr2(FUNCTION_E, pat, funbody, line);
  bump_expr(def = new_expr2(LET_E, id_expr(constr_name, line), body, line));

  mode = simple_mode(PRIVATE_MODE);  /* ref cnt is 1. */
  defer_issue_dcl_p(def, LET_E, mode);
  drop_mode(mode);
  drop_expr(def);	       
}


/****************************************************************
 *			DECLARE_SELECTORS			*
 ****************************************************************
 * List L is a list of EXPR nodes.  Each EXPR node provides the *
 * following information.					*
 *								*
 *    STR	: the name of a thing (constant or variable)  	*
 * 		  in the class.					*
 *								*
 *    ty	: the type of the thing.  This is the nominal	*
 *		  type.  For example, a variable that holds a	*
 *		  natural number has type <:Natural:>.		*
 *								*
 *    SAME_E_DCL_MODE : the declartion mode for this thing.     *
 *		        (This contains information about 	*
 *			whether this is private, protected, ...)*
 * 								*
 * Declare a polymorphic selector for this thing.  If this 	*
 * thing is called X, has nominal type T and mode MODE, then    *
 * the selector is defined by					*
 *								*
 *   Let{underride,MODE} X = X _o_ retract.			*
 *								*
 * where retract is the parameter.  The types are as follows.	*
 *								*
 * Case where arg is NULL:					*
 *								*
 *   retract            : C`a -> B				*
 *   X (on r.h.s. of =) : B -> T				*
 *   X (on l.h.s. of =) : C`a -> T				*
 *								*
 * Case where arg is not NULL:					*
 *								*
 *   retract            : C`a(arg) -> B				*
 *   X (on r.h.s. of =) : B -> T				*
 *   X (on l.h.s. of =) : C`a(arg) -> T				*
 *								*
 * where B and C are parameters.				*
 ****************************************************************/

PRIVATE void 
declare_selectors(EXPR_LIST *L, char *retract, 
		  CLASS_TABLE_CELL *C, TYPE *arg, TYPE *B, int line)
{
  EXPR_LIST *p;

  for(p = L; p != NIL; p = p->tail) {
    TYPE* C_var, *this_type, *domain_type;
    EXPR* this_e    = p->head.expr;
    char* this_name = this_e->STR;

    MODE_TYPE* mode = copy_mode(this_e->SAME_E_DCL_MODE);  /* ref cnt is 1 */
    add_mode(mode, UNDERRIDES_MODE);
    
    bump_type(C_var = var_t(C));
    bump_type(domain_type = (arg == NULL) ? C_var : fam_mem_t(C_var, arg));
    bump_type(this_type = function_t(domain_type, this_e->ty));

    defer_expect_ent_id_p(this_name, this_type, NULL, EXPECT_ATT, mode, line);
    define_by_compose(this_name, this_name, retract, 
		      domain_type, B, this_e->ty, TRUE, mode, 
		      line);

    drop_mode(mode);
    drop_type(C_var);
    drop_type(domain_type);
    drop_type(this_type);
  }
}
			

/****************************************************************
 *			ADD_TO_CLASS_CONTEXT			*
 ****************************************************************
 * Bring each id that is in an EXPR node in list L into context *
 * cntxt, using a bring case.					*
 ****************************************************************/

PRIVATE void
add_to_class_context(EXPR_LIST *L, char *cntxt, int line)
{
  EXPR_LIST *p;

  for(p = L; p != NIL; p = p->tail) {
    add_simple_context(cntxt, p->head.expr->STR, line);
  }
}


/****************************************************************
 *			DO_CLASS_EXPECTATIONS			*
 ****************************************************************
 * Do the expectations (as deferred expectations) that are      *
 * described in list L.  List L contains EXPR nodes, as		*
 * created when collecting expectations in a class.		*
 *								*
 * C is the table entry for the abstraction of the class that   *
 * is being defined, and arg is the type parameter, or NULL if  *
 * this is an unparameterized class.				*
 ****************************************************************/

PRIVATE void 
do_class_expectations(EXPR_LIST *L, CLASS_TABLE_CELL *C, TYPE *arg, int line)
{
  EXPR_LIST *p;

  for(p = L; p != NIL; p = p->tail) {
    EXPR* this_e = p->head.expr;
    char* this_name = this_e->STR;
    TYPE* this_type, *C_var, *domain_type;
    ROLE* this_role;

    C_var = var_t(C);
    if(this_e->SAME_MODE == ANTICIPATE_ATT) {
      C_var = new_type(MARK_T, C_var);
    }
    domain_type = (arg == NULL) ? C_var : fam_mem_t(C_var, arg);
    bump_type(this_type = function_t(domain_type, this_e->ty));
    bump_role(this_role = fun_role(NULL, this_e->role));
    defer_expect_ent_id_p(this_name, this_type, this_role, 
			  this_e->SAME_MODE, this_e->SAME_E_DCL_MODE, line);
    drop_type(this_type);
    drop_role(this_role);
  }
}


/****************************************************************
 *			MAKE_CLASS_TYPE_CUC_LIST		*
 ****************************************************************
 * Put each thing that is part of the current class (as shown   *
 * in the lists class_vars, class_consts) into a class union    *
 * list, and return that list.  This list is used in defininig  *
 * the basic type of the class.					*
 *								*
 * cnstr is the name of the constructor for the basic type.	*
 *								*
 * super_name is the name of the basic type of the superclass   *
 * (Super!).  super_ctc is the class table entry for		*
 * super_name.							*
 *								*
 * arg is the type argument for a family. or is NULL for	*
 * an unparameterized class.					*
 *								*
 * Suppose that list class_vars holds types [A,B,C], and list   *
 * class_consts holds types [D,E,F].  The returned list has     * 
 * one of the following forms, shown the way it would be on the *
 * right-hand side of a species declaration.			*
 *								*
 * If arg == NULL or Super! is not a family:			*
 *    cnstr (F,E,D,C,B,A, retract~>Super!)			*
 *								*
 * If arg != NULL and Super! is a family:			*
 *    cnstr (F,E,D,C,B,A, retract~>Super!(arg))		*
 ****************************************************************/

PRIVATE LIST*
make_class_type_cuc_list(char *cnstr, char *super_name, 
			 CLASS_TABLE_CELL *super_ctc, TYPE *arg, int line)
{
  HEAD_TYPE h;
  RTLIST_PAIR rtl;
  RTYPE rt;          /* the representation type and role */

  if(super_ctc == NULL) {
    semantic_error1(NO_CLASS_SUPER_REP, super_name, line);
    return NIL;
  }
  
  rt.type = (arg == NULL || super_ctc->code == TYPE_ID_CODE) 
               ? super_ctc->ty 
               : fam_mem_t(super_ctc->ty, arg);
  rt.role = basic_role(std_id[RETRACT_ID]);
  rt      = add_types_to_class_type(rt, class_vars);
  rt      = add_types_to_class_type(rt, class_consts);

  rtl.types = type_cons(rt.type, NIL);
  rtl.roles = role_cons(rt.role, NIL);
  union_context = TYPE_DCL_CX;
  h.cuc     = get_discrim_p(cnstr, rtl, NIL, FALSE, line, 0);
  return general_cons(h, NIL, CUC_L);
}


/****************************************************************
 *			DECLARE_CLASS_P				*
 ****************************************************************
 * Perform a declaration					*
 *								*
 *   Class id arg isKindOf superclass				*
 *      ...							*
 *   %Class							*
 *								*
 * where arg is NULL if omitted, and globals used are:		*
 *								*
 *   class_expects is a list of EXPR nodes describing the	*
 *		   expectations and anticipations in the	*
 *		   class.					*
 *								*
 *   class_consts is a list of EXPR nodes describing the	*
 *		  constants in the class.			*
 *								*
 *   class_vars   is a list of EXPR nodes describing the 	*
 *		  variables in the class.			*
 *								*
 *   initializer_for_class is an expression that builds the	*
 *			   variables.				*
 *								*
 * and parameters are:						*
 *								*
 *   id		  is the name of the class being defined.	*
 *								*
 *   superclass	  is the superclass name.			*
 *								*
 *   line	  is the line number where the declaration is   *
 *		  made.						*
 ****************************************************************/

void declare_class_p(char *id, RTYPE arg, char *superclass, int line)
{
  /*------------------------------------------------------------*
   * Assume the superclass is called Super and that id is New.	*
   * Then each of the following variables is as shown.		*
   *------------------------------------------------------------*/

  char *basic_type_name;		/* "New!"  			*/
  char *retract_name;			/* "retract"			*/
  char *superclass_retract_name;	/* "retract"			*/
  char *cnstr_name;			/* ";make-New!"			*/
  char *superclass_basic_type_name;	/* "Super!"			*/
  TYPE *basic_type;			/* New!	or New!(arg)		*/
  TYPE *superclass_basic_type;		/* Super! or Super!(arg)	*/
  CLASS_TABLE_CELL *abstraction_ctc;	/* New (table entry) 		*/
  CLASS_TABLE_CELL *basic_type_ctc;	/* New! (table entry) 		*/
  CLASS_TABLE_CELL *superclass_basic_type_ctc;  /* Super! (table entry) */

  if(!local_error_occurred) {

    /*-------------------------------------------*
     * Get the genus or community of this class. *
     * Its name is given by id.			 *
     *-------------------------------------------*/

    id              = new_name(id, FALSE);
    abstraction_ctc = get_ctc_tm(id);
    if(abstraction_ctc == NULL) return;

    /*------------------------------------------*
     * Get the superclass characteristics.	*
     *-----------------------------------------*/

    superclass                 = new_name(superclass, FALSE);
    superclass_basic_type_name = new_concat_id(superclass, "!", FALSE);
    superclass_basic_type_ctc  = get_ctc_tm(superclass_basic_type_name);
    superclass_retract_name    = std_id[RETRACT_ID];

    if(superclass_basic_type_ctc == NULL) return;
    superclass_basic_type = superclass_basic_type_ctc->ty;
    if(arg.type != NULL && superclass_basic_type_ctc->code == FAM_ID_CODE) {
      superclass_basic_type = fam_mem_t(superclass_basic_type, arg.type);
    }
    bump_type(superclass_basic_type);

    /*---------------------------------------------*
     * Create the basic species.  Its name is id!. *
     * Note that it must have been expected	   *
     * in the class preamble.			   *
     *---------------------------------------------*/

    {LIST *cucs;
     TYPE *this_retract_type;
     MODE_TYPE *amode;

     basic_type_name = new_concat_id(id, "!", FALSE);
     basic_type_ctc  = get_ctc_tm(basic_type_name);
     if(basic_type_ctc == NULL) return;
     basic_type = basic_type_ctc->ty;
     if(arg.type != NULL) {
       basic_type = fam_mem_t(basic_type, arg.type);
     }
     bump_type(basic_type);

     /*----------------------------------------------*
      * Build the r.h.s. of the species declaration. *
      *----------------------------------------------*/

     cnstr_name = attach_prefix_to_id(";make-", basic_type_name, 1);
     bump_list(cucs  = 
	       make_class_type_cuc_list(cnstr_name, superclass_basic_type_name,
					superclass_basic_type_ctc, arg.type, 
					line));
     if(cucs == NIL) goto out;

     /*---------------------------------------------------------------*
      * The retract to the superclass will be defined while creating  *
      * the type.  Expect it now.				      *
      *---------------------------------------------------------------*/

     bump_type(this_retract_type = 
	       function_t(basic_type, superclass_basic_type));
     defer_expect_ent_id_p(superclass_retract_name, this_retract_type, NULL,
			   EXPECT_ATT, 0, line);

     /*---------------------*
      * Create the species. *
      *---------------------*/

     amode = allocate_mode();   /* ref cnt is 1. */
     add_mode(amode, NOEQUAL_MODE);
     add_mode(amode, NODOLLAR_MODE);
     bump_list(amode->noexpects = this_mode.noexpects);
     suppress_role_extras = TRUE;
     declare_tf_p(basic_type_name, arg, cucs, amode);
     suppress_role_extras = FALSE;
     drop_mode(amode);
     drop_list(cucs);
    }

    /*------------------------------------------*
     * Anticipate the retract to the basic 	*
     * species.  This must be deferred to the 	*
     * end of the extension.			*
     *------------------------------------------*/

    {TYPE *retract_domain, *retract_type;
     retract_name   = std_id[RETRACT_ID];
     retract_domain = new_type(MARK_T, var_t(abstraction_ctc));
     if(arg.type != NULL) {
       retract_domain = fam_mem_t(retract_domain, arg.type);
     }
     retract_type   = function_t(retract_domain, basic_type);
     bump_type(retract_type);
     defer_expect_ent_id_p(retract_name, retract_type, NULL, 
			ANTICIPATE_ATT, 0, line);
     drop_type(retract_type);
    }

    /*--------------------------------------------------*
     * Declare the retract as an identity function	*
     * on the basic species.  (Note that the		*
     * definition on secondary species will be made	*
     * along with the anticipation above.)		*
     *--------------------------------------------------*/

    {TYPE *retract_idf_type;
     bump_type(retract_idf_type = function_t(basic_type, basic_type));
     defer_expect_ent_id_p(retract_name, retract_idf_type, NULL, 
			   EXPECT_ATT, 0, line);
     define_by_cast_from_id(retract_name, retract_idf_type, 
			    std_id[IDF_ID], retract_idf_type,
			    0, TRUE, line);
    }

    /*-----------------------------------------------------------*
     * Declare the retract to the superclass.  The definition is *
     *								 *
     *   Let{underride} retract = retract _o_ retract.		 *
     *								 *
     * where the types of the three occurrences of retract (from *
     * left to right) are					 *
     *								 *
     * Case where arg.type == NULL:				 *
     *								 *
     *   1. retract: New`a -> Super!				 *
     *   2. retract: New!  -> Super!				 *
     *   3. retract: New`a -> New!				 *
     *								 *
     * Case where arg.type != NULL and Super! is not a family:	 *
     *								 *
     *   1. retract: New`a(arg) -> Super!			 *
     *   2. retract: New!(arg)  -> Super!			 *
     *   3. retract: New`a(arg)	-> New!(arg)			 *
     *								 *
     * Case where arg.type != NULL and Super! is a family:	 *
     *								 *
     *   1. retract: New`a(arg) -> Super!(arg)			 *
     *   2. retract: New!(arg)  -> Super!(arg)			 *
     *   3. retract: New`a(arg) -> New!(arg)			 *
     *								 *
     * This retract is polymorphic over the new class.  	 *
     *-----------------------------------------------------------*/

    {TYPE *domain_type, *super_retract_type;
     MODE_TYPE *mode;

     bump_type(domain_type = var_t(abstraction_ctc));
     if(arg.type != NULL) {
       SET_TYPE(domain_type, fam_mem_t(domain_type, arg.type));
     }
     bump_type(super_retract_type = 
	       function_t(domain_type, superclass_basic_type));
  
     mode = simple_mode(UNDERRIDES_MODE);  /* ref cnt is 1. */
     defer_expect_ent_id_p(superclass_retract_name, super_retract_type, NULL,
			   EXPECT_ATT, NULL, line);
     define_by_compose(superclass_retract_name, superclass_retract_name, 
		       retract_name, domain_type, basic_type,
		       superclass_basic_type, TRUE, mode, line);
     drop_mode(mode);
     drop_type(domain_type);
     drop_type(super_retract_type);
    }

    /*--------------------------------------------------*
     * Declare the selectors.  They are polymorphic	*
     * over the genus of this class, so they		*
     * use the retract.	 (The selectors on the basic    *
     * species were created above, via the roles.)	*
     *--------------------------------------------------*/

     declare_selectors(class_vars, retract_name, abstraction_ctc, 
		       arg.type, basic_type, line);
     declare_selectors(class_consts, retract_name, abstraction_ctc,
		       arg.type, basic_type, line);

    /*--------------------------------------------*
     * Create the context for this class.  This	  *
     * includes the constants, variables and	  *
     * expectations.				  *
     *--------------------------------------------*/

     add_context_tm(id, NULL, NULL);
     add_to_class_context(class_vars, id, line);
     add_to_class_context(class_consts, id, line);
     add_to_class_context(class_expects, id, line);
     do_context_inherit(id, superclass);

    /*------------------------------------------*
     * Create the basic constructor.		*
     *------------------------------------------*/

    define_basic_class_constructor(basic_type_name, superclass, line);

    /*------------------------------------------*
     * Issue the expectations.			*
     *------------------------------------------*/

    do_class_expectations(class_expects, abstraction_ctc, arg.type, line);
  }

 out:
   drop_type(basic_type);
   drop_type(superclass_basic_type);
}


/****************************************************************
 *			DECLARE_CG_P				*
 ****************************************************************
 * Issue declaration 						*
 *								*
 *    Genus{mode} s %Genus					*
 *								*
 * if which == GENUS_ID_TOK, and declaration			*
 *								*
 *    Community{mode} s %Community				*
 *								*
 * if which == COMM_ID_TOK.  					*
 *								*
 * extens is 0 if s is not extensible,				*
 *           2 if s is extensible. 				*
 *								*
 * The returned value is a pointer to the class table entry for *
 * the genus or community that was created, or NULL if creation *
 * failed.							*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 *								*
 * XREF: Called in parser.y when doing a genus or community	*
 * declaration.							*
 ****************************************************************/

CLASS_TABLE_CELL*
declare_cg_p(char *s, int which, int extens, MODE_TYPE *mode)
{
  /*-------------------------------------------------------------*
   * Declare the id under its modified name, possibly adding the *
   * package name.						 *
   *-------------------------------------------------------------*/

  s = new_name(s, FALSE);

  /*------------------------------------------------------------*
   * Check to see whether s is already in use to name entities. *
   *------------------------------------------------------------*/

  if(check_for_ent_id(id_tb0(s), 0)) return NULL;

  /*------------------------------------------------------------*
   * If there is already a class table entry for s, then s is 	*
   * previously defined. 					*
   *------------------------------------------------------------*/

  if(get_ctc_tm(s) != NULL) {
    semantic_error1(ID_DEFINED_ERR, display_name(s), 0);
    return NULL;
  }

  /*---------------------------------------*
   * Otherwise add s to the class table.   *
   *---------------------------------------*/

  else {
    return add_class_tm(s, which, extens, gen_code, 
			has_mode(mode, TRANSPARENT_MODE) == 0);
  }
}


/****************************************************************
 * 		DEFINE_RANK_CONSTRUCTORS			*
 ****************************************************************
 * Define the constructors for adding type t_new to RANKED, 	*
 * where t_new is represented by t_old.				*
 *								*
 * If enum is true, then t_new is a member of ENUMERATED.  	*
 * Otherwise, t_new is not a member of ENUMERATED.		*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

PRIVATE void 
define_rank_constructors(TYPE *t_new, TYPE *t_old, MODE_TYPE *mode, 
			 Boolean enumq)
{
  TYPE *t1, *t2;

  /*--------------*
   * Report rank. *
   *--------------*/

  bump_type(t1 = function_t(t_new, natural_type));
  report_dcl_p(std_id[RANK_ID], DEFINE_E, 0, t1, NULL);
  drop_type(t1);

  /*----------------*
   * Define unrank. *
   *----------------*/

  bump_type(t1 = function_t(natural_type, t_new));
  bump_type(t2 = function_t(natural_type, t_old));
  define_by_cast_from_id(std_id[UNRANK_ID], t1, std_id[UNRANK_ID], t2,
			 mode, TRUE, current_line_number);
  drop_type(t1);
  drop_type(t2);

  /*-----------------*
   * Define compare. *
   *-----------------*/

  bump_type(t1 = function_t(pair_t(t_new, t_new), 
			    comparison_type));
  bump_type(t2 = function_t(pair_t(t_old, t_old), comparison_type));
  define_by_cast_from_id(std_id[COMPARE_ID], t1, std_id[COMPARE_ID], t2,
			 mode, TRUE, current_line_number);
  drop_type(t1);
  drop_type(t2);

  /*------------------------------------------------------------*
   * Define _upto_, _downto_ (for RANKED) or just report them	*
   * (for ENUMERATED).						*
   *------------------------------------------------------------*/

  bump_type(t1 = function_t(pair_t(t_new, t_new), 
			    list_t(t_new)));
  if(!enumq) {
    bump_type(t2 = function_t(pair_t(t_old, t_old),
			      list_t(t_old)));
    define_by_cast_from_id(std_id[UPTO_ID], t1, std_id[UPTO_ID], t2,
			   mode, TRUE, current_line_number);
    define_by_cast_from_id(std_id[DOWNTO_ID], t1, std_id[DOWNTO_ID], t2,
			   mode, TRUE, current_line_number);
    drop_type(t2);
  }
  else {
    report_dcl_p(std_id[UPTO_ID], DEFINE_E, 0, t1, NULL);
    report_dcl_p(std_id[DOWNTO_ID], DEFINE_E, 0, t1, NULL);
  }
  drop_type(t1);

  /*----------------------------------------------------*
   * Report smallest and define largest for ENUMERATED. *
   *----------------------------------------------------*/

  if(enumq) {
    report_dcl_p(std_id[SMALLEST_ID], DEFINE_E, 0, t_new, NULL);
    define_by_cast_from_id(std_id[LARGEST_ID], t_new, 
			   std_id[LARGEST_ID], t_old,
			   mode, TRUE, current_line_number);
  }
}


/****************************************************************
 * 			GET_REP_TYPE				*
 ****************************************************************
 * Get type and role information from cell cuc.			*
 *								*
 * Get the representation type and the representation role,     *
 * and put them into *rt.  Set *is_curried true if the 		*
 * constructor in cuc is curried.				*
 ****************************************************************/

PRIVATE void 
get_rep_type(CLASS_UNION_CELL *cuc, RTYPE *rt, Boolean *is_curried)
{
  if(cuc->tok == TYPE_ID_TOK) {
    rt->type = cuc->CUC_TYPE;
    rt->role = cuc->CUC_ROLE;
    *is_curried = FALSE;
  }
  else {
    *rt = list_to_type_expr(cuc->CUC_TYPES, cuc->CUC_ROLES, 0);
    *is_curried = TRUE;
  }
}


/****************************************************************
 * 		DECLARE_REMAINING_FUNS				*
 ****************************************************************
 * This is shared code for declare_uniform_constructors and	*
 * declare_nonuniform_constructors.				*
 *								*
 * Define $, pull, copy, Move, Copy and new if not suppressed.  *
 ****************************************************************/

PRIVATE void 
declare_remaining_funs(LIST *L, TYPE *target_type, MODE_TYPE *mode)
{
  /*---------------------*
   * Declare $ and pull. *
   *---------------------*/

  if(!has_mode(mode, NODOLLAR_MODE)) {
    declare_dollar_p(L, target_type);
    if(!has_mode(mode, NOPULL_MODE) && 
       !has_mode(mode, NOEQUAL_MODE)) {
      declare_pull_p(L, target_type);
    }
  }

  /*-----------------------------------*
   * Declare copy, Copy, Move and new. *
   *-----------------------------------*/

  if(has_mode(mode, IMPERATIVE_MODE)) {
    declare_copy_p(L, target_type);
    if(check_imported("collect/boxes.ast", FALSE)) {
      declare_move_copy_p(L, target_type);
    }
  }

  /*---------------------------------------------------*
   * Declare ^^ and downcast for a transparent family. *
   *---------------------------------------------------*/

  if(!has_mode(mode,NOCAST_MODE) && TKIND(target_type) == FAM_MEM_T) {
    TYPE* fam = find_u(target_type->TY2);
    TYPE* arg = find_u(target_type->TY1);
    if(!fam->ctc->opaque) {
      declare_upcast_p(L, fam, arg);
      declare_downcast_p(L, fam, arg);
    }
  }
}


/****************************************************************
 * 		DECLARE_UNIFORM_CONSTRUCTORS			*
 ****************************************************************
 * Declare the constructors, etc. for a uniform species or 	*
 * family declaration of id.  (By uniform we mean that there is *
 * only one part on the right hand side.)  			*
 *								*
 * If arg_type is null, then the declaration has the form	*
 *								*
 *    Species{mode} id = c T.                                   *
 *                                                              *
 * where c and T are the constructor and species contained in   *
 * the first class-union-cell of list L.  (If the constructor   *
 * name is null, then no constructor name was given, and the 	*
 * default name should be used.)				*
 *								*
 * If arg_type is non-null, then the declaration has the form	*
 *								*
 *    Species{mode} id(arg_type) = c T.			        *
 *								*
 * If T is a member of RANKED or ENUMERATED, and the mode does  *
 * not suppress it, and id is a species, then add id to RANKED  *
 * or ENUMERATED as well, and define functions.			*
 *								*
 * Additional parameters are as follows.			*
 *                                                              *
 * id_val 	is the species or family being defined.	 	*
 *								*
 * target_type  is the target of the constructor (same as 	*
 * 		id_val for a species, and a family member for a *
 *		family).                                        *
 *								*
 * mode		is the mode of the species declaration.		*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 *								*
 ****************************************************************/

PRIVATE void
declare_uniform_constructors(char *id, TYPE *arg_type, LIST *L,
			     TYPE *id_val, TYPE *target_type,
			     MODE_TYPE *mode)
{
  Boolean is_curried;
  int prim;
  RTYPE rt;
  char *constr_name, *true_constr_name;

  CLASS_UNION_CELL* cell  = L->head.cuc;
  int is_irregular        = cell->special;

  MODE_TYPE *newmode = copy_mode(mode);  /* ref cnt is 1. */
  modify_mode(newmode, cell->mode, FALSE);

  /*------------------------------------------------------------*
   * Get the representation type and the representation role,   *
   * and put them into rt. Set is_curried true if this is a 	*
   * curried constructor.  
   *------------------------------------------------------------*/

  get_rep_type(cell, &rt, &is_curried);

  /*------------------------------------------------------------*
   * Get the name of the constructor.  If the constructor	*
   * is curried, add two copies of HIDE_CHAR to its front,      *
   * for the non-curried version of the constructor.		*
   *------------------------------------------------------------*/

  constr_name = cell->name;
  if(constr_name == NULL) {
    constr_name = constructor_name(id);
  }
  true_constr_name = constr_name = cell->name = new_name(constr_name, TRUE);
  if(is_curried) {
    constr_name = attach_prefix_to_id(HIDE_CHAR_STR HIDE_CHAR_STR, 
				      constr_name, 0);
  }

# ifdef GCTEST
    if(is_irregular < 0) die(21);
# endif

  /*------------------------------------------------------------*
   * Store representation type of this type in the class table. *
   *------------------------------------------------------------*/

  if(!is_irregular) {
    TYPE *t;
    bump_type(t = (arg_type == NULL) 
			? rt.type 
			: pair_t(arg_type, rt.type));
    bump_type(id_val->ctc->CTC_REP_TYPE = copy_type(t,0));
    drop_type(t);
  }

  /*-------------------------------------------------------*
   * Declare the constructor, destructor, pattern function *
   * and tester.					   *
   *-------------------------------------------------------*/

  prim = is_irregular ? PRIM_WRAP : PRIM_CAST;
  declare_constructor_p(constr_name, true_constr_name,
			rt.type, rt.role, target_type, 
			NULL, prim, 0, newmode, is_irregular, 
			cell->withs, 0, TRUE, gen_code);

  if(is_curried) {
    declare_curried_constructor_p(constr_name, true_constr_name,
				  cell->CUC_TYPES, cell->CUC_ROLES,
				  rt.type, rt.role,
				  target_type, NULL, 
				  newmode, is_irregular,
				  cell->withs, TRUE);
  }
  
  /*-------------------*
   * Declare equality. *
   *-------------------*/

  if(!has_mode(mode, NOEQUAL_MODE)) {
    if(!is_irregular) declare_equality_by_cast(target_type, rt.type);
    else declare_equality_by_cast(target_type, WrappedEQ_type);
  }

  /*---------------------------------------------*
   * Declare the record field accessors, if any. *
   *---------------------------------------------*/

  declare_fields_p(target_type, rt.type, rt, NIL, -1, is_irregular, mode);

  /*---------------------------------------------*
   * Declare $, pull, new, copy, Copy and Move.  *
   *---------------------------------------------*/

  declare_remaining_funs(L, target_type, mode);

  /*---------------------------------------------*
   * Add to RANKED or ENUMERATED if appropriate. *
   *---------------------------------------------*/

  if(has_mode(mode, RANKED_MODE) && 
     TKIND(rt.type) == TYPE_ID_T &&
     arg_type == NULL &&
     ancestor_tm(RANKED_ctc, rt.type->ctc)) {

    Boolean enumq;
    CLASS_TABLE_CELL *addto;
    if(ancestor_tm(ENUMERATED_ctc, rt.type->ctc)) {
      addto = ENUMERATED_ctc;
      enumq = TRUE;
    }
    else {
      addto = RANKED_ctc;
      enumq = FALSE;
    }
    extend1ctc_tm(target_type->ctc, NULL, addto, TRUE, mode);
    define_rank_constructors(target_type, rt.type, mode, enumq);

  }
  drop_mode(newmode);
}


/****************************************************************
 * 		DECLARE_NONUNIFORM_CONSTRUCTORS			*
 ****************************************************************
 * Declare the constructors, etc for a nonuniform species or 	*
 * family declaration of id.  If arg_type is null, then the     *
 * declaration has the form 					*
 *								*
 *    Species{mode} id = L.					*
 *								*
 * where L is a list of class-union-cells giving the things     *
 * that occur on the right-hand side of the declaration.  If    *
 * arg_type is non-null, then the declaration has the form	*
 *								*
 *    Species{mode} id(arg_type) = L.				*
 *								*
 * Additional parameters are as follows.			*
 *								*
 * id_val	is the species or family being defined.		*
 *								*
 * target_type	is the target of the constructor (same as	*
 * 		id_val for a species, and a family member for a *
 *		family).					*
 *								*
 * mode		is the mode of the species declaration.		*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

PRIVATE void
declare_nonuniform_constructors(char *id, TYPE *arg_type, LIST *L,
				TYPE *id_val, TYPE *target_type,
				MODE_TYPE *mode)
{
  LIST *p;
  char *constr_name, *true_constr_name;
  int i, enumq, enum_len, prim;
  Boolean is_curried;
  RTYPE rt;
  CLASS_UNION_CELL *cell;
  
  enumq = (arg_type == NULL);  /* Will be true at end of loop if
				  this is an enumerated type. */
  enum_len = 0;                /* Number of values in enumerated type. */

  for(p = L, i = 0; p != NIL; p = p->tail, i++) {
    cell = p->head.cuc;

#   ifdef GCTEST
      if(cell->special < 0) die(21);
#   endif

    get_rep_type(cell, &rt, &is_curried);

    /*------------------------------*
     * Update enumerated type info. *
     *------------------------------*/

    enum_len++;
    if(!is_hermit_type(rt.type)) enumq = FALSE;

    /*--------------------------------------------------*
     * If there is no constructor, then it is an error. *
     *--------------------------------------------------*/

    if(cell->name == NULL) {
      syntax_error1(CONSTR_REQD_ERR, (char *) (i+1), 0);
      cell->name = "none";
    }
    else {
      MODE_TYPE *newmode;

      /*---------------------------*
       * Get the constructor name. *
       *---------------------------*/

      constr_name = cell->name;
      true_constr_name = constr_name = cell->name = 
	new_name(constr_name, TRUE);
      if(is_curried) {
	constr_name = attach_prefix_to_id(HIDE_CHAR_STR HIDE_CHAR_STR, 
					  constr_name, 0);
      }

      /*-----------------------------------------------------------*
       * Declare the constructor, destructor, pattern function and *
       * tester. 						   *
       *-----------------------------------------------------------*/

      newmode = copy_mode(mode);  /* ref cnt is 1. */
      modify_mode(newmode, cell->mode, FALSE);
      prim = (cell->special) ? PRIM_DWRAP : PRIM_QWRAP;
      declare_constructor_p(constr_name, true_constr_name,
			    rt.type, rt.role, 
			    target_type, NULL, prim, i,
			    newmode, cell->special, cell->withs,
			    FALSE, TRUE, gen_code);

      if(is_curried) {
	declare_curried_constructor_p(constr_name, true_constr_name,
				      cell->CUC_TYPES, cell->CUC_ROLES,
				      rt.type, rt.role, 
				      target_type, NULL, newmode, 
				      cell->special, cell->withs, TRUE);
      }
      drop_mode(newmode);
      
      /*---------------------------------------------*
       * Declare the record field accessors, if any. *
       *---------------------------------------------*/

      declare_fields_p(target_type, rt.type, rt, NIL, i, cell->special, 
		       mode);
    }
  }

  /*------------------------------------------------------------*
   * Enumerated type: 						*
   *								*
   *      Declare the constructor, and add enumerated type to 	*
   *      ENUMERATED.  The representation type is Natural.	*
   *      An enumerated type already has functions rank, 	*
   *      compare, ==, _upto_ and _downto_ (inherited from	*
   *	  RANKED and ENUMERATED), so report them.  Declare	*
   *	  value largest, the largest thing in the type, and 	*
   *	  report smallest, the smallest thing in the type.	*
   *------------------------------------------------------------*/

  if(enumq) {
    TYPE *t, *target_type_pair;
    char *constr_id;

    /*--------------------------*
     * Declare the constructor. *
     *--------------------------*/

    constr_id = new_name(constructor_name(id), TRUE);
    prim = (enum_len < 256) ? PRIM_ENUM_CAST : PRIM_LONG_ENUM_CAST;
    declare_constructor_p(constr_id, constr_id, natural_type, NULL, 
			  target_type, NULL,
			  prim, enum_len, mode, 0, NIL,
			  0, TRUE, gen_code);

    /*------------------------------------------------------------*
     * Add to ENUMERATED, and set representation type to Natural. *
     *------------------------------------------------------------*/

    extend1ctc_tm(id_val->ctc, NULL,ENUMERATED_ctc, TRUE, mode);
    bump_type(id_val->ctc->CTC_REP_TYPE = natural_type);

    /*----------------------------------------*
     * Declare 'largest'.  Report 'smallest'. *
     *----------------------------------------*/

    report_dcl_p(std_id[SMALLEST_ID], DEFINE_E, 0, target_type, NULL);
    define_by_cast_from_nat(std_id[LARGEST_ID], target_type, enum_len - 1, 
			    NULL, TRUE, current_line_number);

    /*-----------------------*
     * Declare rank, unrank. *
     *-----------------------*/

    bump_type(t = function_t(target_type, natural_type));
    report_dcl_p(std_id[RANK_ID], DEFINE_E, NULL, t, NULL);
    drop_type(t);

    bump_type(t = function_t(natural_type, target_type));
    define_by_cast_from_id(std_id[UNRANK_ID], t, constr_id, t,
		           mode, TRUE, current_line_number);
    drop_type(t);

    /*--------------------------*
     * Declare upto and downto. *
     *--------------------------*/

    bump_type(t = function_t(pair_t(target_type,target_type), target_type));
    report_dcl_p(std_id[UPTO_ID], DEFINE_E, 0, t, NULL);
    report_dcl_p(std_id[DOWNTO_ID], DEFINE_E, 0, t, NULL);
    drop_type(t);

    /*---------------------------------------------------*
     * Report compare and ==.  They are already defined. *
     *---------------------------------------------------*/

    bump_type(target_type_pair = pair_t(target_type, target_type));
    bump_type(t = function_t(target_type_pair, boolean_type));
    report_dcl_p(std_id[EQ_SYM], DEFINE_E, 0, t, NULL);
    drop_type(t);

    bump_type(t = function_t(target_type_pair, comparison_type));
    report_dcl_p(std_id[COMPARE_ID], DEFINE_E, 0, t, NULL);
    drop_type(t);
    drop_type(target_type_pair);
  }

  /*----------------------------------------*
   * Non-enumerated type: declare equality. *
   *----------------------------------------*/

  else if(!has_mode(mode, NOEQUAL_MODE)) {
    declare_equality_by_qeq(target_type, L);
  }

  /*---------------------------------------------*
   * Declare $, pull, new, copy, Copy and Move.  *
   *---------------------------------------------*/

  declare_remaining_funs(L, target_type, mode);
}


/****************************************************************
 *			POSSIBLY_EXTEND	  			*
 ****************************************************************
 * Species or family t is being declared with the given mode.   *
 * Add t(arg) or t to the genus described by ctc, provided	*
 * ctc is not NULL.						*
 ****************************************************************/

PRIVATE void 
possibly_extend(TYPE *t, TYPE *arg, CLASS_TABLE_CELL *ctc, MODE_TYPE *mode)
{
  TYPE *this_type;

  if(ctc != NULL) {
    this_type = (arg == NULL)                    ? NULL     :
                has_mode(mode, TRANSPARENT_MODE) ? var_t(ctc)
		                                 : any_type; 
    extend1ctc_tm(t->ctc, this_type, ctc, TRUE, mode);
  }
}


/****************************************************************
 *			EXPECT_TF_P	  			*
 ****************************************************************
 * Expect type or family id.  arg is null for a type, and is 	*
 * the domain type for a family.				*
 *								*
 * MODE is the mode of the expectation.  It is a safe pointer:  *
 * it will not be kept longer than the lifetime of this		*
 * function call.						*
 *								*
 * If full is true, then add this type to EQ, BOXES and NEW     *
 * if appropriate and not suppressed.				*
 *								*
 * Expect $, pull and copy if not suppressed, regardless of the *
 * value of full.						*
 *								*
 * XREF: Called in parser.y when doing a species declaration	*
 * or when doing a species expectation.				*
 ****************************************************************/

void expect_tf_p(char *id, TYPE *arg, MODE_TYPE *mode, Boolean full)
{
  TYPE *t, *this_type;
  Boolean in_import;
  int report_kind;

  /*------------------------------------------------------------*
   * If id is a transparent family, then there must be no	*
   * variables on the left-hand side of a function type in	*
   * arg.							*
   *------------------------------------------------------------*/

  if(arg != NULL) {
    replace_null_vars(&arg);
    if(has_mode(mode, TRANSPARENT_MODE) && var_in_context(arg, 0)) {
      semantic_error(VAR_IN_LEFT_CONTEXT_ERR, 0);
    }
  }

  /*-----------------------------------------------------------*
   * Expect an id under its modified name (possibly adding the *
   * package name to the id.				       *
   *-----------------------------------------------------------*/

  id = new_name(id, FALSE);

  /*--------------------------------------------------------------*
   * Expects in imports are to be entered as if they are defined  *
   * here rather than just expected. Expects in other contexts	  *
   * are entered as expectations, to be fulfilled later.	  *
   *--------------------------------------------------------------*/

  in_import = (main_context == IMPORT_CX || main_context == INIT_CX);
  if(!in_import) {
    t = expect_tf_tm(id, arg, 
		     has_mode(mode, TRANSPARENT_MODE) == 0,
		     has_mode(mode, PARTIAL_MODE) != 0);
    if(t == NULL_T) {
      if(!local_error_occurred) {
	semantic_error1(ID_DEFINED_ERR, display_name(id), 0);
      }
      return;
    }
  }
  else {
    t = add_tf_tm(id, arg, FALSE, 
		  has_mode(mode, TRANSPARENT_MODE) == 0,
		  has_mode(mode, PARTIAL_MODE) != 0);
    if(t != NULL_T) t->ctc->expected = 2;
    else return;
  }

  /*---------------------------------------*
   * Report the wrapped species or family. *
   *---------------------------------------*/

  report_kind = (arg == NULL) ? TYPE_E : FAM_E;
  report_dcl_p(id, report_kind, mode, NULL, NULL);

  /*---------------------------------------------------*
   * If not suppressed, add this type or family to EQ. *
   *---------------------------------------------------*/

  if(full) {
    if(!has_mode(mode, NOEQUAL_MODE)) {
      possibly_extend(t, arg, EQ_ctc, mode);
    }

    if(has_mode(mode, IMPERATIVE_MODE) && 
	check_imported("collect/boxes.ast", FALSE)) {
      possibly_extend(t, arg, get_ctc_tm(std_type_id[BOXES_TYPE_ID]), mode);
      possibly_extend(t, arg, get_ctc_tm(std_type_id[NEW_TYPE_ID]), mode);
    }
  }

  /*--------------------------------------*
   * Expect $ and pull if not suppressed. *
   *--------------------------------------*/

  if(!has_mode(mode, NODOLLAR_MODE)) {
    TYPE *dollar_type, *pull_type;

    this_type = (arg == NULL) ? t : fam_mem_t(t, any_type);
    bump_type(dollar_type = function_t(this_type, string_type));
    bump_type(pull_type   = function_t(string_type, 
				       pair_t(this_type, string_type)));
    defer_expect_ent_id_p(std_id[DOLLAR_SYM], dollar_type, 
			  NULL, EXPECT_ATT, mode, current_line_number);
    defer_expect_ent_id_p(std_id[PULL_ID], pull_type, 
			  NULL, EXPECT_ATT, mode, current_line_number);
    drop_type(dollar_type);
    drop_type(pull_type);        
  }

  /*--------------------------------*
   * Expect copy if requested.	    *
   *--------------------------------*/

  if(has_mode(mode, IMPERATIVE_MODE)) {
    TYPE *cpy_ty;
 
    this_type = (arg == NULL) ? t : fam_mem_t(t, any_type);
    bump_type(cpy_ty = function_t(copyflavor_type, 
				  function_t(this_type, this_type)));
    defer_expect_ent_id_p(std_id[COPY_ID], cpy_ty, 
			  NULL, EXPECT_ATT, mode, current_line_number);
    drop_type(cpy_ty);
  }
}


/****************************************************************
 * 			DECLARE_TF_P				*
 ****************************************************************
 * Declare a species or family, according to 			*
 *								*
 *     species{mode} id = L		(when arg.type == NULL)	*
 * or 								*
 *     species{mode} id(arg) = L.	(when arg.type != NULL)	*
 *								*
 * L is a list of classUnionCells.  See parser.y (classUnion)   *
 * for what is in the classUnionCells.				*
 *								*
 * MODE is a safe pointer: it will not be kept longer than the  *
 * lifetime of this function call.				*
 *								*
 * XREF: Called in parser.y when declaring a species or family. *
 ****************************************************************/

PRIVATE LIST* cnstr_list(LIST *L)
{
  if(L == NIL) return NIL;
  return str_cons(new_name(L->head.cuc->name, TRUE), cnstr_list(L->tail));
}

/*--------------------------------------------------------------*/

void declare_tf_p(char *id, RTYPE arg, LIST *L, MODE_TYPE *mode)
{
  int kind;
  Boolean is_opaque,    /* 1 if the mode contains opaque, 0 if not. */
          is_partial,   /* 1 if the mode contains partial, 0 if not. */
          uniform;	/* True if there is only one part on rhs of dcl.  *
			 * (no |)					  */
  TYPE *id_val;

  /*---------------------------------------*
   * If L = NIL this is a non-declaration. *
   *---------------------------------------*/

  if(L == NIL) return;

  /*------------------------------------------------------------*
   * Declare an id under its modified name (possibly adding the *
   * package name to the id.)				        *
   *------------------------------------------------------------*/

  id = new_name(id, FALSE);

  /*-------------------------------------------*
   * Extract mode features and other features. *
   *-------------------------------------------*/

  is_opaque  = !has_mode(mode, TRANSPARENT_MODE);
  is_partial = has_mode(mode, PARTIAL_MODE);
  uniform    = (L->tail == NIL);
  kind       = (arg.type == NULL) ? TYPE_ID_TOK : FAM_ID_TOK;

  bump_rtype(arg);
  bump_list(L);

# ifdef DEBUG
    if(trace_classtbl > 1) {
      LIST *p;
      CLASS_UNION_CELL *cuc;
      trace_t(32, id);
      for(p = L; p != NIL; p = p->tail) {
        cuc = p->head.cuc;
	trace_t(33, toint(cuc->tok), nonnull(cuc->name), 
		toint(cuc->special), toint(cuc->line));
        if(cuc->tok == TYPE_ID_TOK) {
	  fprintf(TRACE_FILE, "    type = "); 
	  trace_ty(cuc->CUC_TYPE); 
	  tracenl();
        }
        else if(cuc->tok == TYPE_LIST_TOK) {
	  fprintf(TRACE_FILE, "    types = ");
          print_type_list(cuc->CUC_TYPES);
          tracenl();
	}
      }
    }
# endif

  /*---------------------------------------------------------------------*
   * Check for a type variable that is not known.  Replace null pointers *
   * by actual variables.						 *
   *---------------------------------------------------------------------*/

  massage_for_defn_t(arg.type, L, is_opaque);

  /*----------------------------------------------*
   * Install this species or family in the table. *
   *----------------------------------------------*/

  bump_type(id_val = add_tf_tm(id, arg.type, gen_code, is_opaque,
			       is_partial)); 
  if(id_val != NULL) {
    bump_list(id_val->ctc->constructors = cnstr_list(L));
   
    /*--------------------------------------------------------------------*
     * Put this species or family below EQ if noEqual option is not used. *
     *--------------------------------------------------------------------*/

    if(!has_mode(mode, NOEQUAL_MODE)) {
      TYPE* link_label = (arg.type == NULL) ? NULL :
	 		 (is_opaque)        ? WrappedANY_type
					    : WrappedEQ_type;
      extend1ctc_tm(id_val->ctc, link_label, EQ_ctc, 1, mode);
    }

    /*--------------------------------------------------------------------*
     * Now declare the constructors and destructors, and other functions. *
     *--------------------------------------------------------------------*/

    {TYPE *target_type;
     
     /*------------------------------------*
      * Get the target of the constructor. *
      *------------------------------------*/

     bump_type(target_type = 
	       ((arg.type == NULL) ? id_val : fam_mem_t(id_val, arg.type)));

     /*----------------------------------------------------------------*
      * Declare the related objects for a species or family with only  *
      * one part on the rhs of the declaration.  Also record if        *
      * partial.				 		       *
      *----------------------------------------------------------------*/

     if(uniform) {
       declare_uniform_constructors(id, arg.type, L, id_val, target_type,
				    mode);
     }

     /*----------------------------------------------------------------*
      * Declare constructors, etc., for a species or family with more  *
      * than one part on the rhs of its declaration. 		       *
      *----------------------------------------------------------------*/

     else {
       declare_nonuniform_constructors(id, arg.type, L, id_val, target_type,
				       mode);
     }

     drop_type(target_type);
   }
  }

  drop_list(L);
  drop_rtype(arg);
}
