/***************************************************************
 * File:    utils/lists.h
 * Purpose: Linked lists
 * Author:  Karl Abrahamson
 ***************************************************************/

/****************************************************************
 * 		NULL LIST AND DISCRIMINATORS			*
 ****************************************************************/

#define NIL 		((LIST *) 0)
#define STR_L		1
#define STR1_L		2
#define STR2_L		3
#define STR3_L		4
#define EXPR_L		5
#define LIST_L		6
#define INT_L		7
#define TYPE_L		8
#define TYPE1_L		9
#define TYPE2_L		10  /* not ref counted */
#define CTC_L		11
#define FILE_L		12
#define SEXPR_L		13
#define STYPE_L		14
#define HASH2_L		15
#define EXPECT_L	16
#define LTYPE_L		17
#define PAT_L		18
#define PAIR_L		19
#define APPL_L		20
#define ANON_L		21
#define STATE_L         22
#define ACT_L      	23
#define ENTS_L		24
#define SHORTS_L	25
#define BIS_L		26
#define ENV_DESCR_L	27
#define TRAP_VEC_L	28
#define STATES_L        29
#define CUC_L		30
#define CONTROL_L	31
#define NAPPL_L		32
#define BUF_L		33
#define ROLE_L		34
#define EXPECT_TABLE_L  35
#define EXPECT_TABLE1_L 36
#define NAME_TYPE_L	37
#define HASH_KEY_L	38
#define MODE_L		39
#define CHOOSE_INFO_L	40

#ifdef GCTEST
# define LKIND(l)	(lkindf(l))
#else
# define LKIND(l)	((l)->kind)
#endif


/************************************************************************
 * 			FUNCTIONS					*
 ************************************************************************/

#ifndef SET_LIST
# define SET_LIST(x,t)       set_list(&(x), t) 
# define FRESH_SET_LIST(x,t) bump_list(x = t)
# define bmp_list(l)	    (l)->ref_cnt++
#endif

#define push_int(s,x) 		set_list(&(s), int_cons(x,s))
#define push_str(s,x)		set_list(&(s), str_cons(x,s))
#define push_type(s,x)		set_list(&(s), type_cons(x,s))
#define push_expr(s,x)		set_list(&(s), expr_cons(x,s))
#define push_file(s,x)		set_list(&(s), file_cons(x,s))
#define push_list(s,x)		set_list(&(s), list_cons(x,s))
#define push_mode(s,x)		set_list(&(s), mode_cons(x,s))
#define push_act(s,x)           set_list(&(s), act_cons(x,s))
#define push_env_descr(s,x)     set_list(&(s), env_descr_cons(x,s))
#define push_shorts(s,m,n)      push_two_shorts(&(s), m, n)
#define push_buf(s,b)		set_list(&(s), buf_cons(b,s))
#define push_choose_info(s,i)	set_list(&(s), choose_info_cons(i,s))
#define clear_stack(s)		s = NIL
#define empty_stack(s)		((s) == NIL)
#define top_int(s)		((s) == NIL ? 0L : (s)->head.i)
#define top_i(s)		((s) == NIL ? 0 : (int)((s)->head.i))
#define top_str(s)		((s) == NIL ? NULL_S : (s)->head.str)
#define top_type(s)		((s) == NIL ? NULL_T : (s)->head.type)
#define top_mode(s)		((s) == NIL ? NULL : (s)->head.mode)
#define top_expr(s)		((s) == NIL ? NULL_E : (s)->head.expr)
#define top_choose_info(s)	((s) == NIL ? NULL : (s)->head.choose_info)
#define top_file(s)		((s) == NIL ? NULL : (s)->head.file)
#define top_list(s)		((s) == NIL ? NIL : (s)->head.list)
#define top_env_descr(s)        ((s) == NIL ? NULL : (s)->head.env_descr)
#define top_buf(s)		((s) == NIL ? NULL : (s)->head.buf)

int	lkindf		(LIST *l);
LIST*	general_cons	(HEAD_TYPE h, LIST *t, int kind);
LIST* 	str_cons	(char *h, LIST *t);
LIST* 	type_cons	(TYPE *a, LIST *t);
LIST* 	stype_cons	(TYPE **a, LIST *t);
LIST* 	ctc_cons	(CLASS_TABLE_CELL *c, LIST *t);
LIST* 	int_cons	(LONG i, LIST *t);
LIST*   shorts_cons    (struct two_shorts ts, LIST *t);
LIST*	file_cons	(FILE *f, LIST *t);
LIST*	list_cons	(LIST *h, LIST *t);
LIST*	hash2_cons	(HASH2_TABLE *h, LIST *t);
#ifdef TRANSLATOR
 LIST*  mode_cons	(MODE_TYPE *h, LIST *t);
 LIST* 	expr_cons	(EXPR *e, LIST *t);
 LIST*  choose_info_cons(CHOOSE_INFO *i, LIST *t);
 LIST*  role_cons	(ROLE *r, LIST *t);
 LIST* 	exp_cons	(EXPECTATION *h, LIST *t);
 LIST*  buf_cons	(YY_BUFFER_STATE h, LIST *t);
#endif
#ifdef MACHINE
 LIST*    act_cons       (ACTIVATION *h, LIST *t);
 LIST*    state_cons     (STATE *h, LIST *t);
 LIST* 	  ents_cons	 (ENTITY *h, LIST *t);
 LIST*    env_descr_cons (struct env_descr *h, LIST *t);
 LIST*    trap_vec_cons  (TRAP_VEC *h, LIST *t);
#endif

void            push_two_shorts (LIST **s, int m, int n);

void		pop		(LIST **s);		 
void		noref_pop	(LIST **s);		 

void		bump_head	(int k, HEAD_TYPE x);
void		drop_head	(int k, HEAD_TYPE x);
void		set_head	(LIST *l, int k, HEAD_TYPE x);

LIST*		allocate_list	(void);
void		set_list	(LIST **x, LIST *t);
void 		bump_list	(LIST *l);
void 		drop_list	(LIST *l);

LIST*		list_subscript 	(LIST *l, int n);
int		list_length	(LIST *l);
LIST* 		reverse_list	(LIST *l);
LIST* 		append		(LIST *l1, LIST *l2);
INT_LIST* 	append_without_dups(INT_LIST *l1, INT_LIST *l2);
LIST* 		delete_string	(char *name, LIST *l);
LIST*		delete_str      (char *name, LIST *l);
LIST*    	copy_list       (LIST *l);
int		str_member	(CONST char *s, STR_LIST *l);
Boolean		str_memq	(CONST char *s, STR_LIST *l);
Boolean		int_member	(LONG n, INT_LIST *l);
STR_LIST*	str_list_intersect	(STR_LIST *a, STR_LIST *b);
STR_LIST*	str_list_difference	(STR_LIST *a, STR_LIST *b);
Boolean 	str_list_disjoint	(STR_LIST *a, STR_LIST *b);
STR_LIST*	str_list_union		(STR_LIST *a, STR_LIST *b);
Boolean 	str_list_subset		(STR_LIST *a, STR_LIST *b);
Boolean 	str_list_equal_sets	(STR_LIST *a, STR_LIST *b);
EXPR_LIST* 	merge_id_lists	(EXPR_LIST *s, EXPR_LIST *t);
EXPR_LIST* 	sort_id_list	(EXPR_LIST **l, int k);
void 	  	intersect_id_lists(EXPR_LIST *l1, EXPR_LIST *l2, 
				   EXPR_LIST **la, EXPR_LIST **lb);
void 		drop_hash_list   (HASH2_CELLPTR h);
