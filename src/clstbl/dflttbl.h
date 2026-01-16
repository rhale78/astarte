/****************************************************************
 * File:    clstbl/dflttbl.h
 * Purpose: Managing defaults in the class table.
 * Author:  Karl Abrahamson
 ****************************************************************/

void		default_tm	(CLASS_TABLE_CELL *c, TYPE *t, MODE_TYPE*);
TYPE* 		get_default_tm	(TYPE *V, CLASS_TABLE_CELL *c, 
				 Boolean *dangerous);

