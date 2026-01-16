/****************************************************************
 * File:    clstbl/stdtytbl.h
 * Purpose: Functions to handle inserting of standard types, etc.
 * Author:  Karl Abrahamson
 ****************************************************************/

CLASS_TABLE_CELL* standard_type   (TYPE **t, int sym);
CLASS_TABLE_CELL* standard_fam    (TYPE **t, int sym, TYPE *arg,
				   Boolean opaque);
CLASS_TABLE_CELL* standard_tf	  (TYPE **t, char *name, TYPE *arg,
				   Boolean opaque);
CLASS_TABLE_CELL* standard_genus  (int sym, TYPE *dfault, TYPE *mem, 
				   int extensible);
CLASS_TABLE_CELL* standard_comm   (int sym, TYPE *dfault, TYPE *mem,
				   int extensible, Boolean opaque);
CLASS_TABLE_CELL* standard_constr (int code, char *name);


