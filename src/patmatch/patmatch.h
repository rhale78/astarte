/****************************************************************
 * File:    patmatch/patmatch.h
 * Purpose: Handle modes of pattern functions
 * Author:  Karl Abrahamson
 ****************************************************************/


/*********************** From patutils.c *******************************/


Boolean is_simple_pattern	(EXPR *e);
Boolean is_fixed_pat		(EXPR *pattern);
Boolean not_pat			(EXPR *pattern);
int     pat_fun_tag     	(EXPR *pf, TYPE *pat_type);


/*********************** From mode.c ***********************************/

Boolean mode_match_pm		(EXPR *rule_mode, EXPR *arg_mode, 
				 Boolean check_fun, Boolean *is_restrictive);


/********************** From translt.c *****************************/

extern Boolean should_show_patmatch_subst;
extern Boolean always_should_show_patmatch_subst;
extern Boolean warn_on_compare;
extern LONG    max_patmatch_subst_depth;

EXPR*   translate_matches_pm    (EXPR *e, Boolean report_errs, int depth);
EXPR*   translate_match_pm      (EXPR *e, int unif, Boolean lazy, int line,
				 Boolean report_errs, int depth);
Boolean do_patmatch_infer_pm    (EXPR *e, int err_kind, int line);
EXPR*   translate_expand_pm	(EXPR *e, Boolean report_errs, int line, 
				 int depth);


/*********************** From substit.c *******************************/

EXPR*	substitute_pm	  (EXPR *formals, EXPR *actuals, EXPR *translation, 
		           EXPR *image, int unif, Boolean lazy,
                           Boolean open, Boolean do_infer, int line);

/****************************************************************
 *			From pmcompl.c				*
 ****************************************************************/

extern LIST *choose_matching_lists, 
            *working_choose_matching_lists, 
            *copy_choose_matching_lists;

TYPE_LIST* 	pattern_to_types	(EXPR *pat);
TYPE* 		type_to_tag_type	(TYPE *t);
void 		check_pm_completeness	(void);

