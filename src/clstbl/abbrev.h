/****************************************************************
 * File:    clstbl/abbrev.h
 * Purpose: Handle table of abbreviations
 * Author:  Karl Abrahamson
 ****************************************************************/

void 		  abbrev_tm	(char *id, int code, TYPE *t, ROLE *r,
				 Boolean global);
CLASS_TABLE_CELL* get_abbrev_tm (HASH_KEY u, LONG hash);

