/**********************************************************************
 * File:    utils/strops.h
 * Purpose: Miscellaneous string functions from strops.c, t_strops.c
 *          and m_strops.c.
 * Author:  Karl Abrahamson
 **********************************************************************/

/*********************** From strops.c **************************/

CONST char*   name_tail		(CONST char *s);
void          fgets1		(char *s, int n, FILE *f);
CONST char*   nonnull		(CONST char* s);
CONST char*   display_name	(CONST char *s);
CONST char*   quick_display_name(CONST char *s);

Boolean suffix_ignore_case	(CONST char *x, CONST char *y);
Boolean suffix			(CONST char *x, CONST char *y);
Boolean prefix_ignore_case	(CONST char *x, CONST char *y);
Boolean prefix			(CONST char *x, CONST char *y);

CONST char*   concat_id		(CONST char *x, CONST char *y);
CONST char*   concat_string	(CONST char *x, CONST char *y);
STR_LIST*     colonsep_to_list	(CONST char *s);
Boolean lcequal			(CONST char *a, CONST char *b);

/*********************** From t_strops.c **************************/

#ifdef TRANSLATOR
 extern int temp_var_num;
 Boolean is_hidden_id    		(CONST char *s);
 Boolean is_proc_id      		(CONST char *id);
 int     last_char       		(CONST char *s);

 char*   	my_name 		(char *s);
 char*   	skip_white		(char *s);
 CONST char*   	remove_dots		(CONST char *name);
 CONST char*   	new_temp_var_name	(void);
 CONST char*   	new_name        	(CONST char *s, Boolean ent);
 CONST char*   	new_concat_id		(CONST char *s, CONST char *t, 
					 Boolean ent);
 CONST char*   attach_prefix_to_id	(CONST char *pref, CONST char *id, 
					 int case_select);
 CONST char*   lower_case		(CONST char *id);
 CONST char*   constructor_name		(CONST char *name);
 CONST char*   tester_name     		(CONST char *name);
 CONST char*   forgetful_tester_name	(CONST char *name);
 CONST char*   destructor_name		(CONST char *id);
 CONST char*   make_role_mod_id		(CONST char *id);
 CONST char*   make_role_sel_id		(CONST char *id);
 int	       char_val			(CONST char *s, int line);
 CONST char*   restore_cc_var_name	(CONST char *orig_name);
 Boolean       real_consts_equal	(CONST char *s1, CONST char *s2);
#endif

/*********************** From m_strops.c **************************/

#ifdef MACHINE
 Boolean is_package_local_name  (CONST char *name);
 Boolean is_white		(char c);
 Boolean printable		(char c);
 Boolean only_printable		(CONST char* s, LONG n);
#endif



