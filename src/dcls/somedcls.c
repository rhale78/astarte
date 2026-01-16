/*****************************************************************
 * File:    dcls/somedcls.c
 * Purpose: Support for some declarations.
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
 * This file handles assorted kinds of declarations.  Included are	*
 *									*
 *   exception 	 declarations						*
 *   description declarations						*
 *   assume 	 declarations						*
 *   operator 	 declarations						*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/rdwrt.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../parser/parser.h"
#include "../parser/tokens.h"
#include "../lexer/modes.h"
#include "../unify/unify.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../standard/stdfuns.h"
#include "../standard/stdids.h"
#include "../exprs/expr.h"
#include "../dcls/dcls.h"
#include "../evaluate/instruc.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../patmatch/patmatch.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/*==============================================================*
 *			EXCEPTION DECLARATIONS			*
 *==============================================================*/

/****************************************************************
 *			EXCEPTION_P				*
 ****************************************************************
 * Declare an exception, as defined by				*
 *								*
 *    Exception{mode} name(wt) descr.				*
 *								*
 * at given line.  						*
 *								*
 * Parameter trap is true if this exception is initially	*
 * trapped. 							*
 *								*
 * Parameter descr is the description string, or NULL if none   *
 * is present.							*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 *								*
 * XREF: Called in parser.y at an exception declaration.	*
 ****************************************************************/

void exception_p(char *name, Boolean trap, RTYPE wt, char *descr,
		 MODE_TYPE *mode)
{
  int   n      = 0;
  TYPE* domain = (wt.type == NULL) ? hermit_type : wt.type;

  /*----------------------------------------------------*
   * We need to use the correct name for the exception. *
   *----------------------------------------------------*/

  name = new_name(name, TRUE);

  /*-------------------------------------------------------------*
   * Declare the exception in the code, if appropriate. n is the *
   * global label where the exception is declared in the code.	 *
   *-------------------------------------------------------------*/

  if(gen_code) {
    n = generate_exception_g(name, domain, trap, descr);
  }

  /*-------------------------------------------------------------*
   * Declare the exception with the table manager. We can 	 *
   * treat it like a primitive exception at address n if we      *
   * have generated n, but otherwise should treat it as 	 *
   * nonprimitive.  Notice that gen_code is used to tell 	 *
   * declare_constructor_p whether to define this as a full	 *
   * primitive.							 *
   *-------------------------------------------------------------*/

  declare_constructor_p(name, name, domain, wt.role, exception_type, NULL,
			PRIM_EXC_WRAP, n, mode, 0, NIL, 
			trap, FALSE, gen_code);

  /*--------------------------------------*
   * Add the exception to the trap table. *
   *--------------------------------------*/

  exc_trap_tm(name, trap);
}


/*==============================================================*
 *			DESCRIPTION DECLARATIONS	       	*
 *==============================================================*/

/****************************************************************
 *			WRITE_ENTITY_DESCRIPTION		*
 ****************************************************************
 * Write description descr of name: id_type into the		*
 * description file.						*
 ****************************************************************/

PRIVATE void write_entity_description(char *name, TYPE *id_type, char *descr)
{
  if(index_file != NULL) {
    fprintf(index_file, "E%s\n", name);
  } 
  fprintf(description_file, "E%s\n", name);
  if(id_type != NULL) {
    fprintf(description_file, ": ");
    fprint_ty(description_file, id_type);
    fnl(description_file);
    fnl(description_file);
  }
  fprintf(description_file, "%s%c", descr, 0);
  wrote_description = TRUE;
}


/****************************************************************
 *			WRITE_CLASS_DESCRIPTION			*
 ****************************************************************
 * Write description descr of name into the description file.	*
 * 								*
 * ctc is the class table cell for this id.			*
 ****************************************************************/

PRIVATE void 
write_class_description(char *name, char *descr, CLASS_TABLE_CELL *ctc)
{
  char c;

  if(ctc == NULL) c = 'N';
  else {
    int code = ctc->code;
    c =  (code == TYPE_ID_CODE)    ? 'S' :
         (code == FAM_ID_CODE)     ? 'F' :
	 (code == GENUS_ID_CODE) ? 'G' :
	 (code == COMM_ID_CODE)    ? 'C' : 'N';
  }

  if(index_file != NULL) {
    fprintf(index_file, "%c%s\n", c, name);
  } 
  fprintf(description_file, "%c%s\n%s%c", c, name, descr, 0);
  wrote_description = TRUE;
}


/****************************************************************
 *			ISSUE_DESCRIPTION_P			*
 ****************************************************************
 * Issue description dcl this_dcl.				*
 *								*
 * If main_context = EXPORT_CX, write this description into     *
 * the description file.					*
 *								*
 * mode		is the mode of the description declaration.	*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 *								*
 * XREF:							*
 *   Called by parser.y at a description for a non-entity id.	*
 *								*
 *   Called by dcl.c:issue_dcl_p to handle descriptions of 	*
 *   entity ids, after type inference.				*
 *								*
 *   Called by tables/descrip.c for doing ahead descriptions.   *
 ****************************************************************/

int issue_description_p(EXPR *this_dcl, MODE_TYPE *mode)
{
  char *name, *descr;
  EXPR *id;
  GLOBAL_ID_CELL *gic;
  TYPE *id_type;
  Boolean should_write;
  
  if(local_error_occurred) return 0;
  if(is_hermit_expr(this_dcl)) return 1;    /* Ignore null descriptions. */

  /*-------------------------------------------------------------*
   * should_write is true if this description should be written  *
   * into the description file.					 *
   *-------------------------------------------------------------*/

  should_write = doing_export()
                  && description_file != NULL
                  && !has_mode(mode, IMPORTED_MODE);

  /*----------------------------------------*
   * Get the components of the declaration. *
   *----------------------------------------*/

  id      = skip_sames(this_dcl->E1);
  name    = id->STR;
  descr   = this_dcl->STR;

  /*----------------------------------------*
   * Case of a description of an entity id. *
   *----------------------------------------*/

  if(this_dcl->MAN_FORM == 0) {

    name    = new_name(name, TRUE);
    id_type = id->ty;
    gic     = get_gic_tm(name, FALSE);
    if(gic == NULL) return 0;

    /*----------------------------------------------------------*
     * Write the description into the description file, if we   *
     * are in the export part.  Suppress this if mode		*
     * includes IMPORTED_MODE. 					*
     *----------------------------------------------------------*/

    if(should_write) {
      write_entity_description(name, id_type, descr);
    }

    /*----------------------------------------------------------*
     * Install this description, checking that it is consistent *
     * with prior descriptions.					*
     *----------------------------------------------------------*/

     {STR_LIST *who_sees;
      bump_list(who_sees =  get_visible_in(mode, name));
      install_description_tm(descr, id_type, who_sees, gic, name,
			     this_dcl->LINE_NUM);
      drop_list(who_sees);
     }
  }

  /*--------------------------------------*
   * Case of a description of a class id. *
   *--------------------------------------*/

  else if(this_dcl->MAN_FORM == 1) {
    CLASS_TABLE_CELL* ctc = get_ctc_tm(name);

    if(ctc != NULL) {

      /*----------------------------------------------------------*
       * Write the description into the description file, if we   *
       * are in the export part.  Suppress this if mode		  *
       * includes IMPORTED_MODE.				  *
       *----------------------------------------------------------*/

      if(should_write) {
	write_class_description(name, descr, ctc);
      }

      /*-----------------------------------------------------------*
       * Install this description into the table, and check with   *
       * previous descriptions.					   *
       *-----------------------------------------------------------*/

      if(ctc->descr != NULL) {
        if(compare_descriptions(descr, ctc->descr)) {
	  if(strlen(descr) > strlen(ctc->descr)) {
	    ctc->descr = descr;
	    ctc->descrip_package = current_package_name;
	  }
	}
	else {
	  warn1(MAN_MISMATCH_ERR, descr, 0);
	  err_print(MISMATCH_DESCR_PACKAGE_ERR, ctc->descrip_package, 
		    current_package_name); 
	}
      }
    }
  }

  /*-------------------------------*
   * Case of a string description. *
   *-------------------------------*/

  else {

    /*----------------------------------------------------------*
     * Write the description into the description file, if we   *
     * are in the export part.					*
     *----------------------------------------------------------*/

    if(should_write) {
      if(index_file != NULL) {
	fprintf(index_file, "N%s\n", name);
      } 
      fprintf(description_file, "N%s\n%s%c", name, descr, 0);
      wrote_description = TRUE;
    }
  }

  return 1;
}


/****************************************************************
 *			ISSUE_EMBEDDED_DESCRIPTION		*
 ****************************************************************
 * If descr is not NULL, then issue it as a description of the  *
 * identifier defined in definition def.			*
 *								*
 * mode		is the mode of the description declaration.	*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

void issue_embedded_description(EXPR *def, char *descr, MODE_TYPE *mode)
{
  if(descr != NULL) {
    EXPR* id = skip_sames(def)->E1;
    EXPR* dcl = new_expr1(MANUAL_E, id, def->LINE_NUM);
    dcl->STR = descr;
    dcl->MAN_FORM = 0;
    bump_expr(dcl);

    issue_description_p(dcl, mode);
    drop_expr(dcl);
  }
}


/****************************************************************
 *			CREATE_DESCRIPTION_P			*
 ****************************************************************
 * Issue a description descr of id, at given line.		*
 * If ty is not NULL, then ty is the type of id.		*
 * The MAN_FORM is man_form.					*
 *								*
 * mode		is the mode of the description declaration.	*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

void create_description_p(char *id, char *descr, TYPE* ty, 
			  MODE_TYPE *mode, int man_form, int line)
{
  EXPR* man_dcl;
  EXPR* id_as_expr = id_expr(id, line);
  bump_expr(man_dcl = new_expr1(MANUAL_E, id_as_expr, line));
  man_dcl->STR = descr;
  man_dcl->MAN_FORM = man_form;
  if(ty == NULL) issue_description_p(man_dcl, mode);
  else {
    bump_type(id_as_expr->ty = ty);
    issue_dcl_p(man_dcl, MANUAL_E, mode);
  }
  drop_expr(man_dcl);
}


/*==============================================================*
 *			ASSUME  DECLARATIONS			*
 *==============================================================*/

/****************************************************************
 *			ASSUME_P				*
 ****************************************************************
 * Issue declaration						*
 *								*
 *   Assume{mode} x,y,...: rt.					*
 *								*
 * where l is list x,y,...					*
 *								*
 * mode		is the mode of the assume declaration.		*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 *								*
 * XREF: Called by parser.y at an assume declaration.		*
 ****************************************************************/

void assume_p(STR_LIST *l, RTYPE rt, MODE_TYPE *mode)
{
  STR_LIST *p;
  Boolean global;
  char *name;

  global = has_mode(mode, NO_EXPORT_MODE) == 0;
  bump_rtype(rt);
  bump_list(l);

  for (p = l; p != NIL; p = p->tail) {
    name = p->head.str;
    if(dcl_context) {
      assume_tm(name, rt.type, global);
      if(rt.role != NULL) assume_role_tm(name, rt.role, global);
    }
    else {
      push_local_assume_tm(name, rt);
    }
  }

  drop_rtype(rt);
  drop_list(l);
}


/*==============================================================*
 *			OPERATOR  DECLARATIONS			*
 *==============================================================*/

/****************************************************************
 *			OPERATOR_P				*
 ****************************************************************
 * Do one of the following declarations.			*
 *								*
 * If open is false and op != NULL: 				*
 *								*
 *   Operator{mode} s(op).					*
 *								*
 * If open is true and op != NULL:				*
 *								*
 *   Operator{mode} open s(op).					*
 *								*
 * If op == NULL:						*
 *								*
 *   Operator{mode} s.						*
 *								*
 * mode		is the mode of the operator declaration.	*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 *								*
 * XREF: Called by parser.y at an operator declaration.		*
 ****************************************************************/

void operator_p(char *s, char *op, Boolean open, MODE_TYPE *mode)
{
  Boolean x;

  int tok = (op == NULL) ? UNARY_OP : operator_code_tm(op, &x);

  if (tok == 0) {
    syntax_error1(NOT_OP_ERR, display_name(op), 0);
  }
  else {
    operator_tm(s, tok, open, mode);

#   ifdef DEBUG
      if(trace) operator_code_tm(s, &x);
#   endif
  }
} 



