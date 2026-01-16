/**************************************************************
 * File:    error/error.c
 * Purpose: Error reporting for compiler.
 * Author:  Karl Abrahamson
 ***************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************
 * This file manages error reporting for the compiler.  It contains	*
 * assorted functions for reporting errors and warnings, and stores 	*
 * related data.							*
 ************************************************************************/

#include <string.h>
#include <stdarg.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../misc/find.h"
#include "../alloc/allocate.h"
#include "../parser/tokens.h"
#include "../parser/parser.h"
#include "../error/error.h"
#include "../classes/classes.h"
#include "../exprs/expr.h"
#include "../exprs/prtexpr.h"
#include "../unify/unify.h"
#include "../utils/lists.h"
#include "../utils/hash.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../tables/tables.h"
#include "../clstbl/classtbl.h"
#include "../lexer/lexer.h"
#include "../standard/stdids.h"
#include "../dcls/dcls.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
extern char yytext[];


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 * ERR_FLAGS contains some flags that control error reporting.  *
 * The flags are described in error.h.				*
 ****************************************************************/

ERR_FLAGS_TYPE err_flags =
 {
  TRUE,   /* need_err_nl */
  TRUE,   /* novice */
  FALSE,  /* no_suppress_warnings */
  FALSE,  /* suppress_warnings */
  FALSE,  /* forever_suppress_warnings */
  FALSE,  /* suppress_global_shadow_warnings */
  FALSE,  /* forever_suppress_global_shadow_warnings */
  FALSE,  /* suppress_dangerous_default_warnings */
  FALSE,  /* forever_suppress_dangerous_default_warnings */
  FALSE,  /* suppress_pm_incomplete_warnings */
  FALSE,  /* forever_suppress_pm_incomplete_warnings */
  FALSE,  /* suppress_missing_warnings */
  FALSE,  /* forever_suppress_missings */
  TRUE,	  /* suppress_property_warnings */
  TRUE,	  /* forever_suppress_property_warnings */
  FALSE,  /* suppress_unused_warnings */
  FALSE,  /* forever_suppress_unused_warnings */
  FALSE,  /* did_unbound_tyvar_err */
  FALSE,  /* reported_non_assoc_error */
  FALSE,  /* suppress_indentation_errs */
  TRUE,   /* check_indentation */
  TRUE,   /* forever_check_indentation */
};


/****************************************************************
 *			error_occurred				*
 *			warning_occurred			*
 *			local_error_occurred			*
 *			error_reported				*
 ****************************************************************
 * ERROR_OCCURRED is true if a hard error has occurred anywhere *
 * in the program.						*
 *								*
 * WARNING_OCCURRED is true if a warning has occurred anywhere	*
 * in the program.						*
 *								*
 * LOCAL_ERROR_OCCURRED is true if a hard error has occurred	*
 * in the current declaration.					*
 *								*
 * ERROR_REPORTED is true if an error or warning message was	*
 * printed in the current declaration.				*
 ****************************************************************/

 Boolean error_occurred		= FALSE;
 Boolean warning_occurred	= FALSE;
 Boolean local_error_occurred	= FALSE;
 Boolean error_reported		= FALSE;


/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			sure_mismatch_reported_lines		*
 ****************************************************************
 * SURE_MISMATCH_REPORTED_LINES keeps track of			*
 * lines where sure species mismatches have been reported.	*
 * It is used to avoid multiple reports at one line.		*
 ****************************************************************/

PRIVATE LIST* sure_mismatch_reported_lines = NIL;

/****************************************************************
 *			role_lines				*
 ****************************************************************
 * Table ROLE_LINES tells all lines where a role error has been *
 * reported.  Only one role error will be reported per line.    *
 ****************************************************************/

PRIVATE HASH1_TABLE* role_lines = NULL;

/****************************************************************
 *			bad_compile_ids				*
 ****************************************************************
 * BAD_COMPILE_IDS is a list of identifiers whose definitions	*
 * had compile errors in them.  It is used so that a more	*
 * meaningful error message can be shown when an identifier is	*
 * encountered that is unknown because of a prior compilation	*
 * error.							*
 ****************************************************************/

PRIVATE STR_LIST* bad_compile_ids = NIL;

/****************************************************************
 *			reported_unknown_ids			*
 *			global_reported_unknown_ids		*
 ****************************************************************
 * REPORTED_UNKNOWN_IDS is a list of identifiers that have been *
 * reported unknown in the current declaration.  It is used to  *
 * avoid complaining about the same identifier several times.	*
 * It is set to NIL at the start of each declaration in the	*
 * parser.							*
 *								*
 * GLOBAL_REPORTED_UNKNOWN_IDS is a list of all identifiers     *
 * that have been reported unknown during the entire program.   *
 * It is used to avoid redundant information about unknown ids. *
 ****************************************************************/

PRIVATE STR_LIST* reported_unknown_ids = NIL;
PRIVATE STR_LIST* global_reported_unknown_ids = NIL;

/****************************************************************
 *			reported_semantic_errors		*
 ****************************************************************
 * For each n, calls to semantic_error(n,line) will only report *
 * error n once per declaration.  List REPORTED_SEMANTIC_ERRS   *
 * contains the errors that have already been reported in this  *
 * declaration.  It is set to NIL at the start of each		*
 * declaration in the parser.					*
 ****************************************************************/

PRIVATE INT_LIST* reported_semantic_errs = NIL;

/****************************************************************
 *			soft_err				*
 ****************************************************************
 * SOFT_ERR is set true as a (clumsy) way of telling err_head   *
 * not to set local_error_occurred at a soft error.  A soft     *
 * error is one where the compiler thinks that it might have    *
 * recovered.							*
 ****************************************************************/

PRIVATE Boolean soft_err = FALSE;

/****************************************************************
 *			error_strings				*
 *			have_read_error_strings			*
 ****************************************************************
 * ERROR_STRINGS points to an array of strings.			*
 * ERROR_STRINGS[i] is the text (a printf format) for error	*
 * number i.  Initially, error_strings is meaningless -- it     *
 * is only created (and the error message file read in) when    *
 * it is needed.						*
 *								*
 * HAVE_READ_ERROR_STRINGS is true after the error message file *
 * has been read into error_strings.				*
 ****************************************************************/

PRIVATE Boolean have_read_error_strings = FALSE;
char **error_strings;

/****************************************************************
 *			printed_unknown_id_message		*
 ****************************************************************
 * PRINTED_UNKNOWN_ID_MESSAGE is set true if a messages about   *
 * the possible causes of unknown identifiers has been printed  *
 * in the current listing.					*
 ****************************************************************/

PRIVATE Boolean printed_unknown_id_message = FALSE;


/****************************************************************
 *			ERR_BEGIN_DCL				*
 ****************************************************************
 * Initialize the error variables for the start of a		*
 * declaration.							*
 ****************************************************************/

void err_begin_dcl(void)
{
  local_error_occurred = FALSE;
  if(import_level == 0) {
    error_reported = FALSE;
  }
  SET_LIST(reported_unknown_ids, NIL);
  SET_LIST(reported_semantic_errs, NIL);
  err_flags.reported_non_assoc_error = FALSE;
  err_flags.did_unbound_tyvar_err = FALSE;
}


/****************************************************************
 *			ERR_RESET_FOR_DCL			*
 ****************************************************************
 * Put the error and warning handling indicators back to their  *
 * correct values for the start of a declaration.  The action	*
 * in this function can be suppressed, so that several		*
 * declarations are handled together, as in a team.  See	*
 * parser.y.							*
 ****************************************************************/

void err_reset_for_dcl(void)
{
  err_flags.suppress_warnings 		= 
    err_flags.forever_suppress_warnings;
  err_flags.suppress_missing_warnings 	=
    err_flags.forever_suppress_missings;
  err_flags.suppress_property_warnings 	=
    err_flags.forever_suppress_property_warnings;
  err_flags.suppress_unused_warnings 	=
    err_flags.forever_suppress_unused_warnings;
  err_flags.suppress_pm_incomplete_warnings =
    err_flags.forever_suppress_pm_incomplete_warnings;
  err_flags.check_indentation =
    err_flags.forever_check_indentation;
  ignore_trapped_exceptions 	= forever_ignore_trapped_exceptions;
  free_hash2(local_suppress_property_table);
  local_suppress_property_table = NULL;
}


/****************************************************************
 *			LOAD_ERROR_STRINGS			*
 ****************************************************************
 * Allocate and fill array error_strings from the error message *
 * file, provided that has not already been done.		*
 ****************************************************************/

void load_error_strings(void)
{
  if(LISTING_FILE != NULL) fflush(LISTING_FILE);

  if(!have_read_error_strings) {
    char* fname = err_flags.novice ? NOVICE_ERR_MSGS_FILE : ERR_MSGS_FILE;
    char* s = (char*) BAREMALLOC(strlen(MESSAGE_DIR) + strlen(fname) + 1);

    sprintf(s, "%s%s", MESSAGE_DIR, fname);
    init_err_msgs(&error_strings, s, LARGEST_ERR_NO + 1);
    have_read_error_strings = TRUE;
    FREE(s);
  }
}  


/****************************************************************
 *			ERR_PRINT_STR				*
 ****************************************************************
 * Do printf(ERR_FILE, S, ...).  (Actually, print the error	*
 * message on the listing file and the error file if they are   *
 * both active and are different.				*
 ****************************************************************/

void err_print_str(CONST char *s, ...)
{
  va_list args;

  va_start(args,s);
  vfprintf(ERR_FILE, s, args);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    vfprintf(LISTING_FILE, s, args);
  }
  va_end(args);
}


/****************************************************************
 *			ERR_PRINT				*
 ****************************************************************
 * Print error_strings[F], on the error file, with additional	*
 * parameters as needed by format string error_strings[F].	*
 * See err_print_str.						*
 ****************************************************************/

void err_print(int f, ...)
{
  va_list args;

  load_error_strings();
  va_start(args,f);
  vfprintf(ERR_FILE, error_strings[f], args);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    vfprintf(LISTING_FILE, error_strings[f], args);
  }
  va_end(args);
}


/****************************************************************
 *			ERR_SHRT_PR_EXPR			*
 ****************************************************************
 * Print expression E on the error file, in short format.	*
 ****************************************************************/

void err_shrt_pr_expr(EXPR *e)
{
  shrt_pr_expr(ERR_FILE, e);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    shrt_pr_expr(LISTING_FILE, e);
  }
}


/****************************************************************
 *			ERR_SHORT_PRINT_EXPR			*
 ****************************************************************
 * Print expression E on the error file in short format,	*
 * with its species attached.					*
 ****************************************************************/

void err_short_print_expr(EXPR *e)
{
  short_print_expr(ERR_FILE, e);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    short_print_expr(LISTING_FILE, e);
  }
}


/****************************************************************
 *			INDENT_LEN				*
 ****************************************************************
 * Return the amount to indent constraints on a line that 	*
 * starts with error_strings[K].  The amount to indent is one	*
 * greater than the length of this string.			*
 ****************************************************************/

PRIVATE int indent_len(int k)
{
  return strlen(error_strings[k]) + 1;
}


/****************************************************************
 *			ERR_PRINT_TY				*
 ****************************************************************
 * Print species t on the error file.				*
 ****************************************************************/

void err_print_ty(TYPE *t)
{
  if(!err_flags.novice) show_hermit_functions++;
  fprint_ty(ERR_FILE, t);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    fprint_ty(LISTING_FILE, t);
  }
  if(!err_flags.novice) show_hermit_functions--;
}


/****************************************************************
 *		ERR_PRINT_TY_WITH_CONSTRAINTS_INDENTED		*
 ****************************************************************
 * Print species T on the error file, with each constraint	*
 * on a separate line, indented N spaces.			*
 ****************************************************************/

void err_print_ty_with_constraints_indented(TYPE *t, int n)
{
  if(!err_flags.novice) show_hermit_functions++;
  fprint_ty_with_constraints_indented(ERR_FILE, t, n);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    fprint_ty_with_constraints_indented(LISTING_FILE, t, n);
  }
  if(!err_flags.novice) show_hermit_functions--;
}


/****************************************************************
 *			ERR_PRINT_TY1				*
 ****************************************************************
 * Similar to print_ty1, but for the error file.		*
 * This function is currently unused.				*
 ****************************************************************/

#ifdef NEVER
PRIVATE void err_print_ty1(TYPE *t, PRINT_TYPE_CONTROL *c)
{
  FILE* oldf = c->f;
  c->f = ERR_FILE;
  if(!err_flags.novice) show_hermit_functions++;
  print_ty1_with_constraints(t, c);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    c->f = LISTING_FILE;
    print_ty1_with_constraints(t, c);
  }
  c->f = oldf;
  if(!err_flags.novice) show_hermit_functions--;
}
#endif


/****************************************************************
 *	    ERR_PRINT_TY1_WITHOUT_CONSTRAINTS			*
 ****************************************************************
 * Similar to print_ty1, but for the error file.  Does not	*
 * show constraints.						*
 ****************************************************************/

PRIVATE void err_print_ty1_without_constraints(TYPE *t, PRINT_TYPE_CONTROL *c)
{
  FILE* oldf = c->f;
  c->f = ERR_FILE;
  if(!err_flags.novice) show_hermit_functions++;
  print_ty1_without_constraints(t, c);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    c->f = LISTING_FILE;
    print_ty1_without_constraints(t, c);
  }
  c->f = oldf;
  if(!err_flags.novice) show_hermit_functions--;
}


/****************************************************************
 *	ERR_PRINT_TY1_WITH_CONSTRAINTS_INDENTED			*
 ****************************************************************
 * Similar to print_ty1, but for the error file, and with	*
 * each constraint on a separate line, indented N spaces.	*
 ****************************************************************/

PRIVATE void 
err_print_ty1_with_constraints_indented(TYPE *t, PRINT_TYPE_CONTROL *c, int n)
{
  FILE* oldf = c->f;
  c->f = ERR_FILE;
  if(!err_flags.novice) show_hermit_functions++;
  print_ty1_with_constraints_indented(t, c, n);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    c->f = LISTING_FILE;
    print_ty1_with_constraints_indented(t, c, n);
  }
  c->f = oldf;
  if(!err_flags.novice) show_hermit_functions--;
}


/****************************************************************
 *			ERR_PRINT_ROLE				*
 ****************************************************************
 * Print role R on the error file.				*
 ****************************************************************/

void err_print_role(ROLE *r)
{
  fprint_role(ERR_FILE, r);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    fprint_role(LISTING_FILE, r);
  }
}


/****************************************************************
 *			ERR_PRINT_RT1				*
 ****************************************************************
 * Similar to print_rt1, but for the error file.		*
 ****************************************************************/

void err_print_rt1(TYPE *t, ROLE *r, PRINT_TYPE_CONTROL *c)
{
  FILE* oldf = c->f;
  c->f = ERR_FILE;
  print_rt1_with_constraints(t, r, c);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    c->f = LISTING_FILE;
    print_rt1_with_constraints(t, r, c);
  }
  c->f = oldf;
}


/****************************************************************
 *	     ERR_PRINT_RT1_WITH_CONSTRAINTS_INDENTED		*
 ****************************************************************
 * Similar to print_rt1, but for the error file, and with	*
 * each constraint on a separate line.				*
 ****************************************************************/

void err_print_rt1_with_constraints_indented
        (TYPE *t, ROLE *r, PRINT_TYPE_CONTROL *c, int n)
{
  FILE* oldf = c->f;
  c->f = ERR_FILE;
  print_rt1_with_constraints_indented(t, r, c, n);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    c->f = LISTING_FILE;
    print_rt1_with_constraints_indented(t, r, c, n);
  }
  c->f = oldf;
}


/****************************************************************
 *			ERR_PRINT_RT				*
 ****************************************************************
 * Similar to print_rt, but for the error file.			*
 ****************************************************************/

void err_print_rt(TYPE *t, ROLE *r)
{
  print_rt(ERR_FILE, t, r);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    print_rt(LISTING_FILE, t, r);
  }
}


/****************************************************************
 *		   ERR_PRINT_RT_WITH_CONSTRAINTS_INDENTED	*
 ****************************************************************
 * Similar to print_rt_with_constraints_indented, but for the	*
 * error file.							*
 ****************************************************************/

void err_print_rt_with_constraints_indented(TYPE *t, ROLE *r, int n)
{
  print_rt_with_constraints_indented(ERR_FILE, t, r, n);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    print_rt_with_constraints_indented(LISTING_FILE, t, r, n);
  }
}


/****************************************************************
 *			ERR_NL					*
 ****************************************************************
 * Print a newline on the error file.				*
 ****************************************************************/

void err_nl(void)
{
  fnl(ERR_FILE);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) fnl(LISTING_FILE);
  err_flags.need_err_nl = FALSE;
}


/****************************************************************
 *			ERR_NL_IF_NEEDED			*
 ****************************************************************
 * Print two newlines on the error file, provided some non-error*
 * text has been printed since the previous error print.	*
 ****************************************************************/

void err_nl_if_needed(void)
{
  if(err_flags.need_err_nl) {
    err_nl();
    err_nl();
  }
}


/****************************************************************
 *			ATTACH_TYPE				*
 ****************************************************************
 * Print the type of E, preceded by a colon, using		*
 * C to control the printing.					*
 ****************************************************************/

PRIVATE void attach_type(EXPR *e, PRINT_TYPE_CONTROL *c)
{
  if(e->ty != NULL_T) {
    err_print(COLON_ERR);
    err_print_rt1(e->ty, e->role, c);
  }
}


/****************************************************************
 *			ATTACH_ROLE				*
 ****************************************************************
 * Print the role of E, preceded by a colon.			*
 ****************************************************************/

PRIVATE void attach_role(EXPR *e)
{
  err_print(COLON_ERR);
  err_print_role(e->role);
}


/****************************************************************
 *			ERR_PRINT_LABELED_TYPE			*
 ****************************************************************
 * Do a print_labeled_type on the error file.			*
 ****************************************************************/

void err_print_labeled_type(CLASS_TABLE_CELL *ctc, LPAIR lp)
{
  print_labeled_type(ERR_FILE, ctc, lp);
  if(LISTING_FILE != NULL && LISTING_FILE != ERR_FILE) {
    print_labeled_type(LISTING_FILE, ctc, lp);
  }
}


/****************************************************************
 *			ERR_HEAD				*
 ****************************************************************
 * Start an error or warning message, by printing the file,	*
 * line, etc.  Set error_occurred, local_error_occurred, etc.	*
 * Load the error strings if necessary.				*
 *								*
 * Parameter KIND should be 0 for an error and 1 for a warning. *
 ****************************************************************/

void err_head(int kind, int line)
{

  load_error_strings();

  if(file_info_st != NULL) {
    char* fname = strdup(current_file_name);
    force_external(fname);
    if(LISTING_FILE != NULL) fflush(LISTING_FILE);
    if(line == 0) line = current_line_number;
    if(!error_reported) err_nl();
    err_print(kind, line, fname);
    FREE(fname);
  }
  else {
    err_print(SPACE2_ERR);
  }
  if(defining_id != NULL) {
    err_print(DEFINING_ERR, defining_id);
  }
  err_print(ERROR_ERR + kind);

  error_reported = TRUE;
  if(kind == 0) {
    error_occurred = TRUE;
    if(!soft_err) local_error_occurred = TRUE;
    soft_err = FALSE;
  }
  else {
    warning_occurred = TRUE;
  }
}

 
/****************************************************************
 *		      WARN0, WARN1, WARN2, DOWARN2		*
 ****************************************************************
 * Print a warning message.  F is the error number, and S, S1,	*
 * S2 are and additional parameter for the format.  LINE is the	*
 * line to report that the warning occurs on.			*
 *								*
 * do_warn2 holds the shared code.				*
 *								*
 * warn0 keeps track of warnings that it has made, and does	*
 * not make the same warning twice in the same declaration.	*
 ****************************************************************/

PRIVATE void dowarn2(int n, CONST char *s1, CONST char *s2, int line);

/*-----------------------------------------------------------*/

void warn0(int n, int line)
{
  if(should_suppress_warning(FALSE)) return;
  if(int_member(n, reported_semantic_errs)) return;
  SET_LIST(reported_semantic_errs, int_cons(n, reported_semantic_errs));
  dowarn2(n, NULL, NULL, line);
}

/*-----------------------------------------------------------*/

void warn1(int n, CONST char *s, int line)
{
  if(should_suppress_warning(FALSE)) return;
  dowarn2(n, s, NULL, line);
}

/*-----------------------------------------------------------*/

void warn2(int n, CONST char *s1, CONST char *s2, int line)
{
  if(should_suppress_warning(FALSE)) return;
  dowarn2(n, s1, s2, line);
}

/*-----------------------------------------------------------*/

PRIVATE void dowarn2(int n, CONST char *s1, CONST char *s2, int line) 
{
  err_head(1, line);
  err_print(n, s1, s2);
  err_nl();
}


/****************************************************************
 *			SHOULD_SUPPRESS_WARNING			*
 ****************************************************************
 * Parameter SUPPRESS_THIS_WARNING is TRUE if a particular	*
 * class of warnings has been suppressed.  Return TRUE if a 	*
 * warning of this class should be suppressed, and FALSE 	*
 * otherwise, taking into account err_flags.suppress_warnings	*
 * and err_flags.no_suppress_warnings.				*
 ****************************************************************/

Boolean should_suppress_warning(Boolean suppress_this_warning)
{
  return !err_flags.no_suppress_warnings &&
         (err_flags.suppress_warnings || suppress_this_warning);
}


/****************************************************************
 *			DANGEROUS_DEFAULT_WARN			*
 ****************************************************************
 * Warn that a dangerous default is being done.  Indicate that  *
 * variable V_UNUSED is being bound to DFAULT_UNUSED in the 	*
 * type of expression E, which is part of the definition of	*
 * identifier DEFINING.						*
 *								*
 * This warning is suppressed by 				*
 * err_flags.suppress_dangerous_default_warnings.		*
 ****************************************************************/

void dangerous_default_warn(EXPR *e, TYPE *v_unused, TYPE *dfault_unused, 
			    CONST char *defining)
{
  if(!should_suppress_warning(err_flags.suppress_dangerous_default_warnings)){
    warn2(DEFAULT_WARN_ERR, display_name(defining), NULL, e->LINE_NUM);
    err_print_expr(e);
  }
}


/****************************************************************
 *			WARN_GLOBAL_SHADOW			*
 ****************************************************************
 * Print a warning that NAME shadows a global, provided that	*
 * warning is not suppressed.  LINE is the line number of the	*
 * error.							*
 ****************************************************************/

void warn_global_shadow(CONST char *name, int line)
{
  if(!should_suppress_warning(err_flags.suppress_global_shadow_warnings)) {
    warn1(GLOBAL_SHADOW_ERR, display_name(name), line);
  }
}


/****************************************************************
 *			WARN_STRONG_MISSING			*
 ****************************************************************
 * Print a warning that NAME: DEF_TYPE is strongly missing with *
 * type MISS_TYPE.						*
 ****************************************************************/

void warn_strong_missing(CONST char *name, TYPE *def_type, TYPE *miss_type)
{
  warn1(STRONG_MISSING_ERR, display_name(name), 0);
  err_print(DEFINED_TYPE_IS_ERR);
  err_print_ty_with_constraints_indented(def_type, 
					 indent_len(DEFINED_TYPE_IS_ERR));
  err_nl();
  err_print(MISSING_TYPE_IS_ERR);
  err_print_ty_with_constraints_indented(miss_type, 
				         indent_len(MISSING_TYPE_IS_ERR));
  err_nl();
}


/****************************************************************
 *				MO_WARN				*
 ****************************************************************
 * Warn that lazy expression E can produce a stream or might 	*
 * fail, provided list 'properties' contains 			*
 * act__produce__stream or act__fail... .			*
 ****************************************************************/

void mo_warn(EXPR *e, LIST *properties)
{
  if(should_suppress_warning(err_flags.suppress_property_warnings)) return;

  if(!is_suppressed_property(act_produce_stream)
     && rolelist_member(act_produce_stream, properties, 0)) {
    warne(LAZY_STREAM_ERR, NULL, e);
  }

  if(!is_suppressed_property(act_fail) && has_act_fail(properties)) {
    warne(LAZY_FAIL_ERR, NULL, e);
  }
}


/****************************************************************
 *				DEF_PROP_WARN			*
 ****************************************************************
 * Warn that ID: T has property PROP, but has not been declared	*
 * such.							*
 ****************************************************************/

void def_prop_warn(CONST char *id, CONST char *prop, TYPE *t, int line)
{
  warn1(DEF_PROP_ERR, prop, line);
  err_print(SPACE2_STR_ERR, id);
  err_print_ty_with_constraints_indented(t, strlen(id) + 5);
  err_nl();
}


/****************************************************************
 *			 SURE_TYPE_MISMATCH_WARN		*
 ****************************************************************
 * Irregular function FUN is performing a binding that will     *
 * surely fail at run time.  Issue a warning to this effect.    *
 * Suppress multiple warnings on a single line.
 *								*
 * Type A is on the type stack when the binding takes place,    *
 * and type T is being bound to A.				*
 ****************************************************************/

void sure_type_mismatch_warn(EXPR *fun, TYPE *t, TYPE *A)
{
  int line;
  CONST char *name;

  if(should_suppress_warning(FALSE)) return;

  line = fun->LINE_NUM;
  fun  = skip_sames(fun);
  name = display_name(fun->STR);

  if(int_member(line, sure_mismatch_reported_lines)) return;

  dowarn2(RESTRICTION_ERR, name, NULL, line);
  err_print(PREVIOUS_TYPE_IS_ERR);
  err_print_ty_with_constraints_indented(A, indent_len(PREVIOUS_TYPE_IS_ERR));
  err_nl();
  err_print(NEW_TYPE_IS_ERR);
  err_print_ty_with_constraints_indented(t, indent_len(NEW_TYPE_IS_ERR));
  err_nl();

  SET_LIST(sure_mismatch_reported_lines, 
	   int_cons(line, sure_mismatch_reported_lines));
}


/****************************************************************
 *			 CONSTRAINED_DYN_VAR_ERR		*
 ****************************************************************
 * Warn that variable V has both a >= and a >>= constraint.     *
 * Function FUN is the function that does an unwrap that binds  *
 * this variable.						*
 ****************************************************************/

void constrained_dyn_var_err(TYPE *V, EXPR *fun)
{
  if(should_suppress_warning(FALSE)) return;
  dowarn2(CONSTRAINED_DYN_VAR_ERR, NULL, NULL, fun->LINE_NUM);
  err_print(VARIABLE_IS_ERR);
  err_print_ty(V);
  err_nl();
  err_print(IRRFUN_IS_ERR);
  err_print_expr(fun);
  err_nl();
}


/****************************************************************
 *				WARNE				*
 ****************************************************************
 * Do a warning with an expression E printed after the warning.	*
 * F is the warning number, and A is a parameter for it.	*
 ****************************************************************/

void warne(int f, CONST char *a, EXPR *e)
{
  if(should_suppress_warning(FALSE)) return;
  dowarn2(f, a, NULL, e->LINE_NUM);
  err_print_expr(e);
}
  

/****************************************************************
 *			SYNTAX_ERROR				*
 *			SYNTAX_ERROR1				*
 *			SYNTAX_ERROR2				*
 *			SYNTAX_ERROR3				*
 ****************************************************************
 * Report error number N, possibly with parameters for the      *
 * error message.						*
 ****************************************************************/

void syntax_error(int n, int line)
{
  syntax_error1(n, NULL_S, line);
}

/*-----------------------------------------------------------*/

void syntax_error1(int n, CONST char *s, int line)
{
  syntax_error2(n, s, NULL, line);
}

/*-----------------------------------------------------------*/

void syntax_error2(int n, CONST char *s1, CONST char *s2, int line)
{
# ifdef DEBUG
    if(trace && panic_mode) {
      trace_t(99, toint(panic_mode));
    }
# endif

  err_head(0, line);
  err_print(n, s1, s2);
  err_nl();
}


void 
syntax_error3(int n, CONST char *s1, CONST char *s2, CONST char *s3, 
	      int line)
{
# ifdef DEBUG
    if(trace && panic_mode) {
      trace_t(99, toint(panic_mode));
    }
# endif

  err_head(0, line);
  err_print(n, s1, s2, s3);
  err_nl();
}


/****************************************************************
 *			SOFT_SYNTAX_ERROR			*
 *			SOFT_SYNTAX_ERROR1			*
 *			SOFT_SYNTAX_ERROR2			*
 ****************************************************************
 * Report error number N, pointing to current token, but don't	*
 * set local_error_occurred.					*
 ****************************************************************/

void soft_syntax_error(int n, int line)
{
  soft_syntax_error1(n, NULL_S, line);
}

/*-----------------------------------------------------------*/

void soft_syntax_error1(int n, CONST char *s, int line)
{
  soft_syntax_error2(n, s, NULL, line);
}

/*-----------------------------------------------------------*/

void soft_syntax_error2(int n, CONST char *s1, CONST char *s2, int line)
{
  soft_err = TRUE;
  syntax_error2(n, s1, s2, line);
}


/****************************************************************
 *			SEMANTIC_ERROR				*
 ****************************************************************
 * Report error number N.					*
 ****************************************************************/

void semantic_error(int n, int line)
{
  if(int_member(n, reported_semantic_errs)) return;
  SET_LIST(reported_semantic_errs, int_cons(n, reported_semantic_errs));
  semantic_error2(n, NULL_S, NULL_S, line);
}


/*****************************************************************
 *			YYERROR					 *
 *****************************************************************
 * Print an error message from the parser.  This is an unhandled *
 * parser error.						 *
 *****************************************************************/

int yyerror(char *s_unused)
{
  /*-------------------------------------------------------------*
   * If not in panic mode already, print a generic syntax error  *
   * message.							 *
   *-------------------------------------------------------------*/

  if(!panic_mode) syntax_error(SYNTAX_ERR, 0);

  /*-------------------------------------------------------------*
   * If we have not even got so far into the package that we 	 *
   * have not seen the name of the package, complain about	 *
   * a bad package heading.					 *
   *-------------------------------------------------------------*/

  if(!have_read_main_package_name && main_context == EXPORT_CX) {
    err_print(BAD_PACKAGE_HEADING_ERR);
  }

  /*-------------------------------------------------------------*
   * If the most recent token was a reserved word, print a 	 *
   * message saying so, just in case that was the problem.	 *
   *-------------------------------------------------------------*/

  if(is_reserved_word_tok(last_tok) && yytext[0] != 0) {
    err_print(LAST_RESERVED_ERR, yytext);
  }

  /*-------------------------------------------------------------*
   * If the most recent token was a unary operator, warn about	 *
   * that, since that might have been part of the problem.	 *
   *-------------------------------------------------------------*/

  else if(last_tok == UNARY_OP || last_last_tok == UNARY_OP) {
    err_print(BAD_UNARY_OP_ERR,
	      display_name((last_last_tok == UNARY_OP)   
			     ? last_last_yylval.ls_at.name
		             : last_yylval.ls_at.name));
  }

  /*-------------------------------------------------------------*
   * Sometimes an unknown identifier causes a parse error.  	 *
   * Print a message if the most recently read token is an	 *
   * unknown id.						 *
   *-------------------------------------------------------------*/

  else if(last_tok == UNKNOWN_ID_TOK || last_last_tok == UNKNOWN_ID_TOK) {
    err_print(BAD_UNKNOWN_ID_ERR, 
	      display_name((last_last_tok == UNKNOWN_ID_TOK)   
			     ? last_last_yylval.ls_at.name
		             : last_yylval.ls_at.name));
  }

  return 0;
}


/****************************************************************
 *			SHOW_SYNTAX				*
 ****************************************************************
 * In novice mode, show the syntax of the construct that starts *
 * with token TOK, if there is an entry for it.			*
 ****************************************************************/

void show_syntax(int tok)
{
  char *msg;
  Boolean go = FALSE;

  if(!err_flags.novice) return;
 
  if(FIRST_RESERVED_WORD_TOK <= tok &&
     tok <= LAST_RESERVED_WORD_TOK) {
    read_syntax_forms();
    msg = syntax_form[tok - FIRST_RESERVED_WORD_TOK];
    go = TRUE;
  }

  if(FIRST_ALT_RESERVED_WORD_TOK <= tok &&
     tok <= LAST_ALT_RESERVED_WORD_TOK) {
    read_alt_syntax_forms();
    msg = alt_syntax_form[tok - FIRST_ALT_RESERVED_WORD_TOK];
    go = TRUE;
  }

  if(go && msg != NULL && msg[0] != 0) {
    err_print(SYNTAX_IS_ERR);
    err_print_str(msg);
    err_nl();
  }
}


/****************************************************************
 *			EOF_ERR					*
 ****************************************************************
 * Indicate that an end-of-file has been encountered before it  *
 * was expected.						*
 ****************************************************************/

PRIVATE char* last_eof_err_package = NULL;

void eof_err(void)
{
  /*---------------------------------------------*
   * Avoid saying the same thing several times.  *
   *---------------------------------------------*/

  if(current_package_name != last_eof_err_package) {
    syntax_error(EOF_ERR, 0);
    suppress_panic = 1;
    last_eof_err_package = current_package_name;
  }
}


/****************************************************************
 *			BAD_CU_MEM_ERR				*
 ****************************************************************
 * Complain that a class-union member T is not correct.		*
 ****************************************************************/

void bad_cu_mem_err(TYPE *t, int line)
{
  syntax_error(BAD_CU_MEM_ERR, line);
  err_print(BAD_MEM_IS_ERR);
  err_print_ty(t);
  err_nl();
}


/****************************************************************
 *			BAD_PATFUN_TYPE_ERROR			*
 ****************************************************************
 * Complain that a pattern function has an unacceptable type.   *
 * NAME is the name of the pattern function, PREV_TYPE is the   *
 * type inferred up to now, and NEWW_TYPE is the type that	*
 * appears to be needed.					*
 ****************************************************************/

void 
bad_patfun_type_error(CONST char *name, TYPE *prev_type, TYPE *neww_type)
{
  semantic_error1(BAD_PATFUN_TYPE_ERR, display_name(name), 0);
  err_print(PREVIOUS_TYPE_IS_ERR);
  err_print_ty_with_constraints_indented(prev_type, 
					 indent_len(PREVIOUS_TYPE_IS_ERR));
  err_nl();
  err_print(NEW_TYPE_IS_ERR);
  err_print_ty_with_constraints_indented(neww_type, 
					 indent_len(NEW_TYPE_IS_ERR));
  err_nl();
}


/****************************************************************
 *			BAD_EXTENS_ERR				*
 ****************************************************************
 * Report a bad extension.  C is the class table entry for the	*
 * genus or community being extended, and T is the class	*
 * union element being added.  Print string S, if not null, 	*
 * beneath error line.						*
 ****************************************************************/

void bad_extens_err(CLASS_UNION_CELL *t, CLASS_TABLE_CELL *c, 
		    CONST char *s)
{
  int tok;

  err_head(0, t->line);
  err_print(CANNOT_EXTEND_ERR, display_name(c->name));
  tok = t->tok;
  if(tok == COMM_ID_TOK || tok == FAM_ID_TOK || tok == GENUS_ID_TOK) {
    err_print(STR_ERR, display_name(t->name));
  }
  else if(tok == TYPE_LIST_TOK) {
    TYPE_LIST *p;
    for(p = t->CUC_TYPES; p != NIL; p = p->tail) {
      err_print_ty(p->head.type);
      err_print_str(" ");
    }
    err_nl();
  }
  else {
    err_print_ty(t->CUC_TYPE);
    err_nl();
  }

  if(s != NULL) {
    err_print(SPACE2_ERR);
    err_print(STR_ERR, s);
  }
}


/****************************************************************
 *		        POLYMORPHISM_ERROR			*
 ****************************************************************
 * Complain the definition of identifier ID_BEING_DEFINED	*
 * is polymorphic, and should not be. 				*
 ****************************************************************/

void polymorphism_error(EXPR* id_being_defined)
{
  char* name = id_being_defined->STR;
  PRINT_TYPE_CONTROL c;

  begin_printing_types(ERR_FILE, &c);
  semantic_error1(POLYMORPHISM_ERR, display_name(name), 0);
  err_print(SPACE2_ERR);
  attach_type(id_being_defined, &c);
  err_nl();
  end_printing_types(&c);
}


/****************************************************************
 *			TY_EXPR_ERROR				*
 ****************************************************************
 * Print a message that contains an expression E and a type T.  *
 * Message I1 is the main message, and message I2 introduces    *
 * the type.							*
 *								*
 * If SHOW_CONSTRAINTS is true, then do a full print of t.  	*
 * Otherwise, print T without constraints.			*
 ****************************************************************/

PRIVATE void 
ty_expr_error(int i1, int i2, EXPR *e, TYPE *t, Boolean show_constraints)
{
  PRINT_TYPE_CONTROL c;

  err_head(0, e->LINE_NUM);
  err_print(i1);
  err_nl();
  begin_printing_types(ERR_FILE, &c);
  err_print(EXPRESSION_ERR, toint(e->LINE_NUM));
  err_shrt_pr_expr(e);
  err_nl();
  if(e->ty != NULL) {
    err_print(CURRENT_TYPE_ERR);
    err_print_ty1_with_constraints_indented(e->ty, &c, 
					    indent_len(CURRENT_TYPE_ERR));
    err_nl();
  }
  err_print(SPACE2_ERR);
  err_print(i2);
  if(show_constraints) {
    err_print_ty1_with_constraints_indented(t, &c, indent_len(i2));
  }
  else err_print_ty1_without_constraints(t, &c);
  err_nl();
  end_printing_types(&c);
}


/****************************************************************
 *			TYPE_ERROR				*
 ****************************************************************
 * Print an error message that type type of E does not match    *
 * type T.							*
 ****************************************************************/

void type_error(EXPR *e, TYPE *t)
{
  EXPR *ee;
  EXPECTATION *exp;

  ty_expr_error(CANNOT_MATCH_TYPE_ERR, NOW_NEED_ERR, e, t, TRUE);

  ee = skip_sames(e);
  if(EKIND(ee) == OVERLOAD_E) {
    err_print(AVAILABLE_TYPES_ERR, display_name(ee->STR));
    for(exp = ee->GIC->expectations; exp != NULL; exp = exp->next) {
      err_print(SPACE2_ERR);
      err_print(SPACE2_ERR);
      err_print_ty_with_constraints_indented(exp->type, 5);
      err_nl();
    }
  }
}

/****************************************************************
 *			UNBOUND_TYVAR_ERROR	       		*
 ****************************************************************
 * Report that variable V is unbound in the type of expression	*
 * E, which is part of the definition of identifier DEFINING.	*
 ****************************************************************/

void unbound_tyvar_error(EXPR *e, TYPE *V, CONST char *defining)
{
  if(!err_flags.did_unbound_tyvar_err) {
    ty_expr_error(CONSIDER_TAG_ERR, UNBOUND_VAR_IS_ERR, e, V, FALSE);
    err_print(WHILE_DEFINING_ERR, defining);
    err_flags.did_unbound_tyvar_err = TRUE;
  }
}


/****************************************************************
 *			REPORT_ROLE				*
 ****************************************************************
 * Report "previous role" THIS_ROLE for NAME.  PACK_NAMES	*
 * is a list of the names of the packages that contribute	*
 * role information to the role that is being complained about.	*
 ****************************************************************/

void report_role(ROLE *this_role, CONST char *name, STR_LIST *pack_names)
{
  STR_LIST *p;
  err_print(PREV_ROLE_ERR, name);
  err_print_role(this_role);
  for(p = pack_names; p != NIL; p = p->tail) {
    err_print(PACKAGE_STR_ERR, p->head.str);
  }
}


/****************************************************************
 *			ROLE_ERROR				*
 ****************************************************************
 * Report a mismatch between roles R1 and R2 at expression E.	*
 * If E is NULL, don't print an expression.  If PROPERTY_ERR is	*
 * true, then this error is a property error, such as no__role	*
 * together with role.  Otherwise, it is a singleton-role	*
 * mismatch error.						*
 ****************************************************************/

void role_error(EXPR *e, CONST char *r1, CONST char *r2, Boolean property_err)
{
  EXPR_TAG_TYPE kind;
  int line;

  /*------------------------------------------------------------------*
   * If this is a property error, and property errors are suppressed, *
   * do nothing. 						      *
   *------------------------------------------------------------------*/

  if(property_err &&
     should_suppress_warning(err_flags.forever_suppress_property_warnings)) {
    return;
  }

  role_error_occurred = TRUE;
  line = (e == NULL) ? current_line_number : e->LINE_NUM;

  /*---------------------------------------------------------*
   * If this is not a property error and there was already a *
   * report at this line, do not make another report.  If    *
   * there has not yet been a report at this line, then	     *
   * record this line.					     *
   *---------------------------------------------------------*/

  {HASH1_CELLPTR h;
   HASH_KEY u;
   u.num = line;
   h = insert_loc_hash1(&role_lines, u, inthash(u.num), eq);
   if(h->key.num != 0) return;
   h->key.num = u.num;
  }

  /*--------------------*
   * Issue the warning. *
   *--------------------*/

  dowarn2(ROLE_MISMATCH_ERR, NULL, NULL, line);
  if(e != NULL) {
    kind = EKIND(e);
    if(kind == APPLY_E || kind == MATCH_E
       || kind == LET_E || kind == DEFINE_E || kind == PAT_DCL_E
       || kind == EXPAND_E) {
      EXPR *lhs, *rhs;
      char *lhs_head, *rhs_head;

      lhs = e->E1;
      rhs = e->E2;
      if(kind == APPLY_E) {
	lhs_head = "Function";
	rhs_head = "Argument";
      }
      else {
	lhs_head = "LHS";
	rhs_head = "RHS";
      }
      err_print(STR_LINE_ERR, lhs_head, toint(lhs->LINE_NUM));
      err_shrt_pr_expr(lhs);
      err_nl();
      err_print(STR_LINE_ERR, rhs_head, toint(rhs->LINE_NUM));
      err_shrt_pr_expr(rhs);
      err_nl();
      err_print(ROLE1_ERR);
      if(kind == APPLY_E && lhs->role != NULL && lhs->role->role1 != NULL) {
        err_print_role(lhs->role->role1);
      }
      else err_print_role(lhs->role);
      err_nl();
      err_print(ROLE2_ERR);
      err_print_role(rhs->role);
      err_nl();
    }
    else {
      err_print(EXPRESSION_ERR, toint(e->LINE_NUM));
      err_shrt_pr_expr(e);
      attach_role(e);
      err_nl();
    }
  }

  err_print(ROLE1_ERR);
  err_print_str(r1);
  err_nl();
  err_print(ROLE2_ERR);
  err_print_str(r2);
  err_nl();
}


/****************************************************************
 *			ERR_PRINT_EXPR				*
 ****************************************************************
 * Print "Expression: " followed by expression E.		*
 ****************************************************************/

void err_print_expr(EXPR *e)
{
  if(e != NULL) {
    err_print(EXPRESSION_ERR, toint(e->LINE_NUM));
    err_short_print_expr(e);
  }
}


/****************************************************************
 *			EXPR_ERROR				*
 ****************************************************************
 * Report error N with expression E.  Don't report the same	*
 * message at the same line more than once.			*
 ****************************************************************/

PRIVATE int last_expr_error_line = 0;
PRIVATE int last_expr_error_num = 0;

void expr_error(int n, EXPR *e)
{
  int line = e->LINE_NUM;

  if(n == last_expr_error_num && line == last_expr_error_line) return;

  err_head(0, line); 
  err_print(STR_ERR, error_strings[n]);
  err_print_expr(e);
  last_expr_error_num  = n;
  last_expr_error_line = line;
}


/****************************************************************
 *			APPL_TYPE_ERROR				*
 ****************************************************************
 * Indicate that function E1 cannot be applied to argument E2   *
 * due to a type conflict.					*
 ****************************************************************/

void appl_type_error(EXPR *e1, EXPR *e2)
{
  PRINT_TYPE_CONTROL c;

  err_head(0, e1->LINE_NUM);
  err_print(CANNOT_APPLY_ERR);
  err_print(FUNCTION_IS_ERR, toint(e1->LINE_NUM));
  begin_printing_types(ERR_FILE, &c);
  err_shrt_pr_expr(e1);
  attach_type(e1, &c);
  err_nl();
  err_print(ARGUMENT_IS_ERR, toint(e2->LINE_NUM));
  err_shrt_pr_expr(e2);
  attach_type(e2, &c);
  err_nl();
  end_printing_types(&c);
}


/****************************************************************
 *			NO_PAT_PARTS_ERROR			*
 ****************************************************************
 * Complain that function PF has no pattern rules.		*
 ****************************************************************/

void no_pat_parts_error(EXPR *pf)
{
  char *sname = display_name(nonnull(pf->STR));

  if(unknown_id_error(NO_PAT_PARTS_ERR, sname, pf->LINE_NUM)) {
    err_print(INFERRED_STR_ERR, sname);
    err_print_ty_with_constraints_indented
	    (pf->ty, indent_len(INFERRED_STR_ERR) + strlen(sname));
    err_nl();
  }
}


/****************************************************************
 *			CHECK_FOR_BAD_COMPILE_ID		*
 ****************************************************************
 * If a local error occurred and defining_id is not NULL, then  *
 * add defining_id to the bad_compile_ids list.			*
 *								*
 * defining_id is a global described in parser/parsvars.c.	*
 ****************************************************************/

void check_for_bad_compile_id(void)
{
  char *id = display_name(defining_id);
  if(id != NULL && local_error_occurred) {
    if(!str_memq(id, bad_compile_ids)) {
      SET_LIST(bad_compile_ids, str_cons(id, bad_compile_ids));
    }
    defining_id = NULL;
  }
}


/****************************************************************
 *			PRINT_ASTHELP_LOC			*
 ****************************************************************
 * If asthelp knows about identifier ID, then print a message   *
 * saying where it can be found on file F.			*
 ****************************************************************/

PRIVATE Boolean reported_asthelp_file;

PRIVATE void
print_asthelp_loc_help(FILE *f, CONST char *dir_unused, 
		       CONST char *id, void *info_unused)
{
  char knd, *found_id, *filename;

  do {
    filename = next_file(f, id, &knd, &found_id);
    if(filename != NULL) {
      if(knd != 'N') {
        if(!reported_asthelp_file) {
          err_print(ID_FOUND_IN_ERR, id);
        }
        if(prefix(get_absolute_std_dir(), filename)) {
          err_print_str("    $%s\n", 
			filename + strlen(get_absolute_std_dir()));
        }
        else {
          force_external1(filename);
          err_print_str("%s\n", filename);
        }
        reported_asthelp_file = TRUE;
      }
      free(filename);
      free(found_id);
    }
  } while(filename != NULL);
}

/*--------------------------------------------------------------------*/

PRIVATE Boolean print_asthelp_loc(CONST char *id)
{
  if(str_member(id, global_reported_unknown_ids)) return TRUE;
  else {
    reported_asthelp_file = FALSE;
    find_description_info(id, asthelp_path(), print_asthelp_loc_help, NULL);
    if(reported_asthelp_file) {
      SET_LIST(global_reported_unknown_ids, 
	       str_cons(id_tb0(id), global_reported_unknown_ids));
    }
    return reported_asthelp_file;
  }
}


/****************************************************************
 *			UNKNOWN_ID_ERROR			*
 ****************************************************************
 * Complain that identifier ID is unknown.  Use error number 	*
 * ERR to do the report.					*
 *								*
 * If this same id has been reported unknown before in this	*
 * declaration, do not report again. 				*
 *								*
 * Return true if the report was done, and false if the report	*
 * was suppressed.						*
 ****************************************************************/

Boolean unknown_id_error(int err, CONST char *id, int line)
{
  id = display_name(id);

  if(id == NULL) syntax_error(UNK_ID_ERR, line);

  /*------------------------------------------------------------*
   * If we already complained about this identifier in this	*
   * declaration, don't complain again.				*
   *------------------------------------------------------------*/

  else if(str_member(id, reported_unknown_ids) != 0) return FALSE;

  else if(str_memq(id, bad_compile_ids)) {
    syntax_error1(UNKNOWN_PREV_ID_ERR, id, line);
  }

  /*-----------------------------------------------------------*
   * If we see x being used in a declaration Let x = ..., then *
   * suggest using Define instead of Let.		       *
   *-----------------------------------------------------------*/

  else if(id == main_id_being_defined) {
    syntax_error1(RECURSIVE_LET_ERR, id, line);
  }

  /*-----------------------------------------------*
   * Otherwise, just complain about an unknown id. *
   *-----------------------------------------------*/

  else {
    syntax_error1(err, id, line);
    if(pat_vars_ok) err_print(MISSING_QUESTION_MARK_ERR);

    /*----------------------------------------------------------*
     * Print extra information.  If this id is known to asthelp,*
     * then say where it is defined.  If not, then indicate	*
     * possible reasons for an unknown id.			*
     *----------------------------------------------------------*/

    if(!print_asthelp_loc(id)) {
      if(!printed_unknown_id_message && err_flags.novice) {
        err_print(UNKNOWN_ID_REASONS_ERR);
        printed_unknown_id_message = TRUE;
      }
    }
  }

  /*------------------------------------------------------------------*
   * Record this id to prevent several complaints about the same	*
   * id in one declaration.						*
   *------------------------------------------------------------------*/

  SET_LIST(reported_unknown_ids, str_cons(id, reported_unknown_ids));

  return TRUE;
}


/****************************************************************
 *			CHECK_FOR_ENT_ID			*
 ****************************************************************
 * Report an error if NAME is a known entity identifier.  This  *
 * function should be called when NAME is about to be defined   *
 * as a type identifier.					*
 *								*
 * Note: NAME should be in the string table.			*
 *								*
 * Returns TRUE if an error was reported, FALSE if none was	*
 * reported.							*
 ****************************************************************/

Boolean check_for_ent_id(CONST char *name, int line)
{
  if(is_visible_global_tm(name, current_package_name, TRUE)) {
    semantic_error1(ENT_ID_AS_CLASS_ID_ERR, display_name(name), line);
    return TRUE;
  }
  else return FALSE;
}


/****************************************************************
 *			DUP_INTERSECT_ERR			*
 ****************************************************************
 * Report that inconsistent intersections have been declared.	*
 * The intersection A_NAME & B_name = H_VAL(H_LABS) was 	*
 * previously reported, but intersection A_NAME & B_NAME = 	*
 * C(C_LABS) is now declared.					*
 ****************************************************************/

void dup_intersect_err(char *a_name, char *b_name, CLASS_TABLE_CELL *c, 
		       LPAIR c_labs, int h_val, LPAIR h_labs)
{
  CLASS_TABLE_CELL *h_ctc;
  CONST char* da_name = display_name(a_name);
  CONST char* db_name = display_name(b_name);

  err_head(0, current_line_number);
  h_ctc = ctcs[h_val];
  err_print(INCONSISTENT_INTERSECT_ERR, da_name, db_name);
  err_print_labeled_type(h_ctc, h_labs);
  err_nl();
  err_print(REQUEST_ERR, da_name, db_name);
  err_print_labeled_type(c, c_labs);
  err_nl();
}


/****************************************************************
 *			BAD_OVERLAP				*
 ****************************************************************
 * Report that there is a bad overlap of types in expectations  *
 * or definitions of NAME.  Use error number N for the report.	*
 ****************************************************************/

void bad_overlap(int n, CONST char *name, 
		 TYPE *old_overlap_type, TYPE *new_overlap_type,
		 char *old_package_name, int old_line, int line)
{
  err_head(0, line);
  err_print(n, display_name(name),old_package_name, old_line);
  err_print_ty_with_constraints_indented(old_overlap_type,
					 indent_len(THIS_DCL_TYPE_ERR));
  err_print(THIS_DCL_TYPE_ERR);
  err_print_ty_with_constraints_indented(new_overlap_type,
					 indent_len(THIS_DCL_TYPE_ERR));
  err_nl();
}


/****************************************************************
 *			PARTS_OVERLAP_ERR			*
 ****************************************************************
 * Report that there is a bad overlap of types in parts.  P1 	*
 * and P2 are the parts that overlap, and NAME is the name of   *
 * the identifier.						*
 ****************************************************************/

void parts_overlap_err(CONST char *name, ENTPART *p1, ENTPART *p2)
{
  int ind;
  name = display_name(name);
  ind = strlen(name) + 7;
  err_head(0, 0);
  err_print(DEF_OVERLAP_ERR, name);
  err_print(PACK_LINE_TYPE_ERR, p1->package_name, p1->line_no, name);
  err_print_ty_with_constraints_indented(p1->ty, ind);
  err_nl();
  err_print(PACK_LINE_TYPE_ERR, p2->package_name, p2->line_no, name);
  err_print_ty_with_constraints_indented(p2->ty, ind);
  err_nl();
  if(p1->irregular || p2->irregular) {
    err_print(IRREG_OVERLAP_ERR);
  }
}
