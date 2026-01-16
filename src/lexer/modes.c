/***********************************************************************
 * File:   lexer/modes.c
 * Purpose: Functions for handling modes of declarations.
 * Author: Karl Abrahamson
 ***********************************************************************/

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

/**************************************************************************
 * This file contains functions that handle modes.  There are two groups. *
 *									  *
 *  (1) Those that handle the lexical aspects of modes.			  *
 *									  *
 *  (2) Those that handle the semantic aspects, telling what modes	  *
 *      are present in a mode data structure.				  *
 *									  *
 * A MODE_TYPE structure contains information about the mode.		  *
 *									  *
 *  patrule_mode	The mode (eager, lazy, unif, ...) for a pattern   *
 *			declaration.  It is an "or" of the following bits.*
 *									  *
 *			  1	eager					  *
 *			  2	lazy					  *
 *			  4	==					  *
 *			  8	==!					  *
 *			  16	==!?					  *
 *									  *
 *  patrule_tag		The tag for a pattern rule.			  *
 *									  *
 *  define_mode		This is an "or" of the MODE_MASK bits defined in  *
 *			modes.h.					  *
 *									  *
 *			In the case of a var expr, the low order three	  *
 *			bits are used to indicate the kind of variable    *
 *			that is requested (shared, nonshared, ...).	  *
 *			The bits that occupy those positions normally 	  *
 *			must not be used by var declarations.		  *
 *									  *
 *			The low order byte of define_mode is used as the  *
 *			mode for the machine: it is generated into the	  *
 *			byte code.					  *
 *									  *
 *  noexpects		This is a list of the names that follow option	  *
 *			noExpect.					  *
 *									  *
 *  visibleIn		This is a list of the names that follow option	  *
 *			visibleIn.					  *
 *									  *
 *  u.def_package_name	This is the name that follows option 'from' in 	  *
 *			the mode.					  *
 **************************************************************************/

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../lexer/lexer.h"
#include "../lexer/modes.h"
#include "../parser/parser.h"
#include "../utils/lists.h"
#include "../tables/tables.h"
#include "../generate/generate.h"
#include "../error/error.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif


/************************************************************************
 *			this_mode					*
 *			propagate_mode					*
 *			defopt_mode					*
 *			null_mode					*
 ************************************************************************
 * These are used to manage declaration modes.  null_mode is a constant *
 * mode of all 0's.  See parser.y under dclMode for a description of	*
 * how these variables are used.					*
 ************************************************************************/

MODE_TYPE	null_mode;
MODE_TYPE	this_mode;
MODE_TYPE	propagate_mode;
MODE_TYPE	defopt_mode;


/****************************************************************
 *			def_opt_table				*
 ****************************************************************
 * This table gives translations between mode names and mode	*
 * numbers.  The last entry must have a null name, to signal	*
 * the end of the table.					*
 *								*
 * This table must be in the same order as the modes, since it  *
 * is also used to get the mode names by their index.		*
 ****************************************************************/

struct def_opt_struct {
  char *str;
  LONG val;
};

PRIVATE struct def_opt_struct FARR def_opt_table[] =
{{"force",	FORCE_MODE_MASK 	},
 {"primitive",  PRIMITIVE_MODE_MASK	},
 {"copy", 	COPY_MODE_MASK      	},
 {"override", 	OVERRIDES_MODE_MASK 	},
 {"default", 	DEFAULT_MODE_MASK 	},
 {"underride", 	UNDERRIDES_MODE_MASK | DEFAULT_MODE_MASK},
 {"irregular", 	IRREGULAR_MODE_MASK 	},
 {"",		0			},
 {"assume", 	ASSUME_MODE_MASK 	},
 {"noAuto",	NOAUTO_MODE_MASK	},
 {"noEqual", 	NOEQUAL_MODE_MASK   	},
 {"ahead",	AHEAD_MODE_MASK     	},
 {"nodescription",  NODESCRIP_MODE_MASK },
 {"dangerous",  DANGEROUS_MODE_MASK  	},
 {"noDollar", 	NODOLLAR_MODE_MASK  	},
 {"incomplete", INCOMPLETE_MODE_MASK	},
 {"noExport",   NO_EXPORT_MODE_MASK  	},
 {"noExpect",   NO_EXPECT_MODE_MASK 	},
 {"private",    PRIVATE_MODE_MASK   	},
 {"strong",     STRONG_MODE_MASK    	},
 {"imported",   IMPORTED_MODE_MASK  	},
 {"noExpect",   PARTIAL_NO_EXPECT_MODE_MASK},
 {"hide",	HIDE_MODE_MASK      	},
 {"pattern",	PATTERN_MODE_MASK      	},
 {"missing",	MISSING_MODE_MASK      	},
 {"ranked",     RANKED_MODE_MASK     	},
 {"transparent",TRANSPARENT_MODE_MASK	},
 {"knownSafe",  SAFE_MODE_MASK		},
 {"partial",    PARTIAL_MODE_MASK    	},
 {"noPull",	NOPULL_MODE_MASK     	},
 {"protected",	PROTECTED_MODE_MASK     },
 {"imperative", IMPERATIVE_MODE_MASK | NOEQUAL_MODE_MASK },
 {"noCast",     NOCAST_MODE_MASK 	},
 {NULL,         0			}
};


/************************************************************************
 *			varopt_table					*
 ************************************************************************
 * varopt table holds the options (modes) for var expressions and	*
 * declarations, along with their values.  These only include modes     *
 * such as shared and nonshared that tell the kind of variable.  Other  *
 * modes are handled differently.					*
 ************************************************************************/

PRIVATE struct {
  char *name;
  int val;
} varopt_table[] =
{
 {"shared", 1},
 {"nonshared", 2},
 {"unknown", 4},
 {"logic", 6}
};


/************************************************************************
 *			ADD_VAR_MODE					*
 ************************************************************************
 * Add the var-expr mode given by id to mode MODE.			*
 ************************************************************************/

void add_var_mode(MODE_TYPE *mode, char *id)
{
  int k;
  LONG    new_define_mode = mode->define_mode;
  Boolean found           = FALSE;

  for(k = 0; varopt_table[k].name != NULL; k++) {
    if(strcmp(id, varopt_table[k].name) == 0) {
      LONG lowres = new_define_mode & 7;
      LONG a_mode = varopt_table[k].val;
      if((lowres & 1) && (a_mode & 2) ||
         (lowres & 2) && (a_mode & 1)) {
	semantic_error(DUPLICATE_MODES_ERR, 0);
      }
      else {
	new_define_mode |= a_mode;
	found = TRUE;
	break;
      }
    }
  }

  if(!found) {
    handle_simple_mode(mode, id, FALSE);
  }
  else {
    mode->define_mode = new_define_mode;
  }
}


/************************************************************************
 *			ADD_VAR_LIST_MODE				*
 ************************************************************************
 * Add the var-expr mode given by id{ids} to mode MODE.  Here, id	*
 * should be visibleIn, or the mode is bad.				*
 ************************************************************************/

void add_var_list_mode(MODE_TYPE *mode, char *id, STR_LIST *ids)
{
  if(strcmp(id, "visibleIn") != 0) {
    syntax_error1(BAD_MODE_ERR, id, 0);
  }
  else {
    SET_LIST(mode->visibleIn, str_list_union(mode->visibleIn, ids));
  }
}


/****************************************************************
 *			INSTALL_PATRULE_OPT			*
 ****************************************************************
 * install_patrule_opt(opt) installs option opt into		*
 * defopt_mode.patrule_mode.					*
 ****************************************************************/

PRIVATE Boolean install_patrule_opt(char *opt)
{
  if(!pat_rule_opts_ok) return FALSE;

  if(strcmp(opt, "eager") == 0) defopt_mode.patrule_mode |= 1;
  else if(strcmp(opt, "lazy") == 0) defopt_mode.patrule_mode |= 2;
  else if(strcmp(opt, "==") == 0) defopt_mode.patrule_mode |= 4;
  else if(strcmp(opt, "==!") == 0) defopt_mode.patrule_mode |= 8;
  else if(strcmp(opt, "==!?") == 0) defopt_mode.patrule_mode |= 16;
  else if(strcmp(opt, "special") == 0) {
    if(!compiling_standard_asi && 
       current_package_name != standard_package_name) {
      return FALSE;
    }
    else {
      open_pat_dcl = TRUE; 
      return TRUE;
    }
  }
  else return FALSE;

  return TRUE;
}


/****************************************************************
 * 			CHECK_MODE				*
 ****************************************************************
 * Check whether mode is consistent.				*
 ****************************************************************/

PRIVATE void check_mode(MODE_TYPE *mode)
{
  LONG defmode = mode->define_mode;
  if((defmode & (UNDERRIDES_MODE_MASK | OVERRIDES_MODE_MASK)) ==
     (UNDERRIDES_MODE_MASK | OVERRIDES_MODE_MASK)) {
    semantic_error(UNDER_OVER_ERR, 0);
  }
  if((defmode & (PRIVATE_MODE_MASK | PROTECTED_MODE_MASK)) ==
     (PRIVATE_MODE_MASK | PROTECTED_MODE_MASK)) {
    semantic_error(PRIV_PROT_ERR, 0);
  }
}


/****************************************************************
 *			HANDLE_SIMPLE_MODE			*
 ****************************************************************
 * The parser has just read a mode such as override or private. *
 * Modify mode appropriately.	Parameter modename is		*
 * the mode that was read.					*
 *								*
 * If lowok is true, then any mode can be added.  If lowok is   *
 * false, then refuse to add a mode whose number is smaller	*
 * than 3.							*
 ****************************************************************/

void handle_simple_mode(MODE_TYPE *mode, char *modename, Boolean lowok)
{
  struct def_opt_struct *p;
  LONG mask;

  if(!install_patrule_opt(modename)) {
    mask = 0;

    /*----------------------------------------------*
     * First search for this mode in def_opt_table. *
     *----------------------------------------------*/

    for(p = def_opt_table; p->str != NULL; p++) {
      if(strcmp(modename, p->str) == 0) {
	mask = p->val;
	break;
      }
    }

    /*----------------------------------------------------------*
     * If not found in def_opt_table, check for the special	*
     * modes "implicitPop" and "protected".			*
     *----------------------------------------------------------*/

    if(mask == 0) {
      if(strcmp(modename, "implicitPop") == 0) {
	generate_implicit_pop = TRUE;
      }
      else if(strcmp(modename, "protected") == 0) {
        mask = PROTECTED_MODE_MASK;
        if(!str_memq(current_package_name, mode->visibleIn)) {
	  SET_LIST(mode->visibleIn, 
		   str_cons(current_package_name, mode->visibleIn));
	}
      }
      else {
	syntax_error1(BAD_MODE_ERR, modename, 0);
      }
    }

    if(lowok || mask > 7) {
      mode->define_mode |= mask;
    }
    else {
      syntax_error1(BAD_MODE_ERR, modename, 0);
    }
  }
  check_mode(mode);
}


/****************************************************************
 *			BAD_MODE_NAME				*
 ****************************************************************
 * Return the name of the first mode that is in A but not in B. *
 * Each of A and B is a bit vector of modes.			*
 ****************************************************************/

PRIVATE char* bad_mode_name(LONG A, LONG B)
{
  int i;

  for(i = 0; i < 32; i++) {
    LONG mask = 1L << i;
    if((A & mask) && !(B & mask)) return def_opt_table[i].str;
  }
  return "(none)";
}


/****************************************************************
 *			CHECK_MODES				*
 ****************************************************************
 * Check that this_mode.define_mode does not have any bits set	*
 * that are not set in allowed_modes.				*
 ****************************************************************/

void check_modes(LONG allowed_modes, int line)
{
  if(this_mode.define_mode & ~allowed_modes) {
    syntax_error1(BAD_MODE_ERR, 
		  bad_mode_name(this_mode.define_mode, allowed_modes), 
		  line);
  }
}


/****************************************************************
 *			HAS_MODE				*
 ****************************************************************
 * Return 1 if mode MODE contains mode-bit MODE_BIT, and zero	*
 * if it does not contain that bit.				*
 ****************************************************************/

Boolean has_mode(const MODE_TYPE *mode, int mode_bit)
{
  if(mode == NULL) return 0;
  else return (mode->define_mode >> mode_bit) & 1L;
} 


/****************************************************************
 * 			GET_DEFINE_MODE				*
 ****************************************************************
 * Return the define_mode of mode, or 0 if mode is NULL.	*
 ****************************************************************/

LONG get_define_mode(MODE_TYPE *mode)
{
  return (mode == NULL) ? 0 : mode->define_mode;
}


/****************************************************************
 * 			GET_MODE_NOEXPECTS			*
 ****************************************************************
 * Return the noexpects field of mode, or NIL if mode is NULL.	*
 ****************************************************************/

STR_LIST* get_mode_noexpects(MODE_TYPE *mode)
{
  return (mode == NULL) ? NIL : mode->noexpects;
}


/****************************************************************
 * 			GET_MODE_VISIBLE_IN			*
 ****************************************************************
 * Return the visibleIn field of mode, or NIL if mode is NULL.	*
 ****************************************************************/

STR_LIST* get_mode_visible_in(MODE_TYPE *mode)
{
  return (mode == NULL) ? NIL : mode->visibleIn;
}


/****************************************************************
 * 			ADD_MODE				*
 ****************************************************************
 * Modify mode by adding bit mode_bit to it.  MODE must be	*
 * nonnull.							*
 *								*
 * If UNDERRIDES_MODE is added, then DEFAULT_MODE is implicitly *
 * added as well.						*
 *								*
 * If IMPERATIVE_MODE is added, then NOEQUAL_MODE is implicitly *
 * added as well.						*
 ****************************************************************/

void add_mode(MODE_TYPE *mode, int mode_bit)
{
  mode->define_mode |= 1L << mode_bit;
  if     (mode_bit == UNDERRIDES_MODE) mode->define_mode |= DEFAULT_MODE_MASK;
  else if(mode_bit == IMPERATIVE_MODE) mode->define_mode |= NOEQUAL_MODE_MASK;
  check_mode(mode);
} 


/****************************************************************
 * 			SIMPLE_MODE				*
 ****************************************************************
 * Return an new mode (with reference count set to 1) that	*
 * differs from null_mode only in that bit mode_bit is set.     *
 ****************************************************************/

MODE_TYPE* simple_mode(int mode_bit)
{
  MODE_TYPE* result = allocate_mode();
  add_mode(result, mode_bit);
  return result;
}


/****************************************************************
 * 			DO_COPY_MODE				*
 ****************************************************************
 * Copy the contents of mode B into mode A.			*
 ****************************************************************/

void do_copy_mode(MODE_TYPE *A, MODE_TYPE *B)
{
  drop_list(A->noexpects);
  drop_list(A->visibleIn);
  *A = *B;
  bump_list(A->noexpects);
  bump_list(A->visibleIn);
}


/****************************************************************
 * 			COPY_MODE				*
 ****************************************************************
 * Return a copy of MODE.  The copy has reference count 1	*
 * A copy of a null pointer is a non-null but empty mode.	*
 ****************************************************************/

MODE_TYPE* copy_mode(MODE_TYPE *mode)
{
  MODE_TYPE *result;

  result = allocate_mode();
  if(mode != NULL) {
    *result = *mode;
    bump_list(result->noexpects);
    bump_list(result->visibleIn);
    result->ref_cnt = 1;
  }
  return result;
}


/****************************************************************
 * 			MODE_EQUAL				*
 ****************************************************************
 * Return true just when modes A and B are the same.		*
 ****************************************************************/

Boolean mode_equal(const MODE_TYPE *A, const MODE_TYPE *B)
{
  if(A == NULL) {
    if(B == NULL) return TRUE;
    else return mode_equal(&null_mode, B);
  }

  else if(B == NULL) return mode_equal(A, &null_mode);

  else {
    return A->define_mode == B->define_mode &&
           A->patrule_mode == B->patrule_mode &&
           A->patrule_tag == B->patrule_tag &&
	   str_list_equal_sets(A->noexpects, B->noexpects) &&
	   str_list_equal_sets(A->visibleIn, B->visibleIn) &&
	   A->u.def_package_name == B->u.def_package_name;
  }
}


/****************************************************************
 * 			MODIFY_MODE				*
 ****************************************************************
 * Add all modes from mode B to mode A, giving precedence to B.	*
 * A must be nonnull.						*
 *								*
 * Parameter isvar is true for a var expr/dcl, and false for	*
 * other kinds of declarations.					*
 ****************************************************************/

void modify_mode(MODE_TYPE *A, MODE_TYPE *B, Boolean isvar)
{
  if(B == NULL) return;
  else {

    /*-------------------------*
     * Modify the define_mode. *
     *-------------------------*/

    LONG A_mask = A->define_mode;
    LONG B_mask = B->define_mode;
 
    if((A_mask & OVERRIDES_MODE_MASK) && 
       (B_mask & UNDERRIDES_MODE_MASK)) {
      A_mask &= ~((LONG)OVERRIDES_MODE_MASK);
    }
    if((A_mask & UNDERRIDES_MODE_MASK) && 
       (B_mask & OVERRIDES_MODE_MASK)) {
      A_mask &= ~((LONG)UNDERRIDES_MODE_MASK);
    }
    if(isvar && (B_mask & 7) != 0) {
      A_mask &= ~7L;
    }
    A->define_mode = A_mask | B_mask;

    /*-------------------------------------------*
     * Modify the noexpects and visibleIn lists. *
     *-------------------------------------------*/

    SET_LIST(A->visibleIn, str_list_union(A->visibleIn, B->visibleIn));
    SET_LIST(A->noexpects, str_list_union(A->noexpects, B->noexpects));

    /*-----------------------------------*
     * Modify the remaining fields of A. *
     *-----------------------------------*/

    if(B->u.def_package_name != NULL) {
      A->u.def_package_name = B->u.def_package_name;
    }
    if(B->patrule_tag != NULL) {
      A->patrule_tag = B->patrule_tag;
    }
    A->patrule_mode |= B->patrule_mode;
  }
}


/****************************************************************
 *			PRINT_MODE				*
 ****************************************************************
 * Print mode on the trace file, indented n spaces.		*
 ****************************************************************/

#ifdef DEBUG
void print_mode(MODE_TYPE *mode, int n)
{
  indent(n);
  if(mode == NULL) fprintf(TRACE_FILE, "mode : NULL\n");
  else {
    fprintf(TRACE_FILE, "mode(rc=%d): %lx (dpn = %s)\n", 
	    mode->ref_cnt, mode->define_mode, 
	    nonnull(mode->u.def_package_name));
    indent(n);
    fprintf(TRACE_FILE, "visible: ");
    print_str_list_nl(mode->visibleIn);
    fprintf(TRACE_FILE, "noexpects: ");
    print_str_list_nl(mode->noexpects);
  }
}
#endif

/****************************************************************
 *			INIT_MODES				*
 ****************************************************************
 * Set up the standard mode variables.				*
 ****************************************************************/

void init_modes(void)
{
  memset(&null_mode, 0, sizeof(MODE_TYPE));
  memset(&this_mode, 0, sizeof(MODE_TYPE));
  memset(&propagate_mode, 0, sizeof(MODE_TYPE));
  memset(&defopt_mode, 0, sizeof(MODE_TYPE));

  null_mode.ref_cnt =
    this_mode.ref_cnt =
    propagate_mode.ref_cnt =
    defopt_mode.ref_cnt = 10000;
}
