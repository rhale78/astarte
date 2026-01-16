/*****************************************************************
 * File:    dcls/deferdcl.c
 * Purpose: Support for deferred declarations
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
 * When inside an extension declaration, we must perform definitions 	*
 * that cannot actually be made until the extension is finished.     	*
 * Such declarations are placed in a chain of deferred declarations  	*
 * that are processed when the extension ends.  These functions      	*
 * record and process deferred declarations.			     	*
 *								  	*
 * Note: Type inference on deferred declarations is done without  	*
 * using local expectations as assumptions.  Unwrap warnings      	*
 * on deferred dcls are suppressed.				  	*
 ************************************************************************/


#include <string.h>
#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../alloc/allocate.h"
#include "../error/error.h"
#include "../tables/tables.h"
#include "../parser/parser.h"
#include "../lexer/modes.h"
#include "../unify/unify.h"
#include "../alloc/allocate.h"
#include "../classes/classes.h"
#include "../generate/generate.h"
#include "../generate/prim.h"
#include "../exprs/expr.h"
#include "../dcls/dcls.h"
#include "../infer/infer.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			deferred_dcls				*
 *			deferred_dcls_end			*
 ****************************************************************
 * deferred_dcls points to the node at the start of the chain	*
 * of deferred declarations.					*
 *								*
 * deferred_dcls_end points to the last cell in the chain of	*
 * deferred declarations.					*
 *								*
 * Both deferred_dcls and deferred_dcls_end are NULL when there *
 * are no deferred declarations.				*
 ****************************************************************/

PRIVATE DEFERRED_DCL_TYPE* deferred_dcls     = NULL;
PRIVATE DEFERRED_DCL_TYPE* deferred_dcls_end = NULL;

/****************************************************************
 *			INSTALL_DEFERRED_DCL			*
 ****************************************************************
 * Install d at the end of chain deferred_dcls.			*
 ****************************************************************/

PRIVATE void install_deferred_dcl(DEFERRED_DCL_TYPE *d)
{
  if(deferred_dcls == NULL) deferred_dcls = d;
  else deferred_dcls_end->next = d;
  d->next = NULL;
  deferred_dcls_end = d;
}


/****************************************************************
 *			DEFER_ATTACH_PROPERTY			*
 ****************************************************************
 * Place a deferred attach_property call in deferred_dcls.	*
 * This function is similar to attach_property.  See 		*
 * attach_property.						*
 ****************************************************************/

void defer_attach_property(char *prop, char *name, TYPE *t)
{
  DEFERRED_DCL_TYPE* d = allocate_deferred_dcl();

  d->tag = ATTACH_PROP_DEFER;
  d->fields.attach_prop_fields.name = name;
  d->fields.attach_prop_fields.prop = prop;
  bump_type(d->fields.attach_prop_fields.type = t);
  install_deferred_dcl(d);
}
  

/*****************************************************************
 *			DEFER_ISSUE_DCL_P			 *
 *****************************************************************
 * Place a deferred issue_dcl_p call in the deferred declaration *
 * chain.  This function is similar to issue_dcl_p.		 *
 * See issue_dcl_p.						 *
 *								 *
 * mode		is the mode of the declaration.			 *
 *		It is a safe pointer: it will not be kept	 *
 *		longer than the lifetime of this function call.	 *
 *****************************************************************/

Boolean defer_issue_dcl_p(EXPR *ex, int kind, MODE_TYPE *mode)
{
  DEFERRED_DCL_TYPE* d = allocate_deferred_dcl();

  d->tag = ISSUE_DCL_DEFER;
  bump_expr(d->fields.issue_dcl_fields.ex = ex);
  d->fields.issue_dcl_fields.kind = kind;
  d->fields.issue_dcl_fields.mode = copy_mode(mode);
  install_deferred_dcl(d);
  return TRUE;
}


/****************************************************************
 *			DEFER_ISSUE_MISSING_TM			*
 ****************************************************************
 * Place a deferred issue_missing_tm call in the deferred	*
 * dcl chain.  This function is similar to issue_missing.	*
 * See issue_missing_tm.					*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

void defer_issue_missing_tm(char *name, TYPE *type, MODE_TYPE *mode)
{
  DEFERRED_DCL_TYPE* d = allocate_deferred_dcl();

  d->tag = ISSUE_MISSING_DEFER;
  bump_type(d->fields.expect_dcl_fields.type = type);
  d->fields.expect_dcl_fields.role    = NULL;
  d->fields.expect_dcl_fields.name    = name;
  d->fields.expect_dcl_fields.mode    = copy_mode(mode);
  install_deferred_dcl(d);
}  
 

/*****************************************************************
 *			DEFER_ISSUE_DESCRIPTION_P		 *
 ****************************************************************
 * Place a deferred description in the deferred declaration 	*
 * chain.  This function is similar to issue_description_p.	*
 * See issue_description_p.					*
 *								*
 * mode		is the mode of the description declaration.	*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

void defer_issue_description_p(EXPR *ex, MODE_TYPE *mode)
{
  DEFERRED_DCL_TYPE* d = allocate_deferred_dcl();

  d->tag = DESCRIPTION_DEFER;
  bump_expr(d->fields.issue_dcl_fields.ex = ex);
  d->fields.issue_dcl_fields.mode = copy_mode(mode);
  install_deferred_dcl(d);
}


/****************************************************************
 *			DEFER_EXPECT_ENT_ID_P			*
 ****************************************************************
 * Place a deferred call to expect_ent_id_p in the deferred	*
 * declaration chain.  This function is similar to		*
 * expect_ent_id_p.						*
 * See expect_ent_id_p.						*
 *								*
 * mode		is the mode of the declaration.			*
 *		It is a safe pointer: it will not be kept	*
 *		longer than the lifetime of this function call.	*
 ****************************************************************/

void defer_expect_ent_id_p(char *name, TYPE *type, ROLE *role,
			   int context, MODE_TYPE *mode, int line)
{
  DEFERRED_DCL_TYPE *d;

# ifdef DEBUG
    if(trace_defs) {
      trace_t(41, name);
      trace_ty(type);
      tracenl();
    }
# endif

  d = allocate_deferred_dcl();
  d->tag = EXPECT_DCL_DEFER;
  bump_type(d->fields.expect_dcl_fields.type = type);
  bump_role(d->fields.expect_dcl_fields.role = role);
  d->fields.expect_dcl_fields.mode      = copy_mode(mode);
  d->fields.expect_dcl_fields.name      = name;
  d->fields.expect_dcl_fields.context   = context;
  d->fields.expect_dcl_fields.main_ctxt = main_context;
  d->fields.expect_dcl_fields.line      = line;
  install_deferred_dcl(d);
}


/******************************************************************
 *			HANDLE_DEFERRED_DCLS_P			  *
 ******************************************************************
 * Process the declarations represented by deferred_dcls.	  *
 *								  *
 * Note: Type inference on deferred declarations is done without  *
 * using local expectations as assumptions.  Unwrap warnings      *
 * on deferred dcls are suppressed.				  *
 *								  *
 * XREF: Called by parser.y at the end of an extension.		  *
 ******************************************************************/

void handle_deferred_dcls_p(void)
{
  DEFERRED_DCL_TYPE *r;
  Boolean old_no_suppress_warnings = err_flags.no_suppress_warnings;

# ifdef DEBUG
    if(trace_defs) trace_t(42);
# endif

  suppress_using_local_expectations  = TRUE;
  err_flags.no_suppress_warnings     = FALSE;

  for(r = deferred_dcls; r != NULL; r = r->next) {
    switch(r->tag) {
      case ISSUE_DCL_DEFER:
	issue_dcl_p(r->fields.issue_dcl_fields.ex,
		    r->fields.issue_dcl_fields.kind,
		    r->fields.issue_dcl_fields.mode);
	break;

      case ISSUE_MISSING_DEFER:
	issue_missing_tm(r->fields.expect_dcl_fields.name,
			 r->fields.expect_dcl_fields.type,
			 r->fields.expect_dcl_fields.mode);
	break;

      case EXPECT_DCL_DEFER:
	{LIST* l;
	 char*    name   = r->fields.expect_dcl_fields.name;
         MODE_TYPE* mode = r->fields.expect_dcl_fields.mode;
         TYPE*    ty     = r->fields.expect_dcl_fields.type;
         int   context   = r->fields.expect_dcl_fields.context;

	 bump_list(l = str_cons(name, NIL));
	 expect_ent_ids_p(l,
			  ty,
			  r->fields.expect_dcl_fields.role,
			  context, 
			  mode,
			  r->fields.expect_dcl_fields.line,
			  TRUE, NULL);
	 note_expectations_p(l,
			     ty,
			     context,
			     mode,
			     current_package_name, current_line_number);
	 drop_list(l);
	 break;
	}

#     ifdef NEVER
        case PAT_CONST_DCL_DEFER:
	declare_pat_const_tm(r->fields.expect_dcl_fields.name);
	break;
#     endif

      case ATTACH_PROP_DEFER:
	attach_property(r->fields.attach_prop_fields.prop,
			r->fields.attach_prop_fields.name,
			r->fields.attach_prop_fields.type);
	break;

      case DESCRIPTION_DEFER:
	{EXPR*      desc = r->fields.issue_dcl_fields.ex;
	 MODE_TYPE* mode = r->fields.issue_dcl_fields.mode;
	 if(!(desc->MAN_FORM)) {
	   issue_dcl_p(desc, MANUAL_E, mode);
	 }
	 else issue_description_p(desc, mode);
	 break;
        }
        
    } /* end switch */
  } /* end for */

  free_deferred_dcl(deferred_dcls);
  deferred_dcls = NULL;
  suppress_using_local_expectations = FALSE;
  err_flags.no_suppress_warnings = old_no_suppress_warnings;
}


