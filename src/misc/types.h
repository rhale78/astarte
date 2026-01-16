/***************************************************************
 * File:    misc/types.h
 * Purpose: Types used in translator and machine
 * Author:  Karl Abrahamson
 ***************************************************************/

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

/*********************************************************************
                           Integer types
 *********************************************************************
 * Integer types are defined in the architecture-dependent header    *
 * files 16bitn.h, 32bitn.h, 32bita.h, 64bita.h.		     *
 *********************************************************************/

/*********************************************************************
                           Miscellaneous Types
 *********************************************************************/

typedef UBYTE          		Boolean;
typedef ULONG 			SIZE_T;
typedef LONG      		package_index;
typedef UBYTE HUGEPTR 		CODE_PTR;
typedef char HUGEPTR  		charptr;
typedef UBYTE HUGEPTR   	ubyteptr;

/*------------------------------------------------------------*/

struct two_shorts {
  SHORT line;
  SHORT col;
};

/*------------------------------------------------------------*/

struct scopes_struct {
  SHORT pred, nonempty, offset;
};

/*---------------------------------------------------------------*
 * A FILE_OR_STR represents either a file or a char* buffer that *
 * can be written into.						 *
 *---------------------------------------------------------------*/

typedef struct {
  LONG offset;	/* -1 if this value represents a file, and the current 	*/
		/* offset in str for printing on a string.		*/

  LONG len;	/* The length of buffer str, when printing on a string. */

  union {
    FILE* file;
    char* str;
  } u;
} 
FILE_OR_STR;

/*------------------------------------------------------------*/

struct debug_tbl_struct {
  char* str;
  UBYTE* var1;
  UBYTE* var2;
};

/************ Set stored as bit-vector *****************/

typedef ULONG intset;
#define MEMBER(x,s)	(((1L << (x)) & (s)) != 0)


/****************************************************************
 *			   INTERPRETER PROFILER			*
 ****************************************************************/

typedef struct {
  LONG instructions_executed;
  LONG number_calls;
  struct hash2_table *tbl;
} PROFILE_INFO;

/****************************************************************
 *				ENTITIES 			*
 ****************************************************************/

#ifdef SMALL_ENTITIES
#  ifdef STRUCT_ENTITY
     typedef struct {
       LONG val; 
     } ENTITY;
#    define EV(e)      ((e).val)
#    define EVS(es)    ((es)->val)
#    define MAKEENT(l) (make_ent(l))
#  else
     typedef LONG ENTITY;
#    define EV(e) ((LONG)(e))
#    define EVS(es) ((LONG)(*(es)))
#    define MAKEENT(l) ((LONG)(l))  
#  endif
#else
   typedef struct {
     LONG val;
     UBYTE tag;
   } ENTITY;
#endif

/*------------------------------------------------------------*/

typedef ubyteptr CHUNKPTR;

/*------------------------------------------------------------*/

typedef USHORT HUGEPTR CHUNKHEADPTR;

/*------------------------------------------------------------*/

struct typed_entity {
  ENTITY      ent;
  struct type* type;
};

/*------------------------------------------------------------*/

struct filed {
  int   fd;                      /* Descriptor of file, or -1 if none. 	*/

  Boolean delay;                 /* True if set for nonblocking read.  	*/

  Boolean was_blocked;		 /* True if previous attempt to read   	*/
				 /* blocked. 				*/

  FILE*  file;                   /* FILE* corresponding to fd, only for	*/
				 /* out file. 				*/

  LONG  stamp;                   /* Indicates access time. 		*/

  struct file_entity* file_rec;  /* Points to file_entity node that	*/
				 /* points to this filed node.  (NULL if*/
				 /* none.) 				*/
};

/*------------------------------------------------------------*/

struct fontd {
  int handle;			 /* Handle of the font, or -1 if none. 	*/

  LONG stamp;			 /* Indicates access time. 		*/

  struct file_entity* font_rec;  /* Points to the file_entity node that	*/
				 /* points to this filed node.  (NULL if*/
                                 /* none.) 				*/
};

/*-------------------------------------------------------------------*
 * file_entity structures are used to describe both files and fonts. *
 *-------------------------------------------------------------------*/

struct file_entity {
  UBYTE kind;		/* Kind of file (one of the _FK constants below) */

  UBYTE mode;		/* Mode of file (one of the _FM constants below) */
			/* Unused when kind is FONT_FK. 		 */

  SBYTE mark;       	/* For garbage collection 			 */

  SBYTE descr_index;	/* For a file: index of file record for this	 */
			/*    file (-1 if none).			 */
			/* For a font: index of the font record for this */
			/*    font (-1 if none) 			 */

  union {
    struct {
      LONG pos;		/* Character position in file `name'		 */
			/* (input files only) 				 */

      ENTITY val;	/* Contents of input file (true if not known) 	 */

      char* name;	/* Name of file 				 */
    } file_data;

    struct {
      char* fileName;	/* Name of file for font 			*/

      char* fontName;	/* Name of font 				*/

      int pointHeight;  /* Height of font, in points. 			*/
    } font_data;

    struct file_entity* next; 	/* For linking free space list 		*/
  } u;
};

/*------------------------------------------------------------*/

struct file_ent_block {
  struct file_ent_block* next;
  struct file_entity blk[FILE_ENT_BLOCK_SIZE];
};


#define NO_FILE_FK 		0
#define OUTFILE_FK		1
#define INFILE_FK		2
#define STDIN_FK		3
#define STDOUT_FK		4
#define STDERR_FK		5
#define FONT_FK			6

#define VOLATILE_INDEX_FM	0
#define APPEND_INDEX_FM		1
#define BINARY_INDEX_FM		2
#define LARGEST_INDEX_FM	2       /* Largest _FM index */
#define VOLATILE_FM		1	/* 2^VOLATILE_INDEX_FM */
#define APPEND_FM		2	/* 2^APPEND_INDEX_FM */
#define BINARY_FM		4	/* 2^BINARY_INDEX_FM */

#define IS_VOLATILE_FM(m) ((m) & VOLATILE_FM)
#define IS_TEXT_FM(m) !(IS_BINARY_FM(m))
#define IS_BINARY_FM(m) ((m) & BINARY_FM)
#define IS_APPEND_FM(m) ((m) & APPEND_FM)
#ifdef MSWIN
# define IS_VOLATILE_OR_DOSTEXT_FM(m) (((m) & (BINARY_FM | VOLATILE_FM)) == BINARY_FM)
#else
# define IS_VOLATILE_OR_DOSTEXT_FM(m) IS_VOLATILE_FM(m)
#endif

#define READ		0
#define WRITE		1

#define WTYPE u.type
#define WTAG  u.tag

/*------------------------------------------------------------*/

typedef union small_real {
  DOUBLE val;
  union small_real *next;
} 
SMALL_REAL;

/*------------------------------------------------------------*/

typedef struct large_real {
  ENTITY man, ex;
} 
LARGE_REAL;

/*------------------------------------------------------------*/

struct exception_data_struct {
  char*    name;
  CODE_PTR type_instrs;
  char*    descr;
};


/****************************************************************
 *				TYPE	 			*
 ****************************************************************/

typedef enum {
  /*---------------------------------------------------------------*
   * Structured type kinds must come first.  Do not reorder: these *
   * must begin with FAM_MEM_T and end with PAIR_T.		   *
   *---------------------------------------------------------------*/

  FAM_MEM_T, 		/* FIRST_STRUCTURED_T */
  FUNCTION_T, 
  PAIR_T, 		/* LAST_STRUCTURED_T  */

  /*--------------------------------------------------------*
   * Next comes BAD_T, for types that do not mean anything. *
   *--------------------------------------------------------*/

  BAD_T, 

  /*--------------------------------------------------------*
   * Identifier codes must come next. Do not reorder these. *
   *--------------------------------------------------------*/

  TYPE_ID_T, 		  /* FIRST_TYPE_T, FIRST_PRIMARY_TYPE_T
			     A primary species			   */
  FAM_T,		  /* LAST_REAL_PRIMARY_TYPE_T
			     A primary family			   */
  FICTITIOUS_TYPE_T, 	  /* A fictitious member of a genus    	   */
  FICTITIOUS_FAM_T,  	  /* LAST_PRIMARY_TYPE_T,
			     A fictitious member of a community    */
  WRAP_TYPE_T,		  /* FIRST_WRAP_TYPE_T 
			     A wrapped (secondary) species	   */
  WRAP_FAM_T,		  /* A wrapped family		 	   */
  FICTITIOUS_WRAP_TYPE_T, /* A fictitious wrapped species.         */
  FICTITIOUS_WRAP_FAM_T,  /* LAST_WRAP_TYPE_T, LAST_TYPE_T	   */
			  /* A fictitious wrapped family.	   */

  /*-------------------------*
   * MARK_T  must come next. *
   *-------------------------*/

  MARK_T,		/* FIRST_BOUND_T */

  /*-------------------------------------------------------------*
   * The variables must be last, and must start with TYPE_VAR_T. *
   * Do not reorder these.  The relative positions are used in   *
   * vartbl.c.							 *
   *-------------------------------------------------------------*/

  TYPE_VAR_T, 		/* FIRST_VAR_T	       */
  FAM_VAR_T,		/* LAST_ORDINARY_VAR_T */
  PRIMARY_TYPE_VAR_T,	/* FIRST_PRIMARY_VAR_T */
  PRIMARY_FAM_VAR_T,	/* LAST_PRIMARY_VAR_T  */
  WRAP_TYPE_VAR_T,	/* FIRST_WRAP_VAR_T    */
  WRAP_FAM_VAR_T 	
} 
TYPE_TAG_TYPE;

#define FIRST_STRUCTURED_T	FAM_MEM_T
#define LAST_STRUCTURED_T	PAIR_T
#define FIRST_TYPE_T		TYPE_ID_T
#define FIRST_PRIMARY_TYPE_T	TYPE_ID_T
#define LAST_PRIMARY_TYPE_T	FICTITIOUS_FAM_T
#define FIRST_WRAP_TYPE_T	WRAP_TYPE_T
#define LAST_TYPE_T		FICTITIOUS_WRAP_FAM_T
#define LAST_WRAP_TYPE_T	FICTITIOUS_WRAP_FAM_T
#define FIRST_BOUND_T		MARK_T
#define FIRST_VAR_T		TYPE_VAR_T
#define FIRST_ORDINARY_VAR_T	TYPE_VAR_T
#define LAST_ORDINARY_VAR_T	FAM_VAR_T
#define FIRST_PRIMARY_VAR_T	PRIMARY_TYPE_VAR_T
#define LAST_PRIMARY_VAR_T	PRIMARY_FAM_VAR_T
#define FIRST_WRAP_VAR_T	WRAP_TYPE_VAR_T

/*------------------------------------------------------------*/

#define IS_STRUCTURED_T(k)\
  /* Is k a structured type kind ? */\
  ((k) <= LAST_STRUCTURED_T)

/*------------------------------------------------------------*/

#define IS_VAR_T(k)\
  /* Is k a variable kind? */\
  ((k) >= FIRST_VAR_T)

/*------------------------------------------------------------*/

#define IS_WRAP_VAR_T(k)\
  /* Is k a wrap variable kind? */\
  ((k) >= FIRST_WRAP_VAR_T)

/*------------------------------------------------------------*/

#define IS_PRIMARY_VAR_T(k)\
  /* Is k a primary variable kind? */\
  ((k) >= FIRST_PRIMARY_VAR_T && (k) <= LAST_PRIMARY_VAR_T)

/*------------------------------------------------------------*/

#define IS_ORDINARY_VAR_T(k)\
  /* Is k an ordinary variable kind? */\
  ((k) >= FIRST_ORDINARY_VAR_T && (k) <= LAST_ORDINARY_VAR_T)

/*------------------------------------------------------------*/

#define IS_ORDINARY_KNOWN_VAR_T(k)\
  /* Is k an ordinary variable, given that k is known to be a variable? */\
  ((k) <= LAST_ORDINARY_VAR_T)

/*------------------------------------------------------------*/

#define IS_PRIMARY_OR_ORDINARY_VAR_T(k)\
  /* Is k either a primary or ordinary variable? */\
  ((k) >= FIRST_ORDINARY_VAR_T && (k) <= LAST_PRIMARY_VAR_T)

/*------------------------------------------------------------*/

#define IS_NOT_WRAP_VAR_T(k)\
  /* Is k not a wrap variable? */\
  ((k) < FIRST_WRAP_VAR_T)

/*------------------------------------------------------------*/

#define IS_TYPE_VAR_T(k)\
  /* Is k a type variable (not a family variable)? */\
  ((k) == TYPE_VAR_T || (k) == WRAP_TYPE_VAR_T || (k) == PRIMARY_TYPE_VAR_T)

/*------------------------------------------------------------*/

#define IS_TYPE_T(k)\
  /* Is k a kind of species or family? */\
  ((k) >= FIRST_TYPE_T && (k) <= LAST_TYPE_T)

/*------------------------------------------------------------*/

#define IS_FAM_OR_FAM_VAR_T(k)\
  /* Is k a kind of family or family variable? */\
  (MEMBER((k), fam_tkind_set) != 0)

/*------------------------------------------------------------*/

#define IS_FAM_VAR_T(k)\
  /* Is k a kind of family variable? */\
  (MEMBER((k), fam_var_tkind_set) != 0)  

/*------------------------------------------------------------*/

#define IS_FAM_T(k)\
  /* Is k a kind of family (and not a variable) */\
  (MEMBER((k), fam_type_tkind_set) != 0)  

/*------------------------------------------------------------*/

#define IS_WRAP_TYPE_T(k)\
  /* Is k a kind of wrapped species or family? */\
  ((k) >= FIRST_WRAP_TYPE_T && (k) <= LAST_WRAP_TYPE_T)

/*------------------------------------------------------------*/

#define IS_PRIMARY_TYPE_T(k)\
  /* Is k a kind of primary species or family? */\
  ((k) >= FIRST_PRIMARY_TYPE_T && (k) <= LAST_PRIMARY_TYPE_T)

/*------------------------------------------------------------*/

#define IS_KNOWN_WRAP_TYPE_T(k)\
  /* Is k a kind of wrapped species or family? Here, it must be known\
   * that k is some kind of species or family.  */\
  ((k) >= FIRST_WRAP_TYPE_T)

/*------------------------------------------------------------*/

#define IS_FICTITIOUS_T(k)\
  /* Is k a fictitious kind of species or family? */\
  ((k) == FICTITIOUS_TYPE_T      || (k) == FICTITIOUS_FAM_T ||\
   (k) == FICTITIOUS_WRAP_TYPE_T || (k) == FICTITIOUS_WRAP_FAM_T)

/*------------------------------------------------------------*/

#define IS_VAR_OR_WRAP_TYPE_T(k)\
  /* Is k a kind of variable, or a kind of wrapped species or family? */\
  /* It is assumed that k cannot be MARK_T, so an efficient test can  */\
  /* be done */\
  ((k) >= FIRST_WRAP_TYPE_T)

/*------------------------------------------------------------*/

#define IS_PRIMARY_T(k)\
  /* Is k a kind of primary type or variable? */\
  (MEMBER((k), primary_tkind_set) != 0)

/*------------------------------------------------------------*/

#define IS_SECONDARY_T(k)\
  /* Is k a kind of secondary type or variable? */\
  (MEMBER((k), wrap_tkind_set) != 0)

/*------------------------------------------------------------*/

typedef struct type {
  LONG			ref_cnt;	/* Reference count for this cell */

  TYPE_TAG_TYPE    	kind;		/* Tag of this type.		 */ 

  SHORT 		TOFFSET;	/* Offset of this var in global	 */
					/* env 				 */

  USHORT		dfnum;		/* Needed for cycle traversal	*/
  USHORT		lowlink;	/* Needed for cycle traversal	*/

  unsigned	        hermit_f: 1,	/* Used to indicate a function	 */
					/* type that is really the	 */
					/* hermit_type. 		 */
					/*				 */
					/* Also used in a variable to    */
					/* indicate a variable that      */
					/* prefers to default to a	 */
					/* secondary type.		 */

			freeze: 1,	/* 1 if this node should not	 */
					/* be freed.			 */

  		        special: 1,	/* 1 for a type that is used only*/
					/* in completeness checking 	 */

			nosplit: 1,	/* 1 for a variable that should     */
					/* not be split during completeness */
					/* check.			    */

			seen: 2,	/* Used in cycle checking. 	 */

			standard: 1,	/* 1 if this is a standard id. 	 */

			free: 1,	/* 1 if this type has free	 */
					/* variables, where a variable	 */
					/* that is bound at definition   */
					/* time is not considered free,  */
					/* but other unbound variables   */
					/* are free, including currently */
					/* bound runtime- bound variables*/

  			norestrict: 1,  /* 0 normally, 1 for a variable	 */
					/* that should not be restricted */
					/* (C``a) 			 */

           		copy: 1,	/* Copy is 1 iff this type 	 */
				 	/* contains a variable or mark,	 */
					/* 0 otherwise.  (It can be out  */
				        /* of date and be 1 even though  */
					/* there are no variables or	 */
					/* marks.) 			 */

 			used: 1,	/* 1 if this variable is of any  */
					/* interest in generating global */
					/* id lookups. 			 */

			prmark: 1,	/* Used to detect loops when	 */
					/* printing			 */

  			mark: 1,	/* Used during gargage collection*/
					/* and during constraint reduction */

			dfsmark: 1,	/* Used in dept-first search     */

 			anon: 1,        /* 1 for an anonymous variable   */

			seenTimes: 2;   /* Number of times this type	 */
					/* occurs in a list of types --  */
					/* used in gentype.c		 */

  union {
    LONG 	 thash;			/* Hash of this type.		 */
    struct list* lower_bounds;		/* List of lower bounds for a	 */
					/* variable.			 */
  } hu;

  struct class_table_cell* ctc;		/* cell in class_table.		 */

  struct type*		ty1;		/* Component type or binding of  */
					/* a type variable.              */
  union{
    LONG	 num;			/* Extra num parameter.		 */
    struct type* ty2;			/* Second type parameter.	 */

    char*	 str;			/* Second string parameter	 */

    struct list* list;                  /* Program variables that hold   *
					 * the binding of this type	 *
					 * variable, for a run-time 	 *
					 * bound variable.		 */
  }			u;

} 
TYPE;
        
/*------------------------------------------------------------*/

typedef struct lpair {
  SHORT label1, label2;
} 
LPAIR;

/****************************************************************/
/* 		NULL TYPE AND FIELD NAMES OF TYPE 		*/
/****************************************************************/

#define NULL_T 		((TYPE *) 0)
#define TY1		ty1
#define TNUM		u.num
#define TY2		u.ty2
#define STR2		u.str
#define TLIST		u.list
#define LOWER_BOUNDS    hu.lower_bounds
#define THASH           hu.thash
#define PREFER_SECONDARY hermit_f

#ifdef GCTEST
# define TKIND(x)	(tkindf(x))
#else
# define TKIND(x)	((x)->kind)
#endif


/************************************************************************
 *			        PRINT_TYPE_CONTROL			*
 ************************************************************************/

/****************************************************************
 * Type printing functions are shared by the compiler and the   *
 * interpreter.  The compiler wants to print to a file, but the *
 * interpreter wants to print to a file or string.  FPRINTF is  *
 * fprintf for the compiler and ggprintf for the interpreter.   *
 * Type FOS is FILE for the compiler, and FILE_OR_STR for the   *
 * interpreter.							*
 ****************************************************************/

#ifdef MACHINE
# define FPRINTF ggprintf
# define FOS     FILE_OR_STR
#else
#  ifdef TRANSLATOR
#    define FPRINTF fprintf
#    define FOS     FILE
#  endif
#endif

/*-------------------------------------------------------------------------*
 * This is used in controling how variables are shown when printing types. *
 *-------------------------------------------------------------------------*/

#ifdef FOS
typedef struct {
  FOS *f;			/* Where to print.			    */

  struct hash2_table *ty_b;	/* Binds variables to numbers.  For         *
				 * example, if variable V is bound to 0,    *
				 * then V is printed as `a.		    */

  struct list *constraints;     /* This is a list of pairs, where (V,A)     *
				 * indicates that V >= A.		    */

  int next_qual_num;		/* Next number to use for a variable.       */
				/* Negative for alternate form of print.    */

  int chars_printed;		/* The number of characters that have been  */
				/* printed to this control.		    */

  Boolean print_marks;		/* true if marks {V} should be shown.  They */
				/* are ignored if this is false.	    */

} PRINT_TYPE_CONTROL;
#endif


/************************************************************************
 *				ROLE					*
 ************************************************************************/

typedef struct role {
  SHORT kind;
  SHORT ref_cnt;
  struct list* namelist;
  struct role* role1;
  struct role* role2;
  struct mode_type* mode;
} 
ROLE;

typedef struct rtype {
  TYPE* type;
  ROLE* role;
} 
RTYPE;


/************************************************************************
 *				CLASS_UNION_CELL			*
 ************************************************************************/

typedef struct class_union_cell {
  SHORT special;
  SHORT tok;
  SHORT line;
  char* name;
  struct mode_type* mode;
  struct list* withs;
  union {
    struct expr* expr;
    TYPE* 	 type;
    struct list* types;
  } u0;
  union {
    ROLE*        role;
    struct list* roles;
    struct class_union_cell* next;
  } u;
} 
CLASS_UNION_CELL;

#define CUC_TYPE  u0.type
#define CUC_EXPR  u0.expr
#define CUC_TYPES u0.types
#define CUC_ROLE  u.role
#define CUC_ROLES u.roles


/************************************************************************
 *				EXPR					*
 ************************************************************************/

typedef enum {
    BAD_E,
    IDENTIFIER_E,
    GLOBAL_ID_E,
    LOCAL_ID_E,
    UNKNOWN_ID_E,
    OVERLOAD_E,
    CONST_E,
    SPECIAL_E,
    DEFINE_E,
    LET_E,
    MATCH_E,
    OPEN_E,
    PAIR_E,
    SAME_E,
    IF_E,
    TRY_E,
    TEST_E,
    APPLY_E,
    STREAM_E,
    FUNCTION_E,
    SINGLE_E,
    LAZY_LIST_E,
    AWAIT_E,
    LAZY_BOOL_E,
    TRAP_E,
    LOOP_E,
    RECUR_E,
    FOR_E,
    PAT_FUN_E,
    PAT_VAR_E,
    PAT_RULE_E,
    PAT_DCL_E,
    EXPAND_E,
    MANUAL_E,
    WHERE_E,
    EXECUTE_E,
    SEQUENCE_E
} 
EXPR_TAG_TYPE;

/*----------------------------------------------------------------------*
 * The following are for reports: they must occur beyond the last 	*
 * genuine expr kind.							*
 *----------------------------------------------------------------------*/

#define ANTICIPATE_E 	50
#define BEHAVIOR_E 	51
#define EXPECT_E	52

/*--------------------------------------------------------*
 * All of the following must occur at or beyond IMPORT_E. *
 * This is used in report.c.				  *
 *--------------------------------------------------------*/
 
#define IMPORT_E	60
#define TYPE_E  	61
#define FAM_E   	62
#define GENUS_E 	63
#define COMM_E  	64
#define TEAM_E   	65
#define EXTEND_E 	66
#define MEET_E 		68
#define EXPAND_AUX_E    69

#define LAST_E		69     /* Number of last _E constant */

/*----------------------------------------------------------------------*
 * The following is for performing first pass of type inference for	*
 * overloads. These should be larger than the largest _E constant. 	*
 *----------------------------------------------------------------------*/

#define FIRST_PASS 70

typedef struct expr {
  LONG	 		   ref_cnt;	/* Reference count for this node  */

  UBYTE			   temp;        /* Used by LOCAL_ID_E nodes for   */
					/* keeping track of nesting depth */
					/* of open ifs, etc.		  */

  UBYTE			   kind;	/* The kind of this expression    */

  UBYTE			   mark;

  unsigned		   pat:1,      	/* 1 if expr contains a pattern var,*/
					/* 0 otherwise 			   */

  			   bound:1,	/* 0 for an unbound id,		   */
					/* 1 for a bound id.  Used at code */
  					/*   generation time. 		   */

  		       	   used_id:1,   /* 1 for an id that has been used, */
					/* 0 for an id that has not been   */
					/*   used. 			   */

			   done:1,	/* 0 for a special expr or id	   */
					/*   that has not been handled 	   */
					/*   by do_the_defaults,	   */
					/* 1 for one that has been handled.*/
					/*				   */
					/* This bit is also used in code   */
					/* generation, on non-identifiers, */
					/* to indicate an expression that  */
					/* binds type variables dynamically.*/

			   extra:1,     /* Used by FOR_E, PAT_RULE_E, 	   */
					/* AWAIT_E, LOOP_E, DEFINE_E, 	   */
					/* SPECIAL_E, GLOBAL_ID_E, 	   */
					/* PAT_FUN_E, PAIR_E 		   */

			   irregular:1,	/* For a GLOBAL_ID_E, this is 1 to */
					/* indicate an irregular id.	   */

			   nomatches:1, /* 1 if contains no match exprs    */

			   patfun_prim:1; /* 1 for a pat-fun select primitive*/

  struct type* 		   ty;		/* Type of this expr		   */

  struct role* 		   role;	/* Role of this expr		   */

  union {
    struct {
      SHORT primitive;   		/* Indicates whether id has a 	   */
					/* primitive instruction 	   */
					/* associated with it.		   */

      SHORT line_num;			/* Line number where this expr	   */
					/* begins			   */
    } p1;

    char *tag_name;
  }			   u0;		

  union {
    struct expr*	    e1;		/* A subexpression  		 */
    struct global_id_cell*  gic;	/* Record for a global id 	 */
    struct list*            l0;
  }			   u1;

  union {
    struct expr*      e2;		/* Second subexpression 	  */
    char*             str;		/* Description of expression      */
    struct list*      l1;     
  }			   u2;

  union{
    char*            str;		/* Description of expression      */
    struct expr*     e3;		/* Third subexpression            */
    LONG     	     i;			/* an integer value 		  */
    struct list*     l2;
    struct {
      SHORT scope;			/* Scope of id 			  */
      SHORT offset;			/* Offset of id 		  */
    }		     where;
  }                        u3;

 union {
   struct list*	     l3;
   char*	     str;
   struct mode_type* mode;
   LONG		     i;
 }			   u4;

} 
EXPR;


/********************************************************/
/* 	NULL EXPRESSION AND ACCESS TO FIELDS OF EXPR  	*/
/********************************************************/

#define NULL_E ((EXPR *) 0)

#define MARKED_PF	extra

#define SAME_CLOSE	bound

#define PAT_RULE_MODE   temp
#define ETEMP		temp
#define TEAM_MODE	temp

#define PRIMITIVE	u0.p1.primitive
#define OPEN_FORM	u0.p1.primitive
#define OPEN_LOOP	u0.p1.primitive
#define LAZY_BOOL_FORM	u0.p1.primitive
#define PAT_DCL_FORM	u0.p1.primitive
#define EXPAND_FORM	u0.p1.primitive
#define IRREGULAR_FUN	u0.p1.primitive
#define TRAP_FORM	u0.p1.primitive
#define TEAM_FORM	u0.p1.primitive
#define OUTER_ID_FORM	u0.p1.primitive
#define FOR_FORM	u0.p1.primitive
#define LET_FORM	u0.p1.primitive
#define DEFINE_FORM	u0.p1.primitive
#define STREAM_MODE	u0.p1.primitive
#define ROLE_MOD_APPLY  u0.p1.primitive
#define IF_MODE		u0.p1.primitive
#define SAME_MODE	u0.p1.primitive
#define PATFUN_ARGS	u0.p1.primitive
#define DOUBLE_COMPILE	u0.p1.primitive
#define COROUTINE_FORM	u0.p1.primitive
#define MAN_FORM	u0.p1.primitive
#define RULE_LOC	u0.p1.primitive
#define ETAGNAME	u0.tag_name
#define LINE_NUM	u0.p1.line_num

#define E1		u1.e1
#define GIC		u1.gic
#define EL0		u1.l0

#define E2		u2.e2
#define STR		u2.str
#define EL1		u2.l1

#define E3		u3.e3
#define SCOPE		u3.where.scope
#define ETEMP2		u3.where.scope
#define OFFSET		u3.where.offset
#define SINGLE_MODE	u3.where.offset
#define EINT		u3.i
#define STR3		u3.str
#define EL2		u3.l2

#define EL3		u4.l3
#define STR4		u4.str
#define SAME_E_DCL_MODE u4.mode
#define TRY_KIND	u4.i

#ifdef GCTEST
# define EKIND(e)       (ekindf(e))
#else
# define EKIND(e)	((e)->kind)
#endif


/****************************************************************
 *			DECLARATION PROCESSING			*
 ****************************************************************/

typedef struct report_record {
  int  			kind;
  struct mode_type*	mode;
  char* 		name;
  TYPE* 		type;
  ROLE*			role;
  struct report_record* next;
  char*			aux;
  struct class_table_cell*  ctc;
  LPAIR			lp;
} 
REPORT_RECORD;



/************************************************************************
 *			CLASS TABLE					*
 ************************************************************************/

typedef ULONG ANCESTOR_TYPE;

typedef struct class_table_cell {
  SHORT		        num;             /* Number of this id (index in	    */
					 /* ctcs). 			    */

  SHORT                 var_num;         /* Number of genus or community.   */
					 /* (index in v_ctcs)		    */
					 /* Holds -1 if this is a type or   */
					 /* family.                         */

  SHORT			mem_num;	 /* Number of ctc that represents   */
					 /* a canonical member of a class   */
					 /* or community.  Only meaningful  */
					 /* when nonempty field holds true. */

  UBYTE			std_num;         /* Standard number, if standard,   */
					 /* or 0 if not standard.           */

  SBYTE 		code;            /* Token - CODE_OFFSET             */

  unsigned 	 	extensible:2,    /* 0 for a nonextensible genus or  */
					 /*   community			    */
					 /* 1 for a genus or community such */
					 /*   as ANY that can be extended,  */
					 /*   but not explicitly,           */
					 /* 2 for a program-extensible 	    */
					 /*   genus or community.	    */ 

  			expected:2,      /* 1 if an expected type or family,*/
					 /* 2 if expected in import         */

  			is_changed:1,	 /* 1 if this genus or comm has     */
					 /* had its definition changed      */
					 /* recently.                       */

			nonempty:1,	 /* 0 for an empty genus or comm,   */
					 /* 1 for a nonempty genus or comm  */

			dangerous:1,	 /* 0 normally, 1 for a genus or    */
					 /* comm whose default is dangerous */

			opaque:1,	 /* 0 for a transparent family or   */
					 /* community (having a self link   */
					 /* label), and 1 for an opaque	    */
					 /* family or community (having an  */
					 /* ANY link label).		    */

			partial:1,	 /* 1 if this is a uniform species  */
				         /* that only uses a subset of its  */
					 /* representation species.	    */

			closed:1;        /* 1 if existed at prior closure   */

  char*		name;                    /* Name of this id                 */

  char*         package;                 /* package where defined           */

  struct list*	constructors;		 /* list of constructor names for a */
					 /* type                            */

  ANCESTOR_TYPE ancestors[ANCESTORS_SIZE]; /* Bit vector giving proper      */
					   /* ancestors of this id.         */

  TYPE*		ty;                      /* Type expr for this id           */

  struct role*	role;                    /* Role expr for this id           */

  char*         descr;		 	 /* Description of this id.         */

  char*	        descrip_package;	 /* Name of the package where this  */
					 /* description was given.          */
  union {
    TYPE*	dfault;                  /* default for a genus or comm.    */

    TYPE* 	rep_type;		 /* Representation type for a type, */
					 /* (arg-type,rep-type) for a family*/
  } u;

  struct hash2_table* dfaults;		 /* Table of defaults (in 	    */
					 /* interpreter)		    */
} 
CLASS_TABLE_CELL;

#define CTC_DEFAULT 	u.dfault
#define CTC_DEFAULT_TBL dfaults
#define CTC_REP_TYPE 	u.rep_type

typedef struct ahead_meet_chain {
  char* a;
  char* b;
  char* c;
  char* l1;
  char* l2;
  struct ahead_meet_chain* next;
} 
AHEAD_MEET_CHAIN;


/***************************************************************************
 *			GARBAGE COLLECTOR AND STORAGE ALLOCATION
 ***************************************************************************/

/************************************************************************
 * 			Blocks for Garbage Collector to Search		*
 ************************************************************************/

typedef struct ent_block {
  struct ent_block* next;
  ENTITY            cells[ENT_BLOCK_SIZE];
} 
ENT_BLOCK;

typedef struct free_ent_data {
  ENTITY* free_ents;
  LONG   free_ents_size;
  SHORT   where_ent;
} 
FREE_ENT_DATA;

typedef struct binary_block {
  struct binary_block* next;
  UBYTE                cells[BINARY_BLOCK_SIZE];
} 
BINARY_BLOCK;

typedef struct free_binary_data {
  CHUNKPTR free_chunks;
  USHORT    free_chunks_size;
  USHORT    where;
} 
FREE_BINARY_DATA;

typedef struct small_real_block {
  LONG 			   marks;
  struct small_real_block* next;
  SMALL_REAL 		   cells[SMALL_REAL_BLOCK_SIZE];
} 
SMALL_REAL_BLOCK;


typedef struct gc_block {
  struct gc_block* next;
  UBYTE            cells[GC_BLOCK_SIZE];
} 
GC_BLOCK;

typedef struct boxset {
  int    height;            /* Height of this subtree. */

  LONG   minb, maxb;	    /* minb..maxb are in the set */

  LONG   new_minb;	    /* Relocated location of minb */

  struct boxset* left;      /* left subtree (boxes < minb) */

  struct boxset* right;     /* right subtree (boxes > maxb) */
} 
BOXSET;

typedef int REG_TYPE;
typedef int REGPTR_TYPE;

/************************************************************************
 *			INSTRUCTION INFO				*
 ************************************************************************/

/* See evaluate/instinfo.c for the meanings of the following. */

#define NO_PARAM_INST		1
#define BYTE_PARAM_INST		2
#define TWO_BYTE_PARAMS_INST	3
#define LONG_NUM_PARAM_INST	4
#define BYTE_GLABEL_INST	5
#define GLABEL_PARAM_INST	6
#define LLABEL_PARAM_INST	7
#define LLABEL_ENV_INST		8
#define BYTE_LLABEL_INST	9
#define PREF_INST		10
#define TY_PREF_INST		11
#define LET_INST		12
#define RELET_INST		13
#define DEF_INST		14
#define EXC_INST		15
#define NO_TYPE_INST		16
#define BYTE_TYPE_INST		17
#define TWO_BYTE_TYPE_INST	18
#define GLABEL_TYPE_INST	19
#define LLABEL_INST		20
#define LONG_LLABEL_INST	21
#define LINE_INST		22
#define STOP_G_INST		23
#define END_LET_INST		24
#define ENTER_INST		25
#define END_INST		26

struct instruction_info {
  UBYTE class;	 /* One of the _INST constants above. */

  UBYTE args;	 /* Number of args to show at a trap. */

  UBYTE qtry_ok; /* 1 if ok inside a quick-try, 0 if not. */

  UBYTE pad;	 /* Pad to four bytes. */
};

/************************************************************************
 *			LEXER AND PARSER				*
 ************************************************************************/

/************ Flex scanner ***************/

#ifndef FLEX_SCANNER
struct yy_buffer_state {int i;};  /* Dummy -- suppresses warnings */
typedef struct yy_buffer_state * YY_BUFFER_STATE;
#endif

/****************** Token Attributes ***********************/

/*--------------------------------------------*
 * Do not change SHARED_ATT or NONSHARED_ATT. *
 *--------------------------------------------*/

#define SHARED_ATT    1
#define NONSHARED_ATT 2

typedef enum {
  /*------------------------------------------------------------*
   * Do not reorder FIRST_ATT ... ALL_MIXED_ATT. Order is used	*
   * in choose.c.						*
   *------------------------------------------------------------*/

  FIRST_ATT,		/* which (= first)     		*/
			/* also ATOMIC_TOK (= Unique)	*/
  PERHAPS_ATT,		/* which (= perhaps)    	*/
  ONE_ATT,		/* which (= one)       		*/
  ONE_MIXED_ATT,	/* which (= one mixed) 		*/
  ALL_ATT,		/* which (= all)       		*/
  ALL_MIXED_ATT,	/* which (= all mixed) 		*/

  ATOMIC_ATT,		/* ATOMIC_TOK (= Atomic)    	*/
  CUTHERE_ATT,		/* ATOMIC_TOK (= CutHere)      	*/
  IF_ATT,		/* IF_TOK (= If)           	*/
  TRY_ATT,		/* IF_TOK (= Try)	        */
  LET_ATT, 		/* LET_TOK (= Let)		*/
  DEFINE_ATT, 		/* LET_TOK (= Define)		*/
  RELET_ATT, 		/* LET_TOK (= Relet)		*/
  EXPECT_ATT,		/* EXPECT_TOK (= Expect)	*/
  ANTICIPATE_ATT,	/* EXPECT_TOK (= Anticipate)	*/
  EXPAND_ATT,		/* PATTERN_TOK (= Expand)	*/
  PATTERN_ATT,		/* PATTERN_TOK (= Pattern)	*/
  MIX_ATT,		/* STREAM_TOK (= Mix)		*/
  STREAM_ATT,		/* STREAM_TOK (= Stream)	*/
  TRAP_ATT,		/* TRAP_TOK (= Trap)		*/
  UNTRAP_ATT,		/* TRAP_TOK (= Untrap)		*/
  ROLE_SELECT_ATT,	/* For SINGLE_E exprs		*/
  ROLE_MODIFY_ATT,	/* For SINGLE_E exprs		*/
  LC_ATT,		/* Indicates lower case word.   */
  UC_ATT,		/* Indicates upper case word.   */

  /*---------------------------------------------------------------------*
   * Do not reorder the following block.  The order is used in parser.y. *
   *---------------------------------------------------------------------*/

  ARROW1_ATT,	        /* Indicates <--		*/
  ARROW2_ATT,		/* Indicates <--'		*/

  /*-----------------------------------------------------------------*
   * Do not reorder the following -- order used in parser.y (guards) *
   * and in allocators.c. 					     *
   *-----------------------------------------------------------------*/

  CASE_ATT,		/* CASE_TOK (= case)		*/
  NOCASE_ATT,		
  UNTIL_ATT,		
  UNTIL_ELSE_ATT,        
  ELSE_ATT,		
  WHILE_ATT,		
  WHILE_ELSE_ATT	
} 
ATTR_TYPE;

struct token_map {
  SHORT tok, new_tok;
  ATTR_TYPE attr;
};

struct lstr {
  SHORT     line;		/* line number   		*/

  SHORT     column;		/* column number 		*/

  ATTR_TYPE attr;           	/* additional info, if any 	*/

  SHORT     tok;            	/* end tok 			*/
				/* Note: tok is negative if     */
				/* this attribute is for a begin*/
				/* word that comes from a 	*/
				/* semicolon.			*/

  char*     name;		/* end word, or token name	*/
};

struct ntype {
  struct type* ty;
  char*        name;
};

struct line_num_struct {
  SHORT line_num, col_num;
  LONG  pos;
}; 

struct keep_opt {
  unsigned chop: 5, pure: 1, closed: 1;
};

typedef struct rtlist_pair {
  struct list* types;
  struct list* roles;
} RTLIST_PAIR;

struct with_info {
  struct list* types;
  struct list* roles;
  char* descrip;
};

typedef union {
  int 				int_at;
  char* 			str_at;
  struct lstr			ls_at;
  struct keep_opt		keepopt_at;
  struct rtype			rtype_at;
  struct rtlist_pair		rtlist_pair_at;
  struct with_info		with_info_at;
  struct list* 			list_at;
  struct expr*			expr_at;
  struct type* 			type_at;
  struct mode_type*		mode_at;
  struct ntype			ntype_at;
  struct class_union_cell*      cuc_at;
  struct class_table_cell*	ctc_at;
  struct role*			role_at;
# ifdef VISUALIZER
    struct vexpr*		vexpr_at;
# endif
} 
YYSTYPE;

typedef struct mode_type {
  int		ref_cnt;
  int  		patrule_mode;
  char*         patrule_tag;
  LONG		define_mode;
  struct list*  noexpects;
  struct list*  visibleIn;
  union {
    char*	       def_package_name;
    struct mode_type*  link;              /* used for free-space list */
  } u;
} 
MODE_TYPE;


/***************************************************************
                        Hash Tables
 ****************************************************************/

/*----------------------------------------*
 ******* Type for keys in hash tables *****
 *----------------------------------------*/

typedef union hash_key {
  LONG 	     	     num;
  struct type* 	     type;
  struct expr* 	     expr;
  char* 	     str;
  struct lpair 	     lpair;
  struct hash1_table* hash1_table;
  struct hash2_table* hash2_table;
  ENTITY*	      entp;
} 
HASH_KEY;

/*----------------------------------------------------------*
 **** Type for cells in hash tables that have only keys. ****
 *----------------------------------------------------------*/

typedef struct {
  LONG hash_val;	/* hash(key) */
  HASH_KEY key;
} 
HASH1_CELL;

typedef HASH1_CELL HUGEPTR HASH1_CELLPTR;

/*-----------------------------------------------------------------------*
 **** Type for hash tables that have only keys.  These are allocated  ****
 **** of variable size.  Although `cells' is defined to have only one ****
 **** member, it actually has hash_size[size] members.                ****
 *-----------------------------------------------------------------------*/

typedef struct hash1_table {
  int        size;	/* there are hash_size[size] cells in the table. */
  ULONG      load;	/* number of cells in use    	*/
  HASH1_CELL cells[1];	/* cell array			*/
} 
HASH1_TABLE;

/*------------------------------------------------------------------*
 **** Type for value associated with a key in a hash table that  ****
 **** has values.                                                ****
 *------------------------------------------------------------------*/

typedef union {
  struct hash2_table*    	tab;
  char*				str;
  LONG				num;
  struct type*			type;
  struct role*			role;
  struct class_table_cell*	ctc;
  struct lpair 			lpair;
  struct list*			list;
  PROFILE_INFO*			profile_info;
# ifdef TRANSLATOR
    struct expr*		expr;
    struct global_id_cell*	gic;
    struct descrip_chain*       descr_chain;    
    struct global_expectation_cell* global_expectation;
# endif
# ifdef MACHINE
    ENTITY			entity;
    struct typed_entity*        typed_entity;
# endif
} 
HASH2_VAL;


/*-------------------------------------------------------------------*
 **** Type for the cells in a hash table that has both cells and  ****
 **** values.                                                     ****
 *-------------------------------------------------------------------*/

typedef struct {
  LONG      hash_val;	/* hash(key) 		 */
  HASH_KEY  key;	/* the key		 */
  HASH2_VAL val;	/* value stored with key */
} 
HASH2_CELL;

typedef HASH2_CELL HUGEPTR HASH2_CELLPTR;

/*-------------------------------------------------------------------*
 **** Type for hash tables that have keys and values.  These are  ****
 **** allocated of variable size.  Although `cells' is defined    ****
 **** to have only one member, it actually has hash_size[size]    ****
 **** members.                                                    ****
 *-------------------------------------------------------------------*/


typedef struct hash2_table {
  int    size;		/* there are hash_size[size] cells in the table. */
  ULONG load;		/* number of cells in use    	*/
  
  HASH2_CELL cells[1];	/* cell array			*/
} 
HASH2_TABLE;


/*********************************************************************
                   INFORMATION FOR CHOOSE/LOOP EXPRESSIONS
 *********************************************************************/

typedef struct choose_info {
  ATTR_TYPE   	which;
  int   	choose_kind;
  int   	match_kind;
  EXPR* 	choose_from;
  EXPR* 	else_exp;
  EXPR* 	status_var;
  EXPR* 	loop_ref;
  struct list* 	working_choose_matching_list;  /* EXPR_LIST */
  struct choose_info* next;
} CHOOSE_INFO;


/*********************************************************************
                           LINKED LISTS AND STACKS 
 *********************************************************************/

/***** HEAD_TYPE is the type of the members of a list. ****/

typedef union { 
  LONG				i;
  char*				str;
  struct type* 			type;
  struct type** 		stype;
  struct list* 			list;
  struct mode_type*		mode;
  struct class_table_cell* 	ctc;
  struct two_shorts             two_shorts;
  FILE*				file;
  struct hash2_table*		hash2;
  struct expect_table*		expect_table;
  struct name_type*		name_type;
  struct expr* 			expr;
  ENTITY*                       ents;
  union hash_key		hash_key;
# ifdef TRANSLATOR
    struct expr** 		sexpr;
    struct expectation*		exp;
    struct class_union_cell*    cuc;
    struct role*                role;
    struct choose_info*		choose_info;
    YY_BUFFER_STATE		buf;
# endif
# ifdef MACHINE
    struct state*		state;
    struct state**              states;
    struct activation*	        act;
    struct continuation*        cont;
    struct control*		control;
    struct big_int**		bis;
    struct env_descr*		env_descr;
    struct trap_vec*		trap_vec;
# endif
} 
HEAD_TYPE;

/******* LIST* is the type of lists. ****/

typedef int LIST_TAG_TYPE;

typedef struct list{
  SHORT			ref_cnt;
  UBYTE			kind;
  UBYTE			mark;
  HEAD_TYPE		head;
  struct list* 		tail;
} 
LIST;

/*---------------------------------------------------------------------*
 * Special kinds of lists are defined for convenience in understanding *
 *---------------------------------------------------------------------*/

typedef struct list STR_LIST;		/* Lists of strings */
typedef struct list EXPR_LIST;		/* Lists of expressions */
typedef struct list INT_LIST;		/* Lists of integers */
typedef struct list SHORTS_LIST;        /* List of pairs of shorts */
typedef struct list TYPE_LIST;		/* Lists of types */
typedef struct list ROLE_LIST;		/* Lists of roles */
typedef struct list CTC_LIST;		/* Lists of class table entries */
typedef struct list EXPECT_LIST;	/* List of EXPECTATION cells. */
typedef struct list FILE_LIST;		/* Lists of files */
typedef struct list LIST_LIST;		/* Lists of lists */
typedef struct list HASH2_LIST;		/* Lists of hash tables */
typedef struct list ENTS_LIST;		/* Lists of entity pointers */

typedef struct list* INT_STACK;
typedef struct list* STR_STACK;
typedef struct list* TYPE_STACK;
typedef struct list* EXPR_STACK;
typedef struct list* FILE_STACK;
typedef struct list* LIST_STACK;
typedef struct list* SHORTS_STACK;
typedef struct list* MODE_STACK;
typedef struct list* CHOOSE_INFO_STACK;

/************************************************************************
 *			PACKAGE INFO					*
 ************************************************************************/

/*----------------------------------------------------------------------*
 * Note: in the following structure, entries in the global tables are  	*
 * duplicated in the local tables. 					*
 *----------------------------------------------------------------------*/

typedef struct import_stack {
  UBYTE context;		/* Current context (EXPORT_CX, etc.) 	*/

  Boolean is_interface_package; /* True if this is a frame for an       */
  				/* interface package			*/

  Boolean resume_long_comment_at_eol;
  				/* True if lexer should continue reading*/
				/* a long comment when it encounters an */
				/* end of line.				*/

  SHORT line;			/* Current line number 			*/

  SHORT chars_in_line;		/* Chars already consumed in current	*/
				/* line. 				*/

  SHORT import_seq_num;		/* Sequence number of this import       */

  char* package_name;		/* Name of this package 		*/

  char* file_name;		/* Name of file that holds this package */

  FILE* file;			/* File for reading this package 	*/

  YY_BUFFER_STATE lexbuf;	/* Buffer used by lexer for this package */

  INT_STACK shadow_stack;	/* Shadow of parser stack               */

  SHORTS_STACK shadow_nums;	/* Stack of line and column numbers of 	*/
				/* tokens in shadow_stack               */

  STR_LIST* public_packages;	/* List of names of packages that can 	*/
				/* see public exports from this package */

  STR_LIST* private_packages;   /* List of names of packages that can	 */
				/* see private exports from this package */

  STR_LIST* imported_packages;  /* List of packages that have been 	*/
				/* imported by this package, either	*/
				/* directly or indirectly.		*/

  STR_LIST* import_ids;	        /* List of ids that are being imported  */
				/* from this package, or (LIST *) 1 if  */
			        /* all ids are being imported.          */

  char* import_dir;		/* Current import directory (set by a	*/
				/* directory dcl 			*/

  HASH2_TABLE* import_dir_table;    /* Table of import directory bindings   */

  HASH2_TABLE* assume_table;	    /* Assume tables for this package 	    */

  HASH2_TABLE* global_assume_table; /* Assumes that were global             */

  HASH2_TABLE* assume_role_table;   /* Assumed roles                        */

  HASH2_TABLE* global_assume_role_table; /* Globally assumed roles          */

  HASH2_TABLE* patfun_assume_table; /* Tells pattern function assumptions   */

  HASH2_TABLE* global_patfun_assume_table; /* Globally assumed pattern funs */

  HASH1_TABLE* no_tro_table;	    /* Table of functions on which not to   */
				    /* do tail recursion improvement.	    */

  STR_LIST* no_tro_backout;         /* Strings to remove from no_tro_table  */
				    /* at the end of the declaration.       */

  TYPE* nat_const_assumption;       /* Type of 0                            */

  TYPE* global_nat_const_assumption; /* Globally assumed type of 0          */

  TYPE* real_const_assumption;       /* Type of 0.0                         */

  TYPE* global_real_const_assumption;/* Globally assumed type of 0.0        */

  HASH2_TABLE* abbrev_id_table;	    /* Abbrev table                         */

  HASH2_TABLE* global_abbrev_id_table; /* Global abbrevs                    */

  HASH2_TABLE* ahead_descr_table;   /* Table of ahead descriptions for	    */ 
				    /* this package 			    */

  HASH2_TABLE* local_expect_table;  /* Table of expectations made in this   */
				    /* package that are to be used as       */
  				    /* assumptions in type inference        */

  HASH2_TABLE* other_local_expect_table; /* Table of expectations made in   */
  					 /* this package that are not to be */
  					 /* used as assumptions             */

  HASH2_TABLE* op_table;	    /* Table of binary operators            */

  HASH1_TABLE* unary_op_table;      /* Table of unary operators             */

  HASH2_TABLE* default_table;	    /* Table of defaults for classes and    */
				    /* communities, keyed on ctcs index     */

  MODE_TYPE propagate_mode;	    /* The declaration propagate_mode.	    */

  struct import_stack* next;        /* Link down the stack 		    */
} 
IMPORT_STACK;


/************************************************************************
 *			TRANSLATOR GLOBAL TABLE				*
 ************************************************************************/

typedef struct descrip_chain {
  char* 		descr;		/* The description itself 	*/
  char* 		package_name;   /* The package where this	*/
					/* description occurs 		*/
  STR_LIST*		visible_in;     /* Visibility list		*/
  USHORT		line;		/* Line where this description  */
					/* occurs.			*/
  TYPE* 		type;		/* The types to which this	*/
					/* description applies 		*/
  MODE_TYPE* 		mode;		/* The mode of this description */
  struct descrip_chain* next;  		/* Next in chain 		*/
} 
DESCRIP_CHAIN;


typedef struct role_chain {
  ROLE* 		role;		/* Role				*/
  TYPE* 		type;		/* Polymorphic type that this	*/
					/* role applies to		*/
  LIST* 		visible_in;	/* List of packages that see	*/
					/* this role			*/
  char* 		package_name;	/* Package where this role is	*/
					/* declared			*/
  SHORT 		line_no;	/* Line where this role is 	*/
					/* declared			*/
  struct role_chain* 	next;		/* Next in chain		*/
} 
ROLE_CHAIN;


typedef struct part {
  SBYTE	    primitive;		/* Primitive kind			*/

  UBYTE     mode;		/* Mode of definition (low order byte)	*/

  unsigned  from_expect:1,	/* Created from expectation?		*/
	    in_body:1,		/* Created in body of package?		*/
	    trapped: 1,		/* Trapped exception?			*/
            hidden:1,		/* 1 if this definition has been 	*/
				/* declared hidden using missing{hide}.	*/
	    irregular: 1;	/* Irregular function?			*/

  SHORT arg;			/* Argument of primitive		*/

  SHORT line_no;		/* Line where this definition occurs	*/

  char* package_name;		/* Package where this definition occurs */

  char* attributed_package_name;/* Package to which this definition is	*/
				/* attributed.				*/

  TYPE* ty;			/* Polymorphic type of this definition 	*/

  union {
    EXPR* rule;			/* Translation rule for this expansion	*/
  } u;

  LIST* visible_in;		/* List of packages where this defn	*/
				/* is visible				*/

  LIST* selection_info;		/* Selection info for a selector	*/

  struct part* next;		/* Next in chain			*/
} 
PART;

typedef PART ENTPART;
typedef PART EXPAND_PART;

#define QWRAP_INFO	arg	/* -1 normally, a nonnegative	 */
 				/* number for a pattern function */
				/* that does a PRIM_QUNWRAP.	 */

typedef struct patfun_expectation {
  TYPE*			type;		/* Polymorphic type of this	*/
					/*  expectation 		*/

  LIST*			visible_in;	/* List of packages that see	*/
					/* this expectation		*/

  char*			package_name;	/* The package where this 	*/
					/* expectation was created.	*/

  struct patfun_expectation* next;	/* Next in chain		*/
} 
PATFUN_EXPECTATION;

typedef struct {
  PATFUN_EXPECTATION* patfun_expectations; /* Chain of expectations for */
					   /* pattern functions.	*/

  EXPAND_PART*	patfun_rules;		/* Chain of pattern rules	*/

  EXPAND_PART*	expand_rules;		/* Chain of expand rules	*/
} 
EXPAND_INFO;


typedef struct expectation {
  unsigned              old: 1,	      /* 1 if this expectation existed      */
				      /* during first pass of type          */
				      /* inference        		    */

 			in_body: 1,   /* 1 if this expectation was made	    */
				      /* in the implementation part of the  */
				      /* main package.			    */

			irregular:1;  /* 1 if this is an irregular          */
				      /* expectation 			    */

  USHORT		line_no;      /* Line where this expectation occurs.*/

  char*                 package_name; /* Package where this expectation	    */
				      /* occurs				    */

  TYPE* 		type;         /* Polymorphic type of this           */
				      /* expectation			    */

  STR_LIST*		visible_in;   /* List of names of packages where    */
				      /* this expectation is visible 	    */

  struct expectation*   next;         /* Next in chain 			    */
} 
EXPECTATION;


typedef struct global_id_cell {
  EXPECTATION* 		expectations; /* Chain of expectations 		    */

  TYPE*                 container;    /* Polymorphic type containing all    */
				      /*  types of this id 		    */

  ENTPART*		entparts;     /* Descriptions of definitions        */

  EXPAND_INFO*		expand_info;  /* Expand and pattern function info   */

  DESCRIP_CHAIN*	descr_chain;  /* Description info 		    */

  ROLE_CHAIN*           role_chain;   /* Role info 			    */

  TYPE_LIST*		restriction_types;
  				      /* List of restriction types for       */
				      /* irregular expectations.	     */
} 
GLOBAL_ID_CELL;


typedef struct global_expectation_cell {
  USHORT 	 	line;		/* Line of this expectation	  */

  char* 		id;		/* Id being expected		  */

  char*			package_name;	/* Name of the package that makes */
					/* this expectation.		  */

  TYPE* 		type;		/* Polymorphic type of this	  */
					/* expectation			  */

  ROLE* 		role;		/* Role of this expectation	  */

  STR_LIST* 		visible_in;	/* Visibility of this expectation */

  MODE_TYPE* 		mode;		/* Mode of this expecation	  */

  struct global_expectation_cell* next;	/* Next in chain		  */
} 
GLOBAL_EXPECTATION_CELL;


typedef enum 
{ISSUE_DCL_DEFER, 
 ISSUE_MISSING_DEFER, 
 EXPECT_DCL_DEFER, 
 ATTACH_PROP_DEFER,
 DESCRIPTION_DEFER
} 
DEFER_TAG_TYPE;

typedef struct deferred_dcl_type {
  DEFER_TAG_TYPE tag;
  struct deferred_dcl_type *next;
  union {

    struct {		/* ISSUE_DCL_DEFER */
      EXPR* ex;
      int  kind;
      MODE_TYPE* mode;
    } issue_dcl_fields;

    struct {		/* EXPECT_DCL_DEFER, ISSUE_MISSING_DEFER */
      char* name;
      TYPE* type;
      ROLE* role;
      SHORT  context;
      SHORT  main_ctxt;
      USHORT  line;
      MODE_TYPE* mode;
    } expect_dcl_fields;

    struct {		/* ATTACH_PROP_DEFER */
      char* prop;
      char* name;
      TYPE* type;
    } attach_prop_fields;
  } fields;
} 
DEFERRED_DCL_TYPE;

typedef struct name_type {
  char* name;
  TYPE* type;
  CODE_PTR type_instrs;
}
NAME_TYPE;

typedef struct expect_table {
  TYPE* 	type;
  STR_LIST* 	visible_in;
  char* 	package_name;
} 
EXPECT_TABLE;


/************************************************************************
 *			MACHINE GLOBAL TABLE				*
 ************************************************************************/

/*-----------------------------------------------------------------------*
 * Type global_header is the type of each entry in outer_bindings.  It   *
 * gives the global id name,  plus pointers to the monomorphic and	 *
 * polymorphic tables for that name. 					 *
 *-----------------------------------------------------------------------*/

typedef struct global_table_node {
  UBYTE 			mode;     	/* Mode of this defn */

  Boolean			dummy;		/* True if this is a dummy
						   entry */

  SHORT 			packnum;        /* Number of package in which
						   this defn occurs */

  TYPE*	 			V;		/* Class of this defn */

  LONG 				offset;		/* Offset in package of code 
						   to evaluate this defn */

  CODE_PTR			ty_instrs;      /* Instructions to build the
						   type of this id */

  struct global_table_node*	next;		/* Next entry for this id */

  struct global_table_node*	start;		/* Place to start search if
						   a copy, or NULL if not a
						   copy. */
} 
GLOBAL_TABLE_NODE;

struct global_header {
  char*                     name;
  HASH2_TABLE*              mono_table;
  struct global_table_node* poly_table;
};

typedef struct global_header HUGEPTR GLOBAL_HEADER_PTR;


/************************************************************************
 * 			ACTIVATIONS, CONTINUATIONS			*
 ************************************************************************/

/********************************************************************
 * See machstrc/actvn.doc for a description of types ACTIVATION and *
 * CONTINUATION.						    *
 *								    *
 * If ACTIVATION or CONTINUATION are changed, modify 		    *
 * machstrc/actvn.doc.						    *
 ********************************************************************/

typedef struct activation {
  UBYTE			kind;          	  /* Used by lazy evaluation and to  */
					  /* terminate threads. 	     */

  UBYTE			progress;	  /* 1 if have recently made 	     */
					  /* progress on this activation,    */
					  /* 0 if it was recently blocked.   */

  UBYTE			actmark;	  /* For garbagd collection.	     */

  UBYTE		        num_entries;      /* Number of entries used in env   */

  LONG 	        	st_depth;         /* Depth of the run-time stack.    */
					  /* (Length of continuation chain.) */

  LONG                  ref_cnt;          /* For ref-count management        */

  LONG                  in_atomic;        /* Count of number of atomic       */
					  /* constructs currently inside     */

  LIST_LIST*		type_binding_lists;/* Bindings for fetching types.   */

  ENTS_LIST*            exception_list;   /* Stack of values of exception    */

  struct stack*		stack;            /* Stack, for expressions          */

  struct state*		state_a;	  /* The current state 		     */

  struct list*		state_hold;       /* Preserved states		     */

  struct environment*	env;	          /* The environment     	     */

  struct control*	control;          /* The control          	     */

  struct list*		embedding_tries;  /* This is a stack of identities   */
					  /* of the try control frames	     */
					  /* that belong to                  */
					  /* constructs that this	     */
					  /* activation is inside.	     */

  struct list*		embedding_marks;  /* This is a stack of identities   */
					  /* of the mark control frames	     */
					  /* that belong to                  */
					  /* constructs that this	     */
					  /* activation is inside.	     */

  struct trap_vec*      trap_vec_a;	  /* The current trap vector	     */

  struct list*		trap_vec_hold;    /* Trap vectors pushed by trap and */
  					  /* untrap expressions.  	     */

  char*			name;		  /* Name of running function 	     */

  char*			pack_name;	  /* Name of package that contains   */
					  /* this function.		     */

  CODE_PTR              result_type_instrs;  /* Instructions to build the    */
  					     /* type of the result of this   */
  					     /* computation.                 */

  struct continuation*	continuation;     /* List of continuations (run-time */
					  /* stack) 			     */

  CODE_PTR		program_ctr;      /* Current program counter 	     */

  LIST*			coroutines;       /* Sibling coroutines 	     */
} 
ACTIVATION;

typedef struct continuation {
  LONG 			ref_cnt;
  UBYTE		 	num_entries;
  UBYTE		        mark;
  TYPE_LIST*		type_binding_lists;
  ENTS_LIST*		exception_list;
  struct continuation*	continuation;
  struct environment*	env;
  CODE_PTR		program_ctr;
  char*			name;
  CODE_PTR		result_type_instrs;
} 
CONTINUATION;


/************************************************************************
 * 			MACHINE STRUCTURES				*
 ************************************************************************/

/************************ Controls **********************************/

union ctl_or_act {
  struct control*		ctl;   /* DOWN_CONTROL */
  struct activation* 		act;
};


typedef struct control {
  LONG 		 		ref_cnt;   /* For reference-count management */

  UBYTE				info;      /* Tells about this node and how  */
					   /* it relates to its children.    */

  UBYTE	                	mark;      /* For garbage collection.        */

  char				recompute; /* 0 normally, 		     */
					   /* 1 if should not memoize the    */
					   /* the result of this computation */
					   /* Only used for controls of      */
					   /* lazy and lazy list promises.   */

  LONG				identity;  /* Identity for a MARK_F or TRY_F */
					   /* node.			     */

  union ctl_or_act              left;	   /* Left child or parent 	     */

  union ctl_or_act              right;     /* Right child or other child     */
} 
CONTROL;

typedef CONTROL DOWN_CONTROL;
typedef CONTROL UP_CONTROL;
#define PARENT_CONTROL left.ctl


/**************************** Environments *******************************/

struct envcell {
  ENTITY val;
  int refs;
};

typedef struct environment {
  LONG 					ref_cnt;
  USHORT	 			descr_num;
  UBYTE					mark;  /* For garbage collection */
  UBYTE					kind;
  UBYTE					sz;
  UBYTE					most_entries;
  UBYTE					num_link_entries;
  CODE_PTR				pc; 
  struct environment* 			link;
  struct envcell     			cells[1];  	
					/* There are env_size[sz] */
  					/* cells in the cells array */
} 
ENVIRONMENT;

/*----------------------------------------------------------------*
 * An env_descr is a list of records describing local identifiers *
 *----------------------------------------------------------------*/

struct env_descr {
  LONG pc_offset;		/* program counter value where this id is
				   written into the environment */

  int env_offset;		/* Offset in the environent where this id
				   is located */

  char* name;			/* Name of this id */

  CODE_PTR type_instrs;         /* Instructions that construct the type of
				   this id, and leave it on the type stack.
				   They must be executed in the same 
				   environment as this id occurs in. */

  struct env_descr* next;	/* Next in the list */
};


/*********************** Stacks *********************************/


typedef struct stack {
  SHORT             ref_cnt;
  UBYTE		    top;		    /* Index of top occupied cell. */
  UBYTE    	    mark;
  struct stack*     prev;		    /* Next frame down. */
  ENTITY            cells[NUM_STACK_CELLS]; /* The cells of this frame */
} 
STACK;


/************************ States *****************************/

typedef struct state {
  LONG 		ref_cnt;

  UBYTE 	kind;	    /* LEAF_STT or SMALL_LEAF_STT or INTERNAL_STT */

  UBYTE 	height;     /* Height of this subtree. */

  UBYTE 	mark;	    /* For garbage collection. */

  LONG key;		    /* A box number */

  union {
    struct {
      /* INTERNAL_STT */
      ENTITY content;		/* Content of box key */
      struct state* left;	/* Left subtree */
      struct state* right;	/* Right subtree */
    } internal;

    struct {
      /* LEAF_STT */		/* Min box number is key */
      LONG maxb;		/* maxb-key+1 boxes stored here */
      ENTITY* contents;		/* Array of box contents, from key to max */
    } leaf;

    /* SMALL_LEAF_STT */
    ENTITY content;		/* Content of box key */

  } u;
} 
STATE;

#define LEAF_STT     	1
#define SMALL_LEAF_STT 	2
#define INTERNAL_STT 	3
#define ST_CONTENT  	u.internal.content
#define ST_LEFT	    	u.internal.left
#define ST_RIGHT    	u.internal.right
#define ST_MAX      	u.leaf.maxb
#define ST_CONTENTS 	u.leaf.contents
#define ST_SMCONTENT    u.content


/********************** Handling traps ************************/

typedef struct trap_vec {
  LONG ref_cnt;
  LONG component[1];    /* There are actually trap_vec_size members of
			   this array. */
} 
TRAP_VEC;


/********************** Info for print_rts **********************/

typedef struct {
  TYPE** type_stack;
  TYPE** type_storage;
  int    type_stack_occ;
  int    type_storage_occ;
} TYPE_HOLD;

typedef struct {
  FILE*        rts_file;
  long         rts_file_chars_left;
  Boolean      printing_rts;
  HASH2_TABLE* rts_box_table;
  HASH2_TABLE* rts_place_table;
  Boolean      special_condition;
  Boolean      interrupt_occurred;
  Boolean      do_profile;
  TYPE_HOLD    type_info;
} RTS_INFO;

typedef struct {
  CODE_PTR pc;
  STR_LIST *breakable_funs;
  STR_LIST *breakable_fun_packages;
  STR_LIST *breakable_packages;
  STR_LIST *break_execute_packages;
  char* break_apply_name;
  int suppress_breaks;
  int break_mode;
  int vis_let_offset;
  Boolean show_val;
  Boolean break_all_funs;
  Boolean break_lets;
  Boolean break_applies;
  Boolean break_lazy;
  Boolean break_executes;
  Boolean break_names;
  Boolean break_failures;
  Boolean break_assigns;
  Boolean break_prints;
  ENTITY result;
  CODE_PTR result_type_instrs;
} BREAK_INFO;
