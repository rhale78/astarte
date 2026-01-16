/****************************************************************
 * File:    clstbl/cnstrlst.h
 * Purpose: Handling of constructor lists.
 * Author:  Karl Abrahamson
 ****************************************************************/

int		num_type_tags      (CLASS_TABLE_CELL *c);
int		tag_name_to_tag_num(TYPE *t, char *constr_name);
char*		tag_num_to_tag_name(TYPE *t, int n);

