/***********************************************************************
 * File:    evaluate/instinfo.c
 * Purpose: Fill arrays with instruction information.
 * Author:  Karl Abrahamson
 **********************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 1997 Karl Abrahamson					*
 * All rights reserved.							*
 *									*
 * Redistribution and use in source and binary forms, with or without	*
 * modification, are permitted provided that the following conditions	*
 * are met:								*
 *									*
 * 1. Redistributions of source code must retain the above copyright	*
 *    notice, this list of conditions and the following disclaimer.	*
 *									*
 * 2. Redistributions in binary form must reproduce the above copyright	*
 *    notice in the documentation and/or other materials provided with 	*
 *    the distribution.							*
 *									*
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY		*
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE	*
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 	*
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE	*
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 	*
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 	*
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 	*
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,*
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE *
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,    * 
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.			*
 *									*
 ************************************************************************/
#endif

#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../evaluate/instruc.h"
#include "../evaluate/evaluate.h"

/********************************************************
 * This file provides information about the	 	*
 * instructions described in language.doc.  		*
 *							*
 * The informatio includes names, classification and	*
 * number of arguments to print at a trap.		*
 ********************************************************/

/********************************************************
 *		NAME INFORMATION			*
 ********************************************************
 * This information is for the names of the instructions*
 * for printing.					*
 ********************************************************/

/*---------------------------------*
 * Number of regular instructions. *
 *---------------------------------*/

#define INSTR_NAME_SIZE 	LAST_NORMAL_INSTRUCTION + 1

/*-------------------------------*
 * Number of unary instructions. *
 *-------------------------------*/

#define UN_INSTR_NAME_SIZE	N_UNARIES + 1

/*--------------------------------*
 * Number of binary instructions. *
 *--------------------------------*/

#define BIN_INSTR_NAME_SIZE     N_BINARIES +  1

/*-----------------------------*
 * Number prefix instructions. *
 *-----------------------------*/

#define N_PREFIX_INSTRS         6


/********************************************************************
 * The larger arrays are stored in files, and loaded when needed.   *
 ********************************************************************/

char **instr_name, 
     **un_instr_name, 
     **bin_instr_name;
PRIVATE Boolean have_read_instr_names = FALSE;

extern char **prefix_instr_name[];

/*****************************************************************
 * 			READ_INSTR_NAMES			 *
 *****************************************************************
 * Fill the arrays instr_name, un_instr_name, bin_instr_name and *
 * pref_instr_name from files.				   	 *
 *****************************************************************/

void read_instr_names(void) 
{
    if(!have_read_instr_names) {
      char* s = (char*) BAREMALLOC(strlen(MESSAGE_DIR) + 20);
      sprintf(s, "%s%s", MESSAGE_DIR, INSTR_NAME_FILE);
      init_err_msgs(&instr_name, s, INSTR_NAME_SIZE);
      sprintf(s, "%s%s", MESSAGE_DIR, UN_INSTR_NAME_FILE);
      init_err_msgs(&un_instr_name, s, UN_INSTR_NAME_SIZE);
      sprintf(s, "%s%s", MESSAGE_DIR, BIN_INSTR_NAME_FILE);
      init_err_msgs(&bin_instr_name, s, BIN_INSTR_NAME_SIZE);

      prefix_instr_name[1]  = un_instr_name;
      prefix_instr_name[2]  = bin_instr_name;
      have_read_instr_names = TRUE;
      FREE(s);
    }
}

/********************************************************
 * The following are the small arrays of names.		*
 ********************************************************/

char * FARR list1_instr_name[] =
{/* 0 */		"UNKNOWN",
 /* 1 */		"LENGTH",
 /* 2 */		"REVERSE",
 /* 3 */		"PACK",
 /* 4 */		"LAZY-LEFT",
 /* 5 */		"LAZY-RIGHT",
 /* 6 */		"LAZY-HEAD",
 /* 7 */		"LAZY-TAIL",
 /* 8 */		"INTERN-STRING"
};

char * FARR list2_instr_name[] =
{/* 0 */		"UNKNOWN",
 /* 1 */		"APPEND",
 /* 2 */		"DOCMD",
 /* 3 */		"SCAN-FOR"
};

char *ty_instr_name[] =
{/* 0 */		"UNKNOWN",
 /* 1 */		"BOX-TO-STR",
 /* 2 */		"WRAP",
 /* 3 */		"WRAP-NUMBER",
 /* 4 */		"COMPAT"
};

char * FARR unk_instr_name[] =
{/* 0 */                "UNKNOWN",
 /* 1 */		"UNKNOWNQ",
 /* 2 */		"PROTECTED-UNKNOWNQ",
 /* 3 */                "UNPROTECTED-UNKNOWNQ",
 /* 4 */ 		"SAME-UNKNOWN"
};

char ** FARR prefix_instr_name[] =
{/* 0 */		NULL,
 /* 1 */		/*un_instr_name,*/ NULL,
 /* 2 */		/*bin_instr_name,*/ NULL,
 /* 3 */		list1_instr_name,
 /* 4 */		list2_instr_name,
 /* 5 */                ty_instr_name,
 /* 6 */                unk_instr_name
};


/****************************************************************
 *			CLASSIFICATION INFORMATION		*
 ****************************************************************/

/****************************************************************
 * The classifications of instructions are as follows.		*
 *								*
 *   NO_PARAM_INST		This instruction is just one	*
 *				byte.				*
 *								*
 *   NO_TYPE_INST		This instruction is just one	*
 *				byte, and it is a type		*
 *				instruction.			*
 *								*
 *   BYTE_PARAM_INST		This instruction has a single	*
 *				one-byte parameter, making	*
 *				it 2 bytes long.		*
 *								*
 *   BYTE_TYPE_INST		This instruction has a single	*
 *				one-byte parameter, making	*
 *				it 2 bytes long.  This is a	*
 *				type instruction.		*
 *								*
 *   TWO_BYTE_PARAMS_INST	This instruction takes two	*
 *				single-byte parameters, making	*
 *				it 3 bytes long.		*
 *								*
 *   TWO_BYTE_TYPE_INST		This instruction takes two	*
 *				single-byte parameters, making	*
 *				it 3 bytes long.  This is a	*
 *				type instruction.		*
 *								*
 *   LONG_NUM_PARAM_INST	This instruction takes a single	*
 *				3-byte parameter, making	*
 *				the instruction 4 bytes long.	*
 *								*
 *   GLABEL_PARAM_INST		This instruction has a single	*
 *				global label parameter.  So	*
 *				it is 4 bytes long.		*
 *								*
 *   BYTE_GLABEL_INST		This instruction has a global	*
 *				label followed by a single	*
 *				byte.  So it is 5 bytes long.	*
 *								*
 *   GLABEL_TYPE_INST		This instruction has a single	*
 *				global label parameter.  So	*
 *				it is 4 butes long.  This is	*
 *				a type instruction.		*
 *								*
 *   LLABEL_PARAM_INST		This instruction has a single	*
 *				one-byte parameter that is a 	*
 *				local label.  So this instr	*
 *				is 2 bytes long.		*
 *								*
 *   LLABEL_ENV_INST		This instruction has a single	*
 *				one-byte local label parameter,	*
 *				and is followed by an END_LET_I	*
 *				terminated sequence of type-	*
 *				building instructions, and	*
 *				then an environment size byte.	*
 *								*
 *   BYTE_LLABEL_INST		This instruction has a local	*
 *				label (one byte) followed by	*
 *				one byte.  So this instruction	*
 * 				is 3 bytes long.		*
 *								*
 *   PREF_INST			This instruction is a prefix	*
 *				instruction, other than		*
 *				TY_PREF_I.  It has a single   	*
 *				one-byte parameter, the 	*
 *				instruction that goes with it.  *
 *								*
 *   TY_PREF_INST		This instruction is TY_PREF_I.	*
 *				It has a one-byte instruction	*
 *				and a one byte argument, for	*
 *				a total of 3 bytes.		*
 *								*
 *   LET_INST			This instruction is followed	*
 *				by a one-byte offset, an	*
 *				identifier (a null-terminated	*
 *				string), and an END_LET_I-	*
 *				terminated sequence of 		*
 *				type-building instructions.	*
 *								*
 *   RELET_INST			This instruction is one of	*
 *				the RELET instructions.  It is	*
 *				followed by a one-byte offset,	*
 *				and has no internal form.	*
 *								*
 *   DEF_INST			This is DEF_I.			*
 *								*
 *   EXC_INST			This instruction is one of the	*
 *				EXC_ instructions.  It is 	*
 *				followed by a global label	*
 *				where the tag number of the	*
 *				exception is stored.  In 	*
 *				internal form, the global label *
 *				is replaced by the tag number.	*
 *								*
 *   LLABEL_INST		This is LLABEL_I.		*
 *								*
 *   LONG_LLABEL_INST		This is LONG_LLABEL_I.		*
 *								*
 *   LINE_INST			This is LINE_I.			*
 *								*
 *   STOP_G_INST		This is STOP_G_I.		*
 *								*
 *   END_LET_INST		This is END_LET_I.		*
 *								*
 *   ENTER_INST			This is ENTER_I.		*
 *								*
 *   END_INST			This is END_I.			*
 *								*
 ****************************************************************
 *								*
 * Table instinfo gives information about instructions.  Entry	*
 * {c,a,q,0} indicates that					*
 *								*
 *   1. This instruction has class c (one of the classes above.)*
 *      c = 0 indicates a non-instruction.			*
 *								*
 *   2. a is the number of entities on the top of the stack	*
 *      to print when a trap occurs at this instruction.  a	*
 *      is generally the number of operands that this		*
 *      instruction takes.					*
 *								*
 *   3. If q = 1, then this instruction can occur in the test	*
 *      of a quick-try.						*
 *								*
 ****************************************************************/

struct instruction_info FARR instinfo[] =
{
  /*  0 NOOP_I			*/	{NO_PARAM_INST,0,1,0},
  /*  1 UNARY_PREF_I		*/	{PREF_INST, 1,1,0},
  /*  2 BINARY_PREF_I		*/	{PREF_INST, 2,1,0},
  /*  3 LIST1_PREF_I		*/	{PREF_INST, 1,1,0},
  /*  4 LIST2_PREF_I		*/	{PREF_INST, 2,1,0},
  /*  5 TY_PREF_I		*/	{TY_PREF_INST, 1,1,0},
  /*  6 UNK_PREF_I              */	{PREF_INST, 1,1,0},

  /*  7 DUP_I			*/	{NO_PARAM_INST, 1,1,0},
  /*  8 POP_I			*/	{NO_PARAM_INST, 1,1,0},
  /*  9 SWAP_I			*/	{NO_PARAM_INST, 2,1,0},
  /* 10 LOCK_TEMP_I		*/	{NO_PARAM_INST, 1,1,0},
  /* 11 GET_TEMP_I		*/	{NO_PARAM_INST, 0,1,0},

  /* 12 EMPTY_BOX_I     	*/	{NO_PARAM_INST, 1,1,0},
  /* 13 BOX_I			*/	{NO_PARAM_INST, 0,1,0},
  /* 14 PLACE_I			*/	{NO_PARAM_INST, 0,1,0},
  /* 15 ASSIGN_I		*/	{NO_PARAM_INST, 2,1,0},
  /* 16 ASSIGN_INIT_I		*/	{NO_PARAM_INST, 2,1,0},
  /* 17 ASSIGN_NODEMON_I	*/	{NO_PARAM_INST, 2,1,0},
  /* 18 MAKE_EMPTY_I		*/	{NO_PARAM_INST, 1,1,0},
  /* 19 MAKE_EMPTY_NODEMON_I	*/	{NO_PARAM_INST, 1,1,0},

  /* 20 SMALL_INT_I		*/	{BYTE_PARAM_INST, 0,1,0},
  /* 21 CONST_I                 */	{GLABEL_PARAM_INST, 0,1,0},
  /* 22 ZERO_I			*/	{NO_PARAM_INST, 0,1,0},
  /* 23 TRUE_I			*/	{NO_PARAM_INST, 0,1,0},
  /* 24 STD_BOX_I		*/	{BYTE_PARAM_INST, 0,1,0},

  /* 25 INT_DIVIDE_I		*/	{NO_PARAM_INST, 2,1,0},
  /* 26 AS_I			*/	{BYTE_PARAM_INST, 2,1,0},

  /* 27 NOT_I			*/	{NO_PARAM_INST, 1,1,0},
  /* 28 			*/	{0,0,0,0},
  /* 29 EQ_I			*/	{NO_PARAM_INST, 2,1,0},
  /* 30 			*/	{0,0,0,0},
  /* 31 ENUM_CHECK_I		*/	{BYTE_PARAM_INST, 0,1,0},
  /* 32 LONG_ENUM_CHECK_I	*/	{LONG_NUM_PARAM_INST, 1,1,0},
  /* 33 TEST_I			*/	{BYTE_PARAM_INST, 1,1,0},

  /* 34 NILQ_I			*/	{NO_PARAM_INST, 1,1,0},
  /* 35 NIL_FORCE_I		*/	{NO_PARAM_INST, 1,1,0},
  /* 36 PAIR_I			*/	{NO_PARAM_INST, 2,1,0},
  /* 37 MULTIPAIR_I		*/	{BYTE_PARAM_INST, 2,1,0},
  /* 38 HEAD_I			*/	{NO_PARAM_INST, 1,1,0},
  /* 39 TAIL_I			*/	{NO_PARAM_INST, 1,1,0},
  /* 40 LEFT_I			*/	{NO_PARAM_INST, 1,1,0},
  /* 41 RIGHT_I                 */	{NO_PARAM_INST, 1,1,0},
  /* 42 MULTI_RIGHT_I		*/	{BYTE_PARAM_INST, 1,1,0},
  /* 43 SPLIT_I			*/	{NO_PARAM_INST, 1,1,0},
  /* 44 NONNIL_FORCE_I		*/	{NO_PARAM_INST, 1,1,0},
  /* 45 NONNIL_TEST_I		*/	{NO_PARAM_INST, 1,1,0},
  /* 46 SUBLIST_I		*/	{NO_PARAM_INST, 2,1,0},
  /* 47 SUBSCRIPT_I		*/	{NO_PARAM_INST, 2,1,0},
  /* 48 			*/	{0, 0,0,0},

  /* 49				*/	{0,0,0,0},

  /* 50 FUNCTION_I		*/	{LLABEL_ENV_INST, 0,0,0},
  /* 51 REV_APPLY_I 		*/	{NO_PARAM_INST, 2,0,0},
  /* 52 REV_TAIL_APPLY_I	*/	{NO_PARAM_INST, 2,0,0},
  /* 53 REV_PAIR_APPLY_I	*/	{BYTE_PARAM_INST, 2,0,0},
  /* 54 REV_PAIR_TAIL_APPLY_I   */	{BYTE_PARAM_INST, 2,0,0},
  /* 55 QEQ_APPLY_I		*/	{NO_PARAM_INST, 2,0,0},
  /* 56 SHORT_APPLY_I		*/	{NO_PARAM_INST, 2,0,0},
  /* 57 RETURN_I		*/	{NO_PARAM_INST, 1,0,0},
  /* 58 INVISIBLE_RETURN_I      */	{NO_PARAM_INST, 1,0,0},

  /* 59				*/	{0,0,0,0},
  /* 60				*/	{0,0,0,0},
  /* 61				*/	{0,0,0,0},

  /* 62 DYN_FETCH_GLOBAL_I	*/	{BYTE_GLABEL_INST, 1,0,0},
  /* 63 G_FETCH_I        	*/	{BYTE_PARAM_INST, 0,1,0},
  /* 64 GLOBAL_ID_VAL_I		*/	{BYTE_PARAM_INST, 1,1,0},

  /* 65 FETCH_I			*/	{BYTE_PARAM_INST, 0,1,0},
  /* 66 FINAL_FETCH_I		*/	{BYTE_PARAM_INST, 0,1,0},
  /* 67 LONG_FETCH_I		*/	{TWO_BYTE_PARAMS_INST, 0,1,0},
  /* 68 FINAL_LONG_FETCH_I      */	{TWO_BYTE_PARAMS_INST, 0,1,0},
  /* 69 EXIT_SCOPE_I		*/	{BYTE_PARAM_INST, 0,1,0},
  /* 70 DEF_I			*/	{DEF_INST, 0,0,0},
  /* 71 TEAM_I			*/	{TWO_BYTE_PARAMS_INST, 0,0,0},
  /* 72 LET_I			*/	{LET_INST, 1,1,0},
  /* 73 DEAD_LET_I		*/	{LET_INST, 1,1,0},
  /* 74 LET_AND_LEAVE_I		*/	{LET_INST, 1,1,0},
  /* 75 FINAL_LET_AND_LEAVE_I   */	{LET_INST, 1,1,0},
  /* 76 RELET_I			*/	{RELET_INST, 1,1,0},
  /* 77 RELET_AND_LEAVE_I	*/	{RELET_INST, 1,1,0},
  /* 78 FINAL_RELET_AND_LEAVE_I */	{RELET_INST, 1,1,0},

  /* 79 GOTO_I			*/	{LLABEL_PARAM_INST, 0,1,0},
  /* 80 PAIR_GOTO_I		*/	{BYTE_LLABEL_INST, 0,1,0},
  /* 81 GOTO_IF_FALSE_I		*/	{LLABEL_PARAM_INST, 1,1,0},
  /* 82 GOTO_IF_NIL_I           */	{LLABEL_PARAM_INST, 1,1,0},
  /* 83 AND_SKIP_I		*/	{LLABEL_PARAM_INST, 1,1,0},
  /* 84 OR_SKIP_I		*/	{LLABEL_PARAM_INST, 1,1,0},
  /* 85 LLABEL_I		*/	{LLABEL_INST, 0,1,0},
  /* 86 LONG_LLABEL_I		*/	{LONG_LLABEL_INST, 0,1,0},
  /* 87 TRY_I			*/	{BYTE_LLABEL_INST, 0,1,0},
  /* 88 QUICK_TRY_I		*/	{LLABEL_PARAM_INST, 0,1,0},
  /* 89 THEN_I			*/	{NO_PARAM_INST, 0,0,0},
  /* 90 			*/	{0, 0,0,0},

  /* 91 PUSH_EXC_I		*/	{NO_PARAM_INST, 0,0,0},
  /* 92 POP_EXC_I		*/	{NO_PARAM_INST, 0,0,0},
  /* 93 EXCEPTION_I		*/	{NO_PARAM_INST, 0,0,0},
  /* 94 STREAM_I		*/	{LLABEL_PARAM_INST, 0,0,0},
  /* 95 MIX_I               	*/	{LLABEL_PARAM_INST, 0,0,0},
  /* 96 FAIL_I			*/	{NO_PARAM_INST, 1,1,0},
  /* 97 FAILC_I			*/	{BYTE_PARAM_INST, 0,1,0},

  /* 98 BEGIN_CUT_I		*/	{NO_PARAM_INST, 0,0,0},
  /* 99 END_CUT_I		*/	{NO_PARAM_INST, 0,0,0},
  /* 100 CUT_I			*/	{NO_PARAM_INST, 0,0,0},
  /* 101 BEGIN_PRESERVE_I	*/	{NO_PARAM_INST, 0,0,0},
  /* 102 END_PRESERVE_I		*/	{NO_PARAM_INST, 0,0,0},

  /* 103 PUSH_TRAP_I		*/	{NO_PARAM_INST, 0,0,0},
  /* 104 POP_TRAP_I		*/	{NO_PARAM_INST, 0,0,0},
  /* 105 TRAP_I			*/	{NO_PARAM_INST, 1,1,0},
  /* 106 UNTRAP_I		*/	{NO_PARAM_INST, 1,1,0},
  /* 107 ALL_TRAP_I		*/	{NO_PARAM_INST, 0,1,0},

  /* 108 PAUSE_I		*/ 	{NO_PARAM_INST, 0,0,0},
  /* 109 REPAUSE_I		*/	{NO_PARAM_INST, 0,0,0},
  /* 110 BEGIN_ATOMIC_I		*/	{NO_PARAM_INST, 0,1,0},
  /* 111 END_ATOMIC_I		*/	{NO_PARAM_INST, 0,1,0},

  /* 112			*/	{0,0,0,0},
  /* 113			*/	{0,0,0,0},

  /* 114 BIND_UNKNOWN_I         */	{BYTE_PARAM_INST, 2,1,0},

  /* 115 TEST_LAZY1_I		*/	{NO_PARAM_INST, 1,1,0},
  /* 116 TEST_LAZY_I		*/	{NO_PARAM_INST, 2,1,0},
  /* 117 LAZY_I			*/	{LLABEL_ENV_INST, 0,0,0},
  /* 118 LAZY_RECOMPUTE_I	*/	{LLABEL_ENV_INST, 0,0,0},
  /* 119 LAZY_LIST_I		*/	{LLABEL_ENV_INST, 0,0,0},
  /* 120 LAZY_LIST_RECOMPUTE_I	*/	{LLABEL_ENV_INST, 0,0,0},

  /* 121 NAME_I			*/	{GLABEL_PARAM_INST, 0,1,0},
  /* 122 SNAME_I		*/	{GLABEL_PARAM_INST, 0,1,0},

  /* 123 EXC_CONST_I		*/	{EXC_INST, 0,1,0},
  /* 124 GET_TYPE_I		*/	{BYTE_PARAM_INST, 1,0,0},
  /* 125 WRAP_I			*/	{NO_PARAM_INST, 1,1,0},
  /* 126 UNWRAP_I		*/	{NO_PARAM_INST, 1,1,0},
  /* 127 QWRAP_I		*/	{BYTE_PARAM_INST, 1,1,0},
  /* 128 EXC_WRAP_I		*/	{EXC_INST, 1,1,0},
  /* 129 QUNWRAP_I		*/	{BYTE_PARAM_INST, 1,1,0},
  /* 130 EXC_UNWRAP_I		*/	{EXC_INST, 1,1,0},
  /* 131 QTEST_I                */	{BYTE_PARAM_INST, 1,1,0},
  /* 132 EXC_TEST_I		*/	{EXC_INST, 1,1,0},
  /* 133 UNIFY_T_I		*/	{NO_PARAM_INST, 0,1,0},
  /* 134 PUSH_TYPE_BINDINGS_I	*/	{NO_PARAM_INST, 1,1,0},
  /* 135 POP_TYPE_BINDINGS_I	*/	{NO_PARAM_INST, 1,1,0},
  /* 136 COMMIT_TYPE_BINDINGS_I	*/	{NO_PARAM_INST, 1,1,0},
  /* 137    			*/	{0,0,0,0},
  /* 138 			*/	{0,0,0,0},

  /* 139 COROUTINE_I		*/	{BYTE_LLABEL_INST, 0,0,0},
  /* 140 RESUME_I		*/	{NO_PARAM_INST, 0,0,0},
  /* 141 CHECK_DONE_I		*/	{BYTE_LLABEL_INST, 0,0,0},
  /* 142 STORE_INDIRECT_I	*/	{BYTE_LLABEL_INST, 1,0,0},

  /* 143 LAZY_DOLLAR_I		*/	{NO_PARAM_INST, 1,1,0},
  /* 144	                */	{0, 0,0,0},
  /* 145 BEGIN_VIEW_I		*/	{NO_PARAM_INST, 0,1,0},
  /* 146 END_VIEW_I		*/	{NO_PARAM_INST, 0,1,0},
  /* 147 RAW_SHOW_I		*/	{NO_PARAM_INST, 2,0,0},
  /* 148 LINE_I			*/	{LINE_INST, 0,1,0},
  /* 149 FPRINT_I		*/	{NO_PARAM_INST, 2,1,0},
  /* 150 TOPFORCE_I		*/	{NO_PARAM_INST, 1,1,0},
  /* 151 SPECIES_AS_VAL_I	*/	{NO_PARAM_INST, 0,1,0},
  /* 152        		*/	{0,0,0,0},

  /* 153 STAND_T_I		*/	{BYTE_TYPE_INST, 0,1,0},
  /* 154 STAND_WRAP_T_I		*/	{BYTE_TYPE_INST, 0,1,0},
  /* 155 STAND_VAR_T_I		*/	{BYTE_TYPE_INST, 0,1,0},
  /* 156 STAND_PRIMARY_VAR_T_I	*/	{BYTE_TYPE_INST, 0,1,0},
  /* 157 STAND_WRAP_VAR_T_I	*/	{BYTE_TYPE_INST, 0,1,0},
  /* 158 TYPE_ID_T_I		*/	{GLABEL_TYPE_INST, 0,1,0},
  /* 159 TYPE_VAR_T_I		*/	{GLABEL_TYPE_INST, 0,1,0},
  /* 160 PRIMARY_TYPE_VAR_T_I	*/	{GLABEL_TYPE_INST, 0,1,0},
  /* 161 WRAP_TYPE_ID_T_I	*/	{GLABEL_TYPE_INST, 0,1,0},
  /* 162 WRAP_TYPE_VAR_T_I	*/	{GLABEL_TYPE_INST, 0,1,0},
  /* 163 CONSTRAIN_T_I		*/	{NO_TYPE_INST, 0,1,0},
  /* 164 FUNCTION_T_I		*/	{NO_TYPE_INST, 0,1,0},
  /* 165 PAIR_T_I		*/	{NO_TYPE_INST, 0,1,0},
  /* 166 LIST_T_I		*/	{NO_TYPE_INST, 0,1,0},
  /* 167 BOX_T_I		*/	{NO_TYPE_INST, 0,1,0},
  /* 168 			*/	{0,0,0,0},
  /* 169 FAM_MEM_T		*/	{NO_TYPE_INST, 0,1,0},
  /* 170 UNPAIR_T_I		*/	{NO_TYPE_INST, 0,1,0},
  /* 171 HEAD_T_I		*/	{NO_TYPE_INST, 0,1,0},
  /* 172 TAIL_T_I		*/	{NO_TYPE_INST, 0,1,0},
  /* 173 POP_T_I		*/	{NO_TYPE_INST, 0,1,0},
  /* 174 SWAP_T_I		*/	{NO_TYPE_INST, 0,1,0},
  /* 175 CLEAR_STORAGE_T_I      */	{NO_TYPE_INST, 0,1,0},
  /* 176 STORE_T_I		*/	{NO_TYPE_INST, 0,1,0},
  /* 177 STORE_AND_LEAVE_T_I 	*/	{NO_TYPE_INST, 0,1,0},
  /* 178 GET_T_I		*/	{BYTE_TYPE_INST, 0,1,0},
  /* 179 			*/	{0,0,0,0},
  /* 180    			*/	{0,0,0,0},
  /* 181 GLOBAL_FETCH_T_I	*/	{BYTE_TYPE_INST, 0,1,0},
  /* 182 			*/	{0,0,0,0},

  /* 183 TYPE_ONLY_I	 	*/	{NO_PARAM_INST, 0,0,0},
  /* 184 GET_GLOBAL_I		*/	{GLABEL_PARAM_INST, 0,0,0},
  /* 185 STOP_G_I		*/	{STOP_G_INST,0,0,0},

  /* 186			*/	{0,0,0,0},
  /* 187			*/	{0,0,0,0},
  /* 188			*/	{0,0,0,0},
  /* 189			*/	{0,0,0,0},
  /* 190			*/	{0,0,0,0},
  /* 191			*/	{0,0,0,0},
  /* 192			*/	{0,0,0,0},
  /* 193			*/	{0,0,0,0},
  /* 194			*/	{0,0,0,0},
  /* 195			*/	{0,0,0,0},
  /* 196			*/	{0,0,0,0},
  /* 197			*/	{0,0,0,0},
  /* 198			*/	{0,0,0,0},
  /* 199			*/	{0,0,0,0},
  /* 200			*/	{0,0,0,0},
  /* 201			*/	{0,0,0,0},
  /* 202			*/	{0,0,0,0},
  /* 203			*/	{0,0,0,0},
  /* 204			*/	{0,0,0,0},
  /* 205			*/	{0,0,0,0},
  /* 206			*/	{0,0,0,0},
  /* 207			*/	{0,0,0,0},
  /* 208			*/	{0,0,0,0},
  /* 209			*/	{0,0,0,0},
  /* 210			*/	{0,0,0,0},
  /* 211			*/	{0,0,0,0},
  /* 212			*/	{0,0,0,0},
  /* 213			*/	{0,0,0,0},
  /* 214			*/	{0,0,0,0},
  /* 215			*/	{0,0,0,0},
  /* 216			*/	{0,0,0,0},
  /* 217			*/	{0,0,0,0},
  /* 218			*/	{0,0,0,0},
  /* 219			*/	{0,0,0,0},
  /* 220			*/	{0,0,0,0},
  /* 221			*/	{0,0,0,0},
  /* 222			*/	{0,0,0,0},
  /* 223			*/	{0,0,0,0},
  /* 224			*/	{0,0,0,0},
  /* 225			*/	{0,0,0,0},
  /* 226			*/	{0,0,0,0},
  /* 227			*/	{0,0,0,0},
  /* 228			*/	{0,0,0,0},
  /* 229			*/	{0,0,0,0},
  /* 230			*/	{0,0,0,0},
  /* 231			*/	{0,0,0,0},
  /* 232			*/	{0,0,0,0},
  /* 233			*/	{0,0,0,0},
  /* 234			*/	{0,0,0,0},
  /* 235			*/	{0,0,0,0},
  /* 236			*/	{0,0,0,0},
  /* 237			*/	{0,0,0,0},
  /* 238			*/	{0,0,0,0},
  /* 239			*/	{0,0,0,0},
  /* 240			*/	{0,0,0,0},
  /* 241			*/	{0,0,0,0},
  /* 242			*/	{0,0,0,0},
  /* 243			*/	{0,0,0,0},
  /* 244			*/	{0,0,0,0},
  /* 245			*/	{0,0,0,0},
  /* 246			*/	{0,0,0,0},
  /* 247			*/	{0,0,0,0},
  /* 248			*/	{0,0,0,0},
  /* 249			*/	{0,0,0,0},
  /* 250			*/	{0,0,0,0},
  /* 251			*/	{0,0,0,0},
  /* 252			*/	{0,0,0,0},
  /* 253			*/	{END_LET_INST,0,1,0},
  /* 254			*/	{ENTER_INST,0,0,0},
  /* 255			*/	{END_INST,0,0,0},
};



