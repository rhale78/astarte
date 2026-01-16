/*****************************************************************
 * File:    generate/selmod.h
 * Purpose: Support for generating and translating role
 *	    selection and modification.
 * Author:  Karl Abrahamson
 *****************************************************************/

EXPR* 	   get_role_select_structure	(EXPR *e, TYPE *image_type);
LIST* 	   get_modify_structure		(EXPR *e, EXPR **start_expr, 
					 int *the_discr);
void  	   gen_selection_g		(int discr, EXPR *f);
void       gen_modify_g			(int discr, EXPR *f);
void       gen_role_modify_g		(EXPR *e);

