/****************************************************************
 * File:    clstbl/classtbl.h
 * Purpose: Exports from table managers
 * Author:  Karl Abrahamson
 ****************************************************************/

/*******************************************************************
 *			CODES					   *
 *******************************************************************
 * The _CODE values are used in the code field of the class table  *
 * to tell what kind of thing is being described.  All of them     *
 * must be nonzero.  Each has a corresponding _TOK value.  The 	   *
 *_CODE values are used rather than _TOK values because there is   *
 * only one byte to hold them, and the _CODE values are smaller    *
 * than the _TOK values.		   			   *
 *								   *
 * Two of the _CODE values, FUN_CODE and PAIR_CODE, have no 	   *
 * corresponding _TOK value.  They are used for the entries that   *
 * describe functions and order pairs, respectively.		   *
 *******************************************************************/

#define UNKNOWN_CLASS_ID_CODE   MAKE_CODE(UNKNOWN_ID_TOK)
#define GENUS_ID_CODE           MAKE_CODE(GENUS_ID_TOK)
#define FAM_ID_CODE		MAKE_CODE(FAM_ID_TOK)
#define COMM_ID_CODE		MAKE_CODE(COMM_ID_TOK)
#define TYPE_ID_CODE            MAKE_CODE(TYPE_ID_TOK)
#define PAIR_CODE               MAKE_CODE(PAIR_CTC)
#define FUN_CODE                MAKE_CODE(FUN_CTC)


/****************************************************************
 * 			VARIABLES				*
 ****************************************************************/

extern int 		  next_class_num, next_cg_num;
extern Boolean		  altered_hierarchy;
extern Boolean		  report_extends;
extern Boolean 		  building_standard_types;
extern Boolean 		  ignore_class_table_full;
extern CLASS_TABLE_CELL** ctcs;
extern CLASS_TABLE_CELL** vctcs;
extern CLASS_TABLE_CELL*  Pair_ctc, *Function_ctc;
extern intset 		  fam_codes,  concrete_codes;
Boolean gen_code;


/****************************************************************
 * 			SPECIAL DISCRIMINATORS			*
 ****************************************************************/

#define COPROD_CTC	(COMM_ABBREV_TOK + 1)
#define FAMMEM_CTC	(COMM_ABBREV_TOK + 2)
#define PAIR_CTC	(COMM_ABBREV_TOK + 3)
#define FUN_CTC		(COMM_ABBREV_TOK + 4)


/****************************************************************
 * 			FUNCTIONS				*
 ****************************************************************/

char*           class_name      	 (int tok);
void		init_class_tbl_tm        (void);
void		clear_class_table_memory (void);
int             ctc_num			 (CLASS_TABLE_CELL *ctc);

Boolean		ancestor_tm	  (CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b);
void 		copy_ancestors_tm (CLASS_TABLE_CELL *a, CLASS_TABLE_CELL *b,
				   LPAIR labels);

Boolean         is_class_id	  (char *s, LONG hash);
void 		drop_hash_ctc	  (HASH2_CELLPTR h);
TYPE* 		get_tf_tm	  (char *s);
CLASS_TABLE_CELL* get_ctc_with_hash_tm(char *s, LONG hash);
CLASS_TABLE_CELL* get_ctc_tm	  (char *s);
CLASS_TABLE_CELL* get_new_ctc_tm  (char *s);

CLASS_TABLE_CELL* add_class_tm	(char *s, int c, int ex, Boolean d, 
				 Boolean opaque);
void 		extend_tm	(CTC_LIST *l, CLASS_TABLE_CELL *c, 
				 Boolean ex, MODE_TYPE *mode);
void		extend1_tm	(CLASS_UNION_CELL *t, CLASS_TABLE_CELL *c, 
				 Boolean gen, MODE_TYPE *mode);
void		extend1ctc_tm   (CLASS_TABLE_CELL *t, TYPE *arg, 
				 CLASS_TABLE_CELL *c, Boolean gen,
				 MODE_TYPE *mode);
void 		try_extend_tm	(char *upper, char *lower, TYPE *arg, 
				 Boolean gen, MODE_TYPE *mode);
void 		try_extends_tm	(STR_LIST *L, char *lower, TYPE *arg, 
				 Boolean gen, MODE_TYPE *mode);
TYPE* 		add_tf_tm	(char *s, TYPE *arg, Boolean g, 
				 Boolean opaque, Boolean partial);
TYPE*		expect_tf_tm	(char *id, TYPE *arg, Boolean opaque,
				 Boolean partial);

void		close_classes_p	(void);

void 		print_type_table   (void);
void 		print_lpair	   (LPAIR lp);

/****************************************************************
 *			From classinfo.c			*
 ****************************************************************/

void 		remember_class_info(char *C, char *superclass, int line);
EXPR*		retrieve_class_info(char *C, Boolean complain);




