/****************************************************************
 * File:    clstbl/m_dflt.h
 * Purpose: Managing defaults for the interpreter.
 * Author:  Karl Abrahamson
 ****************************************************************/

void  install_runtime_default_tm(CLASS_TABLE_CELL* ctc, TYPE *dflt, 
				 struct pack_params *packg);
TYPE* get_runtime_default_tm	(CLASS_TABLE_CELL* ctc, UBYTE *pc);
TYPE* dynamic_default		(TYPE *V);

