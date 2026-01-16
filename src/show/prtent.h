/********************************************************************
 * File:    show/prtent.h
 * Purpose: Function to print entities
 * Author:  Karl Abrahamson
 ********************************************************************/

ENTITY raw_show			(ENTITY sense);
long print_ent			(ENTITY e, FILE *where, Boolean in_pair, 
				 int parens, long max_chars, 
				 Boolean *suppress);
void trace_print_entity		(ENTITY e);
long print_entity		(FILE *where, ENTITY e, long max_chars);
long print_entity_with_state	(ENTITY e, TYPE *t, FILE *where,
			     	 STATE *st, TRAP_VEC *tv, Boolean suppress,
			    	 long max_chars);
char*  lazy_name    		(char *result, ENTITY s);
char*  function_name		(ENTITY f);
ENTITY fun_to_str   		(ENTITY f);
ENTITY boxpl_to_str 		(ENTITY x);
ENTITY boxpl_to_str_stdf	(ENTITY e, TYPE *t);
ENTITY lazy_dollar  		(ENTITY x);
char*  get_act_name		(ACTIVATION *a, Boolean moveout);
ENTITY get_dollar   		(TYPE *t, long time_bound);
