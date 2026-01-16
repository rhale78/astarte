/****************************************************************
 * File:    clstbl/stdtytbl.c
 * Purpose: Functions for creating standard species, etc.
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
 * This file contains functions for inserting standard types, etc. into *
 * the class table.							*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#include "../parser/tokens.h"
#include "../utils/hash.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../classes/classes.h"
#include "../standard/stdtypes.h"
#include "../standard/stdids.h"
#include "../tables/tables.h"
#include "../clstbl/stdtytbl.h"
#include "../clstbl/classtbl.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/****************************************************************
 * 			STANDARD_CONSTR				*
 ****************************************************************
 * Build a standard type constructor with given code and name.  *
 * The code should be one of TYPE_ID_CODE, FAM_ID_CODE, 	*
 * GENUS_ID_CODE or COMM_ID_CODE.				*
 *								*
 * This function also builds the constructors "pair" and 	*
 * "function" (codes PAIR_CODE and FUN_CODE). 			*
 * See classes/type.c.						*
 ****************************************************************/

CLASS_TABLE_CELL* standard_constr(int code, char *name)
{
  CLASS_TABLE_CELL *ctc;

  ctc                  = get_new_ctc_tm(name);
  ctc->code            = code;
  ctc->name            = name;
  ctcs[next_class_num] = ctc;
  ctc->num             = next_class_num++;
  ctc->var_num         = -1;
  ctc->ty              = NULL_T;
  return ctc;
}

/****************************************************************
 * 			STANDARD_TYPE				*
 ****************************************************************
 * Declare standard species std_type_id[sym], and put it in *t. *
 * Return a pointer to the class table entry for it.		*
 ****************************************************************/

CLASS_TABLE_CELL* standard_type(TYPE **t, int sym)
{
  return standard_tf(t, std_type_id[sym], NULL, 0);
}


/****************************************************************
 * 			STANDARD_FAM				*
 ****************************************************************
 * Declare standard family std_type_id[sym], with argument arg, *
 * as if declared by						*
 *    Species F(arg) = ...					*
 * and put it in *t.      					*
 *								*
 * Return a pointer to the class table entry for this family.	*
 ****************************************************************/

CLASS_TABLE_CELL* standard_fam(TYPE **t, int sym, TYPE *arg, Boolean opaque)
{
  return standard_tf(t, std_type_id[sym], arg, opaque);
}


/****************************************************************
 * 			STANDARD_TF				*
 ****************************************************************
 * Declare species or family name.  Set t to the species or	*
 * family installed.  Return the class table cell for the	*
 * installed species or family.  For a species, arg should be	*
 * NULL.  For a family, arg should be the domain.		*
 ****************************************************************/

CLASS_TABLE_CELL* standard_tf(TYPE **t, char *name, TYPE *arg, Boolean opaque)
{
  char *s;
  CLASS_TABLE_CELL *ctc;

  s              = stat_id_tb(name);
  bump_type(*t   = add_tf_tm(s, arg, FALSE, opaque, 0));
  (*t)->standard = 1;
  ctc            = (*t)->ctc;
  ctc->std_num   = std_type_num++;
  return ctc;
}


/****************************************************************
 * 			STANDARD_GENUS				*
 *			STANDARD_COMM				*
 ****************************************************************
 * Declare a standard genus or community.			*
 *								*
 *  std_type_id[sym]	is the genus or community name. 	*
 *								*
 *  kind 		is GENUS_ID_TOK for a genus and 	*
 *			COMM_ID_TOK for a community. 		*
 *			(This is only used in the shared	*
 *			function, standard_cg.)			*
 *								*
 *  default 		is the default type or family, 		*
 *								*
 *  extensible 		is the value for the extensible field   *
 * 			of the class table cell.  It is 0 for   *
 *			non-extensible, 1 for only internally	*
 *			extensible and 2 for explicitly 	*
 *			extensible.				*
 *								*
 *  opaque		is true if this is an opaque community. *
 *								*
 * If mem is not NULL, then mark this genus or 			*
 * community as nonempty, with standard member mem.		*
 *								*
 * Function standard_cg is similar, and contains the shared 	*
 * code to do this.						*
 ****************************************************************/

PRIVATE CLASS_TABLE_CELL *
standard_cg(char *s, TYPE *dfault, TYPE *mem, int extensible, int kind,
	    Boolean opaque)
{
  char *ss;
  CLASS_TABLE_CELL *ctc;

  ss  = stat_id_tb(s);
  ctc = add_class_tm(ss, kind, extensible, FALSE, opaque);
  bump_type(ctc->CTC_DEFAULT = dfault);
  ctc->std_num = std_var_num++;
  if(mem != NULL) {
    ctc->nonempty = 1;
    ctc->mem_num = mem->ctc->num;
  }
  return ctc;
}

/*-------------------------------------------------------------*/

CLASS_TABLE_CELL *
standard_genus(int sym, TYPE *dfault, TYPE *mem, int extensible)
{
  return standard_cg(std_type_id[sym], dfault, mem, extensible,
		     GENUS_ID_TOK, 0);
}

/*-------------------------------------------------------------*/

CLASS_TABLE_CELL *
standard_comm(int sym, TYPE *dfault, TYPE *mem, int extensible, Boolean opaque)
{
  return standard_cg(std_type_id[sym], dfault, mem, extensible, 
		     COMM_ID_TOK, opaque);
}


