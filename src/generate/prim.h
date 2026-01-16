/*****************************************************************
 * File:    generate/prim.h
 * Purpose: Primitives and utilities for them.
 * Author:  Karl Abrahamson
 *****************************************************************/

/****************************************************************
 * 			PRIMITIVE KINDS		  		*
 ****************************************************************/

/*******************************************************************
 * See invert_prim[] in prim.c if modifying these. 		   *
 *								   *
 * All function primitives must come first. LAST_FUN_PRIM must be  *
 * the number of the last function primitive.  Following function  *
 * primitives come non-function entity primitives.  LAST_ENT_PRIM  *
 * must be the last of those. 					   *
 *								   *
 * The primitives between FIRST_TY_PRIM and LAST_TY_PRIM are those *
 * that require a type parameter.				   *
 *								   *
 * generate/prim.c assumes that the invertible primitive are all   *
 * less than or equal to LAST_TY_PRIM.  That file needs to be      *
 * changed whenever a new primitive is added below LAST_TY_PRIM.   *
 *								   *
 * PRIM_STACK, etc. are used to indicate kinds of expressions. They*
 * are placed here because they should have different values from  *
 * the true primitives.						   *
 *******************************************************************/

#define PRIM_CAST		1
#define PRIM_ENUM_CAST  	2
#define PRIM_LONG_ENUM_CAST 	3
#define PRIM_SPECIES		4
#define PRIM_QWRAP		5
#define PRIM_QUNWRAP		6
#define PRIM_QTEST      	7
#define PRIM_DWRAP      	8
#define PRIM_DUNWRAP    	9
#define PRIM_EXC_WRAP   	10
#define PRIM_EXC_UNWRAP 	11
#define PRIM_EXC_TEST   	12

#define FIRST_TY_PRIM		13		
#define PRIM_WRAP		13
#define PRIM_UNWRAP		14
#define TY_PRIM_FUN		15
#define TY_PRIM_OP		16
#define TY_FUN			17
#define LAST_TY_PRIM		17

#define PRIM_FUN		18
#define PRIM_OP			19
#define HERM_FUN		20
#define STD_FUN			21
#define STD_OP			22
#define LIST_FUN		23
#define LIST_OP			24
#define PRIM_QEQ        	25
#define PRIM_NOT_OP		26
#define PRIM_SELECT		27
#define PRIM_MODIFY		28
#define PRIM_BIND_UNK   	29
#define UNK_FUN         	30
#define LAST_FUN_PRIM   	30

#define PRIM_CONST		32
#define STD_CONST		33
#define STD_BOX         	35
#define EXCEPTION_CONST 	36
#define LAST_ENT_PRIM		36

#define PRIM_STACK		34
#define PRIM_EXCEPTION  	37
#define PRIM_TARGET		38
#define PAT_FUN			39
#define PAT_CONST		40
#define COLLAPSE		41

/***************************** From prim.c *************************/

int 		invert_prim_val		(int prim) ;
void		gen_prim_g		(int prim, int instr, EXPR *e1);
Boolean 	gen_ent_prim_g		(int prim, int instr);
int		get_prim_g		(EXPR *e, int *instr, ENTPART **part,
					 Boolean *irregular, 
					 INT_LIST **selection_list);
int		get_pat_prim_g		(EXPR *e, int *instr, PART **part,
					 Boolean *irregular, 
					 INT_LIST **selection_list);
void	 	gen_special_g		(EXPR *e);
