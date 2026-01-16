/****************************************************************
 * File:    debug/prtgic.c
 * Purpose: Print information from the translator outer id table.
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

/************************************************************************
 * This file contains functions that print information from the		*
 * translator's global id table on TRACE_FILE.				*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../tables/tables.h"
#include "../exprs/expr.h"
#include "../classes/classes.h"
#include "../utils/lists.h"
#include "../utils/hash.h"

#ifdef DEBUG
# include "../debug/debug.h"

/****************************************************************
 *			PRINT_ROLE_CHAIN			*
 ****************************************************************
 * Print role chain rc, indented n spaces.			*
 ****************************************************************/

void print_role_chain(ROLE_CHAIN *rc, int n)
{
  ROLE_CHAIN *p;

  indent(n); trace_t(517);
  for(p = rc; p != NULL; p = p->next) {
    indent(n+1); trace_t(114, p->line_no);
    fprint_role(TRACE_FILE, p->role); tracenl();
    indent(n+1);
    trace_t(297); trace_ty(p->type); tracenl();
    indent(n+1);
    trace_t(70, nonnull(p->package_name), p->package_name);
    indent(n+1);
    trace_t(577);
    print_str_list_nl(p->visible_in);
  }
}


/****************************************************************
 *			PRINT_DESCRIP_CHAIN			*
 ****************************************************************
 * Print description chain dc, indented n spaces.		*
 ****************************************************************/

void print_descrip_chain(DESCRIP_CHAIN *dc, int n)
{
  DESCRIP_CHAIN *p;

  indent(n); trace_t(516);
  for(p = dc; p != NULL; p = p->next) {
     indent(n+1); trace_t(110);
     indent(n+1);
     trace_t(71, nonnull(p->descr));
     indent(n+1);
     trace_t(70, nonnull(p->package_name), p->package_name);
     indent(n+1);
     trace_t(577);
     print_str_list_nl(p->visible_in);
     indent(n+1);
     trace_t(297); trace_ty(p->type); tracenl();
   }
}


/****************************************************************
 *			PRINT_EXPECTATIONS			*
 ****************************************************************
 * Print expectation chain exp, indented n spaces.		*
 ****************************************************************/

void print_expectations(EXPECTATION *q, int n)
{
  EXPECTATION *p;

  for(p = q; p != NULL; p = p->next) {
    indent(n);
    trace_t(512, toint(p->old), toint(p->irregular), toint(p->line_no));
    indent(n+1);
    trace_t(70, nonnull(p->package_name), p->package_name);
    indent(n+1);
    trace_t(577); print_str_list_nl(p->visible_in);
    indent(n+1);
    trace_t(297); trace_ty(p->type); tracenl();
  }
}


/****************************************************************
 *			PRINT_PATFUN_EXPECTATIONS		*
 ****************************************************************
 * Print expand part  q, indented n spaces.			*
 ****************************************************************/

void print_patfun_expectations(PATFUN_EXPECTATION *q, int n)
{
  PATFUN_EXPECTATION *p;

  for(p = q; p != NULL; p = p->next) {
    indent(n);
    trace_t(518);
    indent(n+1);
    trace_t(70, nonnull(p->package_name), p->package_name);
    indent(n+1);
    trace_t(577);
    print_str_list_nl(p->visible_in);
    indent(n+1);
    trace_t(297); trace_ty(p->type); tracenl();
  }
}


/****************************************************************
 *			PRINT_ENTPART				*
 ****************************************************************
 * Print entity part p, indented n spaces.  Only the		*
 * information in part p is shown -- the next link is not	*
 * followed.							*
 ****************************************************************/

void print_entpart(ENTPART *p, int n)
{
  indent(n);
  trace_t(66, toint(p->primitive), toint(p->arg), toint(p->mode), 
	  toint(p->from_expect), toint(p->in_body), toint(p->trapped), 
	  toint(p->irregular));
  print_str_list_nl(p->selection_info);
  indent(n+1);
  trace_t(70, nonnull(p->package_name), p->package_name);
  indent(n+1);
  trace_t(576, nonnull(p->attributed_package_name));
  indent(n+1);
  trace_t(577);
  print_str_list_nl(p->visible_in);
  indent(n+1);
  trace_t(297); trace_ty(p->ty); tracenl();
}


/****************************************************************
 *			PRINT_ENTPART_CHAIN			*
 ****************************************************************
 * Print entity part chain q, indented n spaces.		*
 ****************************************************************/

void print_entpart_chain(ENTPART *q, int n)
{
  ENTPART *p;

  for(p = q; p != NULL; p = p->next) {
    print_entpart(p, n);
  }
}


/****************************************************************
 *			PRINT_EXPAND_PART			*
 ****************************************************************
 * Print expand part p, indented n spaces.  Do not follow	*
 * the next link.						*
 ****************************************************************/

void print_expand_part(EXPAND_PART *p, int n)
{
  indent(n);
  trace_t(69, toint(p->mode), toint(p->QWRAP_INFO));
    print_str_list_nl(p->selection_info);
  indent(n+1);
  trace_t(70, nonnull(p->package_name), p->package_name);
  indent(n+1);
  trace_t(577);
  print_str_list_nl(p->visible_in);
  indent(n+1);
  trace_t(297); trace_ty(p->ty); tracenl();
  print_expr(p->u.rule, n+1);
}


/****************************************************************
 *			PRINT_EXPAND_PART_CHAIN			*
 ****************************************************************
 * Print expand part chain q, indented n spaces.		*
 ****************************************************************/

void print_expand_part_chain(EXPAND_PART *q, int n)
{
  EXPAND_PART *p;

  for(p = q; p != NULL; p = p->next) {
    print_expand_part(p, n);
  }
}


/*****************************************************************
 *			PRINT_GIC				 *
 ***************************************************************** 
 * Print the contents of global id cell gic, indented n places.  *
 *****************************************************************/

void print_gic(GLOBAL_ID_CELL *g, int n)
{
  if(g == NULL) fprintf(TRACE_FILE,"NULL\n");
  else {
    indent(n);
    trace_t(347); trace_ty(g->container); tracenl();
    print_expectations(g->expectations, n);
    print_entpart_chain(g->entparts, n);
    if(g->expand_info != NULL) {
      EXPAND_PART* expand_rules = g->expand_info->expand_rules;
      EXPAND_PART* patfun_rules = g->expand_info->patfun_rules;
      PATFUN_EXPECTATION* patfun_exp = g->expand_info->patfun_expectations;

      if(expand_rules != NULL) {
	indent(n); trace_t(513);
        print_expand_part_chain(expand_rules, n);
      }
      if(patfun_rules != NULL) {
	indent(n); trace_t(514);
        print_expand_part_chain(patfun_rules, n);
      }
      if(patfun_exp != NULL) {
	indent(n); trace_t(515);
        print_patfun_expectations(patfun_exp, n);
      }
    }
    print_descrip_chain(g->descr_chain, n);
    print_role_chain(g->role_chain, n);
  }
}


/*******************************************************************
 *			PRINT_ENT_TABLE				   *
 *******************************************************************
 * print_ent_table() prints the contents of table global_id_table. *
 *******************************************************************/

PRIVATE void print_ent_cell(HASH2_CELLPTR h)
{
  char* name = h->key.str;
  trace_t(64, nonnull(name), name);
  print_gic(h->val.gic, 1);
}

/*------------------------------------------------------------*/

void print_ent_table(void)
{
  trace_t(65);
  scan_hash2(global_id_table, print_ent_cell);
}
 

#endif
