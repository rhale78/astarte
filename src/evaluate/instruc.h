/*********************************************************************
 * File:    evaluate/instruc.h
 * Purpose: Define instruction numbers
 * Author:  Karl Abrahamson
 *********************************************************************/

/************************************************************************
 *			IMPORTANT NOTE					*
 ************************************************************************
 * Any time the code is changed in a way that is incompatible with 	*
 * the previous version, update BYTE_CODE_VERSION.		   	*
 ************************************************************************/

#define BYTE_CODE_VERSION 14

extern struct instruction_info instinfo[];

/****************************************************************
 * The meaning of instructions is described in language.doc.	*
 * If you want to add instructions, see ADDING INSTRUCTIONS in 	*
 * language.doc. 						*
 ****************************************************************/

/*--------------------------------------------------------------*
 *			Declaration Instructions		*
 *--------------------------------------------------------------*/

#define STOP_PACKAGE_I			1
#define ID_DCL_I			2
#define LABEL_DCL_I			3
#define LONG_LABEL_DCL_I 		4
#define BEGIN_IMPLEMENTATION_DCL_I  	5
#define STRING_DCL_I			6
#define INT_DCL_I			7
#define REAL_DCL_I			8
#define NEW_SPECIES_DCL_I		9
#define NEW_TRANSPARENT_FAMILY_DCL_I	10
#define NEW_OPAQUE_FAMILY_DCL_I		11
#define NEW_GENUS_DCL_I			12
#define NEW_TRANSPARENT_COMMUNITY_DCL_I	13
#define NEW_OPAQUE_COMMUNITY_DCL_I	14
#define SPECIES_DCL_I			15
#define FAMILY_DCL_I			16
#define GENUS_DCL_I			17
#define COMMUNITY_DCL_I			18
#define RELATE_DCL_I			19
#define MEET_DCL_I			20
#define BEGIN_EXTENSION_DCL_I		21
#define END_EXTENSION_DCL_I		22
#define IMPORT_I			23
#define EXECUTE_I			24
#define EXCEPTION_DCL_I			25
#define IRREGULAR_DCL_I			26
#define DEFINE_DCL_I			27
#define DEFAULT_DCL_I			28
#define AHEAD_MEET_DCL_I		29
#define NAME_DCL_I			30
#define HIDDEN_EXECUTE_I		31

#define END_LET_I			253
#define ENTER_I				254
#define END_I				255

/*--------------------------------------------------------------*
 *			Executable Instructions			*
 *--------------------------------------------------------------*/

/*---------------------*
 * NOOP instruction    *
 *---------------------*/

#define NOOP_I			0

/*---------------------*
 * Prefix Instructions *
 *---------------------*/

#define UNARY_PREF_I		1
#define BINARY_PREF_I		2
#define LIST1_PREF_I		3
#define LIST2_PREF_I		4
#define TY_PREF_I		5
#define UNK_PREF_I              6

/*--------------------*
 * Stack manipulation *
 *--------------------*/

#define DUP_I			7
#define POP_I			8
#define SWAP_I			9
#define LOCK_TEMP_I		10
#define GET_TEMP_I		11

/*----------------*
 * Box primitives *
 *----------------*/

#define EMPTY_BOX_I     	12
#define BOX_I			13
#define PLACE_I			14
#define ASSIGN_I		15
#define ASSIGN_INIT_I		16
#define ASSIGN_NODEMON_I	17
#define MAKE_EMPTY_I		18
#define MAKE_EMPTY_NODEMON_I	19

/*-----------*
 * Constants *
 *-----------*/

#define SMALL_INT_I		20
#define CONST_I                 21
#define HERMIT_I		ZERO_I
#define CHAR_I			SMALL_INT_I
#define ZERO_I			22
#define TRUE_I			23
#define STD_BOX_I		24

/*---------*
 * Numbers *
 *---------*/

#define INT_DIVIDE_I		25
#define AS_I			26

/*--------------------------------*
 * Boolean, tests and comparisons *
 *--------------------------------*/

#define NOT_I			27
#define EQ_I			29
#define ENUM_CHECK_I		31
#define LONG_ENUM_CHECK_I	32
#define TEST_I			33

/*-----------------*
 * Products, lists *
 *-----------------*/

#define NIL_I			ZERO_I
#define NILQ_I			34
#define NIL_FORCE_I		35
#define PAIR_I			36
#define MULTIPAIR_I		37
#define HEAD_I			38
#define TAIL_I			39
#define LEFT_I			40
#define RIGHT_I                 41
#define MULTI_RIGHT_I		42
#define SPLIT_I			43
#define NONNIL_FORCE_I		44
#define NONNIL_TEST_I		45
#define SUBLIST_I		46
#define SUBSCRIPT_I		47

/*---------------------------------------------*
 * Function, application and return primitives *
 *---------------------------------------------*/

#define FUNCTION_I		50

/*-----------------------------------------------------------*
 * Note: Do not reorder REV_APPLY_I to REV_PAIR_TAIL_APPLY_I *
 *-----------------------------------------------------------*/

#define REV_APPLY_I 		51
#define REV_TAIL_APPLY_I	52
#define REV_PAIR_APPLY_I	53
#define REV_PAIR_TAIL_APPLY_I   54
#define QEQ_APPLY_I		55
#define SHORT_APPLY_I		56
#define RETURN_I		57
#define INVISIBLE_RETURN_I      58

/*--------------------*
 * Global environment *
 *--------------------*/

#define DYN_FETCH_GLOBAL_I	62
#define G_FETCH_I        	63
#define GLOBAL_ID_VAL_I		64

/*-------------------*
 * Local environment *
 *-------------------*/

#define FETCH_I			65
#define FINAL_FETCH_I		66
#define LONG_FETCH_I		67
#define FINAL_LONG_FETCH_I      68
#define EXIT_SCOPE_I		69
#define DEF_I			70
#define TEAM_I			71
#define LET_I			72
#define DEAD_LET_I		73
#define LET_AND_LEAVE_I		74
#define FINAL_LET_AND_LEAVE_I   75
#define RELET_I			76
#define RELET_AND_LEAVE_I	77
#define FINAL_RELET_AND_LEAVE_I 78

/*-------------------------------*
 * Conditional, try, goto, label *
 *-------------------------------*/

#define GOTO_I			79
#define PAIR_GOTO_I		80
#define GOTO_IF_FALSE_I		81
#define GOTO_IF_NIL_I           82
#define AND_SKIP_I		83
#define OR_SKIP_I		84	/* Must be AND_SKIP + 1 */
#define LLABEL_I		85
#define LONG_LLABEL_I		86
#define TRY_I			87
#define QUICK_TRY_I		88
#define THEN_I			89
#define QUICK_THEN_I		THEN_I

/*------------------------------*
 * Failure, exceptions, streams *
 *------------------------------*/

#define PUSH_EXC_I		91
#define POP_EXC_I		92
#define EXCEPTION_I		93
#define STREAM_I		94
#define MIX_I               	95
#define FAIL_I			96
#define FAILC_I			97

/*------------------------*
 * Chopping and filtering *
 *------------------------*/

#define BEGIN_CUT_I		98
#define END_CUT_I		99
#define CUT_I			100
#define BEGIN_PRESERVE_I	101
#define END_PRESERVE_I		102

/*----------*
 * Trapping *
 *----------*/

#define PUSH_TRAP_I		103
#define POP_TRAP_I		104
#define TRAP_I			105
#define UNTRAP_I		106
#define ALL_TRAP_I		107

/*---------*
 * Threads *
 *---------*/

#define REPAUSE_I		108
#define PAUSE_I			109
#define BEGIN_ATOMIC_I		110
#define END_ATOMIC_I		111

/*----------*
 * Unknowns *
 *----------*/

#define BIND_UNKNOWN_I          114

/*-------------------*
 * Lazy instructions *
 *-------------------*/

#define TEST_LAZY1_I		115
#define TEST_LAZY_I		116
#define LAZY_I			117
#define LAZY_RECOMPUTE_I	118
#define LAZY_LIST_I		119
#define LAZY_LIST_RECOMPUTE_I	120

/*-----------*
 * Debugging *
 *-----------*/

#define NAME_I			121
#define SNAME_I			122

/*------------------------------------------------------*
 * Tagging of values, and run-time type checking.	*
 * NOTE: evaluate.c assumes that			*
 * QUNWRAP_I < EXC_UNWRAP_I < QTEST_I < EXC_TEST_I 	*
 *------------------------------------------------------*/

#define EXC_CONST_I		123
#define GET_TYPE_I		124
#define WRAP_I			125
#define UNWRAP_I		126
#define QWRAP_I			127
#define EXC_WRAP_I		128
#define QUNWRAP_I		129
#define EXC_UNWRAP_I		130
#define QTEST_I                 131
#define EXC_TEST_I		132
#define UNIFY_T_I		133
#define PUSH_TYPE_BINDINGS_I	134
#define POP_TYPE_BINDINGS_I	135
#define COMMIT_TYPE_BINDINGS_I	136

/*-------------*
 * Couroutines *
 *-------------*/

#define COROUTINE_I		139
#define RESUME_I		140
#define CHECK_DONE_I		141
#define STORE_INDIRECT_I	142

/*---------------*
 * Miscellaneous *
 *---------------*/

#define LAZY_DOLLAR_I		143
#define BEGIN_VIEW_I		145
#define END_VIEW_I		146
#define RAW_SHOW_I		147
#define LINE_I			148
#define FPRINT_I		149
#define TOPFORCE_I		150
#define SPECIES_AS_VAL_I	151

/*--------------------------------------------------------------*
 * Type building instructions.	If one of these is added, see	*
 * TYPE_INSTRUCTIONS below.  Also, be careful about reordering, *
 * since the smallest and largest are used below.		*
 *--------------------------------------------------------------*/

#define STAND_T_I		153
#define STAND_WRAP_T_I		154
#define STAND_VAR_T_I		155
#define STAND_PRIMARY_VAR_T_I	156
#define STAND_WRAP_VAR_T_I	157
#define TYPE_ID_T_I		158
#define TYPE_VAR_T_I		159
#define PRIMARY_TYPE_VAR_T_I	160
#define WRAP_TYPE_ID_T_I	161
#define WRAP_TYPE_VAR_T_I	162
#define CONSTRAIN_T_I		163
#define FUNCTION_T_I		164
#define PAIR_T_I		165
#define LIST_T_I		166
#define BOX_T_I			167
#define FAM_MEM_T_I		169
#define UNPAIR_T_I		170
#define HEAD_T_I		171
#define TAIL_T_I		172
#define POP_T_I			173
#define SWAP_T_I		174
#define CLEAR_STORAGE_T_I       175
#define STORE_T_I		176
#define STORE_AND_LEAVE_T_I 	177
#define GET_T_I			178
#define SECONDARY_DEFAULT_T_I	179
#define GLOBAL_FETCH_T_I	181

/*-------------------------------------------*
 * Global environment building instructions. *
 *-------------------------------------------*/

#define TYPE_ONLY_I		183
#define GET_GLOBAL_I		184
#define STOP_G_I		185

#define LAST_NORMAL_INSTRUCTION 185   /* Number of last normal instruction */

/*--------------------------------------------------------------*
 *  		Operations that follow UNARY_PREF_I  		*
 ****************************************************************
 * These are unary functions that have their argument fully	*
 * evaluated. 							*
 *--------------------------------------------------------------*/

/*-------*
 * Boxes *
 *-------*/

#define CONTENT_STDF		1
#define CONTENT_TEST_STDF       2
#define PRCONTENT_STDF		3
#define MAKE_ARRAY_STDF 	4
#define MAKE_PARRAY_STDF	5
#define RANK_BOX_STDF		6
#define FLAVOR_STDF		7
#define BOXFLAVOR_TO_STRING_STDF 8
#define COPYFLAVOR_TO_STRING_STDF 9


/*---------*
 * Numbers *
 *---------*/

#define NAT_TO_HEX_STDF		10
#define ZEROP_STDF		11
#define NEGATE_STDF		12
#define SQRT_STDF		13
#define FLOOR_STDF		14
#define CEILING_STDF		15
#define NATURAL_STDF		16
#define INTEGER_STDF		17
#define RATIONAL_STDF		18
#define REAL_STDF		19
#define ABS_STDF		20
#define RECIPROCAL_STDF 	21
#define MAKE_RAT1_STDF		22
#define ODD_STDF        	23
#define SIGN_STDF		24
#define PRED_STDF		25
#define EXP_STDF		26
#define LN_STDF			27
#define SIN_STDF		28
#define PULL_APART_REAL_STDF	29
#define INVTAN_STDF		30
#define STRING_TO_REAL_STDF     31
#define STRING_TO_NAT_STDF      32     
#define STRING_TO_INT_STDF      33
#define STRING_TO_RAT_STDF      34
#define HEX_TO_NAT_STDF		35

/*--------*
 * System *
 *--------*/

#define FCLOSE_STDF		36
#define CHDIR_STDF		37
#define GETENV_STDF		38
#define DATE_STDF		39
#define SECONDS_STDF		40
#define FSTAT_STDF		41
#define RM_STDF			42
#define RMDIR_STDF		43
#define MKDIR_STDF		44
#define DIRLIST_STDF		45
#define GETCWD_STDF		46
#define FLUSH_FILE_STDF		47
#define OS_ENV_STDF		48

/*------------------------------*
 * General conversion to string *
 *------------------------------*/

#define TO_STRING1_STDF         	49
#define BOOL_TO_STRING_STDF		50
#define HERMIT_TO_STRING_STDF		52
#define EXCEPTION_TO_STRING_STDF 	53
#define COMPARISON_TO_STRING_STDF 	54
#define FILEMODE_TO_STRING_STDF 	55
#define OUTFILE_DOLLAR_STDF		56
#define SPECIES_DOLLAR_STDF		58

/*----------*
 * Unknowns *
 *----------*/

#define PRIVATE_UNKNOWN_STDF	59
#define PUBLIC_UNKNOWN_STDF	60
#define PROT_PRIV_UNKNOWN_STDF  61
#define PROT_PUB_UNKNOWN_STDF   62

/*-------------*
 * Bit vectors *
 *-------------*/

#define BV_MASK_STDF		63
#define BV_MIN_STDF		64
#define BV_MAX_STDF		65

/*------*
 * Misc *
 *------*/

#define WILLTRAP_STDF			66
#define FORCE_STDF			67
#define PRIMTRACE_STDF			68
#define CONT_NAME_STDF			69
#define PROFILE_STDF			70
#define SET_STACK_LIMIT_STDF		71
#define GET_STACK_LIMIT_STDF		72
#define SET_HEAP_LIMIT_STDF		73
#define GET_HEAP_LIMIT_STDF		74
#define SHOW_ENV_STDF			75
#define SHOW_CONFIG_STDF		76
#define LOAD_PACKAGE_STDF		77
#define EXCEPTION_STRING_STDF		78
#define POSITION_STDF			79
#define ACQUIRE_BOX_STDF		80
#define SUPPRESS_COMPACTIFY_STDF 	81
#define GET_STACK_DEPTH_STDF		82

#define N_UNARIES	        82   /* Number of last unary instruction */

/*----------------------------------------------------------------------*
 *		Operations that follow BINARY_PREF_I			*
 ************************************************************************
 * These are binary functions that have their arguments fully 		*
 * evaluated. 								*
 *----------------------------------------------------------------------*/

/*---------*
 * Numbers *
 *---------*/

#define PLUS_STDF		1
#define MINUS_STDF		2
#define ABSDIFF_STDF		3
#define TIMES_STDF		4
#define DIVIDE_STDF		5
#define MAKE_RAT2_STDF  	6
#define TO_STRING2_STDF		7
#define DIV_STDF		8
#define MOD_STDF		9
#define GCD_STDF		10
#define POW_STDF		11
#define UPTO_STDF		13
#define DOWNTO_STDF		14

/*-------------*
 * Comparisons *
 *-------------*/

#define COMPARE_STDF		15
#define LT_STDF			16
#define LE_STDF			17
#define GT_STDF			18
#define GE_STDF			19
#define NE_STDF			20

/*-------------*
 * Bit vectors *
 *-------------*/

#define BV_SETBITS_STDF		21
#define BV_AND_STDF		22
#define BV_OR_STDF		23
#define BV_XOR_STDF		24
#define BV_SHL_STDF		25
#define BV_SHR_STDF		26
#define BV_FIELD_STDF		27

/*--------*
 * System *
 *--------*/

#define RENAME_STDF		28
#define INPUT_STDF		29
#define FOPEN_STDF		30

/*-------*
 * Boxes *
 *-------*/

#define ONASSIGN_STDF		31

#define N_BINARIES   		31   /* Number of last binary instruction */


/*----------------------------------------------------------------------*
 *		 Operations that follow LIST1_PREF_I			*  
 ************************************************************************
 * These are unary functions that do not want their arguments		*
 *  evaluated for them. 						*
 *----------------------------------------------------------------------*/

#define LENGTH_STDF		1
#define REVERSE_STDF		2
#define PACK_STDF		3
#define LAZY_LEFT_STDF		4
#define LAZY_RIGHT_STDF		5
#define LAZY_HEAD_STDF		6
#define LAZY_TAIL_STDF		7
#define INTERN_STRING_STDF	8

#define N_LISTONES		8	/* Number of last L1-instruction */

/*----------------------------------------------------------------------*
 * 		Operations that follow LIST2_PREF_I			*
 ************************************************************************
 * These are binary functions that do not want their arguments		*
 * evaluated for them. 							*
 *----------------------------------------------------------------------*/

#define APPEND_STDF		1
#define DOCMD_STDF		2
#define SCAN_FOR_STDF		3

#define N_LISTTWOS		3   /* Number of last L2-instruction */

/*----------------------------------------------------------------------*
 *		Operations that follow TY_PREF_I			*
 ************************************************************************
 * These are instructions that get information about their own type	*
 * from the global environment.						*
 *----------------------------------------------------------------------*/

#define BOX_TO_STR_STDF 	1
#define WRAP_STDF		2
#define WRAP_NUM_STDF		3
#define COMPAT_STDF		4

#define N_TY_STDFS		4    /* Number of TY_PREF_I instructions. */

/*----------------------------------------------------------------------*
 * 		Operations that follow UNK_PREF_I			*
 ************************************************************************
 * These are unary functions that want their arguments fully evaluated, *
 * except that unknowns are stopped at as if they have been evaluated. 	*
 *----------------------------------------------------------------------*/

#define UNKNOWNQ_STDF		  1
#define PROTECTED_UNKNOWNQ_STDF   2
#define UNPROTECTED_UNKNOWNQ_STDF 3
#define SAME_UNKNOWN_STDF	  4

#define N_UNK_STDFS               4   /* Number of last UNK_PREF_I instr. */



/*=========================================================================*
 * The following list of cases lists all type building instructions.	   *
 *-------------------------------------------------------------------------*/

#define FIRST_TYPE_INSTRUCTION STAND_T_I
#define LAST_TYPE_INSTRUCTION  GLOBAL_FETCH_T_I

#define TYPE_INSTRUCTIONS\
	case STAND_T_I:\
        case STAND_WRAP_T_I:\
	case STAND_VAR_T_I:\
	case STAND_PRIMARY_VAR_T_I:\
	case STAND_WRAP_VAR_T_I:\
	case TYPE_ID_T_I:\
	case TYPE_VAR_T_I:\
	case PRIMARY_TYPE_VAR_T_I:\
	case WRAP_TYPE_ID_T_I:\
	case WRAP_TYPE_VAR_T_I:\
	case CONSTRAIN_T_I:\
	case FUNCTION_T_I:\
	case PAIR_T_I:\
	case LIST_T_I:\
        case BOX_T_I:\
	case FAM_MEM_T_I:\
	case UNPAIR_T_I:\
  	case HEAD_T_I:\
	case TAIL_T_I:\
	case POP_T_I:\
        case SWAP_T_I:\
        case CLEAR_STORAGE_T_I:\
	case STORE_T_I:\
	case STORE_AND_LEAVE_T_I:\
	case GET_T_I:\
        case SECONDARY_DEFAULT_T_I:\
	case GLOBAL_FETCH_T_I:
