/**************************************************************
 * File:    classes.h
 * Purpose: Copy types
 * Author:  Karl Abrahamson
 **************************************************************/

/****************************************************************
 *			From type.c				*
 ****************************************************************/

TYPE_TAG_TYPE  tkindf	(TYPE *t);
TYPE* new_type		(TYPE_TAG_TYPE kind, TYPE *ty1);
TYPE* new_type2		(TYPE_TAG_TYPE kind, TYPE *ty1, TYPE *ty2);
TYPE* box_t		(TYPE *t);
TYPE* fam_mem_t		(TYPE *s, TYPE *t);
TYPE* list_t		(TYPE *t);
TYPE* pair_t		(TYPE *s, TYPE *t);	
TYPE* function_t	(TYPE *s, TYPE *t);
TYPE* type_var_t	(char *name);
TYPE* fam_var_t		(char *name);
TYPE* var_t		(CLASS_TABLE_CELL *ctc);
TYPE* primary_var_t	(CLASS_TABLE_CELL *ctc);
TYPE* primary_type_var_t(char *name);
TYPE* primary_fam_var_t	(char *name);
TYPE* wrap_var_t	(CLASS_TABLE_CELL *ctc);
TYPE* wrap_type_var_t	(char *name);
TYPE* wrap_fam_var_t	(char *name);
TYPE* type_id_t		(char *name);
TYPE* fam_id_t		(char *name);
TYPE* fictitious_tf	(CLASS_TABLE_CELL *ctc, int num);
TYPE* new_fictitious_tf(CLASS_TABLE_CELL *ctc);
TYPE* fictitious_wrap_tf(CLASS_TABLE_CELL *ctc, int num);
TYPE* new_fictitious_wrap_tf(CLASS_TABLE_CELL *ctc);
TYPE* wrap_tf		(CLASS_TABLE_CELL *ctc);
TYPE* primary_tf_or_var_t(CLASS_TABLE_CELL *ctc);
TYPE* tf_or_var_t	(CLASS_TABLE_CELL *ctc);
LPAIR make_lpair_t      (int label1, int label2);
LPAIR make_ordered_lpair_t(int l1, int l2);
void  init_types	(void);

extern TYPE* any_type;		/* `a				*/
extern TYPE* any_box;		/* Box(any_type			*/
extern TYPE* string_type;	/* [Char]			*/
extern TYPE* outfile_type;      /* Outfile(Char)		*/

#ifdef TRANSLATOR
extern TYPE* any_type2;		/* `b				*/
extern TYPE* any_list;		/* <any_type>			*/
extern TYPE* any_EQ;		/* EQ`a				*/
extern TYPE* any_pair;		/* (`a, `b)			*/
extern TYPE* any_pair_rev;	/* (`b, `a)			*/
extern TYPE* cons_type;         /* (`a, [`a]) -> [`a] 		*/
extern TYPE* content_type;      /* Box(`a) -> `a	      	*/
extern TYPE* assign_type;	/* (Box(`a), `a) -> () 		*/
extern TYPE* idf_type;		/* `a -> `a		 	*/
extern TYPE* forget_type;	/* `a -> `b		 	*/
extern TYPE* show_apply_type;   /* (String, String) -> String 	*/
extern TYPE* bad_type;		/* used for erroneous types   	*/
extern TYPE* exc_to_bool_type;  /* ExceptionSpecies -> Boolean 	*/
extern TYPE* exc_to_str_type;   /* ExceptionSpecies -> String 	*/
extern TYPE* str_to_exc_type;   /* String -> ExceptionSpecies 	*/
#endif

extern LPAIR  NULL_LP;         /* pair (0,0) */
extern LPAIR  NOCHECK_LP;      /* pair (-1, -1) */
extern RTYPE  NULL_RT;	       /* (NULL,NULL) */

extern intset fam_tkind_set, fam_type_tkind_set, fam_var_tkind_set;
extern intset wrap_tkind_set, primary_tkind_set;


/****************************************************************
 *			From typeutil.c 			*
 ****************************************************************/

extern TYPE_LIST* seen_record_list;

Boolean   type_id_or_var        (TYPE *t);
Boolean	  good_class_union_pair	(TYPE *ty);
Boolean   is_hermit_type  	(TYPE *t);
Boolean   full_type_equal	(TYPE *s, TYPE *t);
void      mark_seen		(TYPE *t);
void	  reset_seen		(void);
void      replace_null_vars	(TYPE **tt);
TYPE* 	  type_mem      	(TYPE *t, TYPE_LIST *l, int tag);
void      bump_hash_type        (HASH2_CELLPTR h);
void      drop_hash_type  	(HASH2_CELLPTR h);
Boolean   occurs_in             (TYPE *a, TYPE *b);
Boolean   type_var_mem		(TYPE *V, TYPE_LIST *L);
TYPE_LIST* var_list_intersect	(TYPE_LIST *a, TYPE_LIST *b);
TYPE_LIST* add_var_to_list	(TYPE *V, TYPE_LIST *L);
TYPE*     type_join		(TYPE *A, TYPE *B);
TYPE*     type_meet		(TYPE *A, TYPE *B);

/****************************************************************
 *			From m_tyutil.c 			*
 ****************************************************************/

#ifdef MACHINE
void 	  set_in_binding_list	 	(void);
void 	  get_out_binding_list	 	(void);
void 	  set_in_binding_list_to 	(TYPE_LIST *bl);
void 	  get_out_binding_list_to	(TYPE_LIST **bl);
TYPE*	  force_ground		 	(TYPE *t);
TYPE* 	  freeze_type		 	(TYPE *t);
ENTITY    dynamic_default_ent    	(ENTITY tt);
#endif

/****************************************************************
 *			From t_tyutil.c 			*
 ****************************************************************/

#ifdef TRANSLATOR
Boolean    is_boolean_type	(TYPE *t);
Boolean    count_occurrences_1	(TYPE *t);
Boolean    count_occurrences_2	(EXPR_LIST *l);
void 	   clear_seenTimes_1	(TYPE *t);
void 	   clear_seenTimes_2	(EXPR_LIST *l);
TYPE*      extensible_var_in    (TYPE *t, CLASS_TABLE_CELL *C);
Boolean    any_overlap          (TYPE *t, TYPE_LIST *l);
TYPE_LIST* add_type_to_list	(TYPE *t, TYPE_LIST *L, int tag);
TYPE* 	   bind_for_no_fictitious(TYPE *t);
Boolean    has_hidden_id_t	(TYPE *t);
void	   clear_locs_t		(TYPE *t);
void       mark_no_split	(TYPE *t);
void       reset_frees		(TYPE *t);
TYPE*      containing_class	(TYPE_LIST *a);
TYPE*      contain_type		(TYPE *A, TYPE *B);
TYPE_LIST* intersect_type_lists (TYPE_LIST *a, TYPE_LIST *b);
void 	   put_vars_t		(TYPE *t, HASH1_TABLE **tbl);
void 	   put_vars2_t		(TYPE *t, HASH2_TABLE **tbl);
void       count_vars_t		(TYPE *t, HASH2_TABLE **tbl);
TYPE_LIST* marked_vars_t	(TYPE *t, TYPE_LIST *rest);
int 	   dispatchable_count_t (TYPE *V, TYPE *t, int context);
TYPE*	   find_var_t		(TYPE *t, HASH1_TABLE *tbl);
TYPE_LIST* reduce_type_list(TYPE_LIST* l, TYPE* t);
Boolean    is_polymorphic	(TYPE* t);
void	   default_boxes_t	(TYPE *t);
Boolean    var_in_context	(TYPE *t, int side);
Boolean    restrict_families_t  (TYPE *t);
Boolean    commit_restrict_families_t  (TYPE *t);
void	   mark_unrestrictable_t(TYPE *t);
Boolean    unify_with_eq	(TYPE **t);
Boolean    eq_restrict		(TYPE *t);
TYPE*	   substitute_at_posn_t (TYPE *t, TYPE *subst, INT_LIST *posnlist);
RTYPE      list_to_type_expr    (TYPE_LIST *types, ROLE_LIST *roles,
				 int mode);
struct rtlist_pair
	   make_rolelist_type   (STR_LIST *l, RTYPE rt,
				 TYPE_LIST *rest_types,
				 ROLE_LIST *rest_roles);
void       check_for_constrained_dynamic_var(TYPE *t, EXPR *the_fun);
TYPE_LIST* copy_vars_to_list_t	(TYPE *t, TYPE_LIST *l, TYPE_LIST *skip);
TYPE*      eval_type		(TYPE *t, TYPE_LIST *boundvars);
TYPE_LIST* get_true_lower_bounds(LIST *L);
TYPE*	  class_mem      	(TYPE *V, TYPE_LIST *l);
TYPE*     reduce_type           (TYPE *t);
TYPE* 	  member_of_class	(TYPE *t);
#endif

/****************************************************************
 *			From bring.c 				*
 ****************************************************************/

#ifdef TRANSLATOR
TYPE*	   cast_for_bring_t	(TYPE *t, MODE_TYPE *mode, int line);
#endif

/************************************************************************
 *			From copytype.c  				*
 ************************************************************************/

TYPE* copy_type		(TYPE *t, int mode);
TYPE* copy_type1	(TYPE *t, HASH2_TABLE **ty_b, int mode);
TYPE* copy_type_node	(TYPE *t);
TYPE* copy_type_except  (TYPE *t, TYPE_LIST *tl1, TYPE_LIST *tl2, int mode);

/************************************************************************
 *			From printty.c 					*
 ************************************************************************/

extern int show_hermit_functions;

int  fprint_ty				(FILE *f, TYPE *t);
int  fprint_ty_unmarked			(FILE *f, TYPE *t);
int  fprint_ty_with_constraints_indented(FILE *f, TYPE *t, int n);
int  fprint_ty_without_constraints	(FILE *f, TYPE *t);
int  fprint_ty_without_constraints_unmarked(FILE *f, TYPE *t);
void print_namelist			(FILE *f, STR_LIST *l);

#ifdef MACHINE
void sprint_ty				(char *s, int len, TYPE *t);
#endif

#ifdef TRANSLATOR
void print_rt				(FILE *f, TYPE *t, ROLE *r);
void print_rt_with_constraints_indented (FILE *f, TYPE *t, ROLE *r, int n);
void print_behaviors			(FILE *f, STR_LIST *l);
void fprint_role			(FILE *f, ROLE *r);
void fprint_tag_type    		(FILE *f, TYPE *t, TYPE *mirror, 
					 Boolean print_for_debug);
void print_link_labels  		(FILE *f, int label1, int label2,
					 CLASS_TABLE_CELL *ctc);
void print_labeled_type 		(FILE *f, CLASS_TABLE_CELL *ctc, 
					 LPAIR lp);
#endif

#ifdef FOS
void begin_printing_types		(FOS *f, PRINT_TYPE_CONTROL *c);
void begin_printing_alt_types           (FOS *f, PRINT_TYPE_CONTROL *c);
void begin_printing_types_unmarked	(FOS *f, PRINT_TYPE_CONTROL *c);
void begin_printing_alt_types_unmarked  (FOS *f, PRINT_TYPE_CONTROL *c);
void end_printing_types  		(PRINT_TYPE_CONTROL *c);
void print_ty1_without_constraints	(TYPE *t, PRINT_TYPE_CONTROL *c);
void print_ty1_with_constraints		(TYPE *t, PRINT_TYPE_CONTROL *c);
void print_ty1_with_constraints_indented(TYPE *t, PRINT_TYPE_CONTROL *ctl, 
					 int n);
void print_rt1_without_constraints	(TYPE *t, ROLE *r, 
					 PRINT_TYPE_CONTROL *c);

#ifdef TRANSLATOR
void print_rt1_with_constraints		(TYPE *t, ROLE *r, 
					 PRINT_TYPE_CONTROL *c);
void print_rt1_with_constraints_indented
      					(TYPE *t, ROLE *r, 
					 PRINT_TYPE_CONTROL *ctl, int n);
#endif

#endif /* def FOS */

/************************************************************************
 * 			From aloctype.c and allocors.c			*
 ************************************************************************/

#ifndef SET_TYPE
# define SET_TYPE(x,t)       set_type(&(x), t) 
# define FRESH_SET_TYPE(x,t) bump_type(x = t)
# define bmp_type(t)	     (t)->ref_cnt++
#endif

void set_type		(TYPE **x, TYPE *t);
void bump_type		(TYPE *t);
void drop_type		(TYPE *t);

void bump_role		(ROLE *r);
void drop_role		(ROLE *r);
void set_role		(ROLE **x, ROLE *r);
void bump_rtype		(RTYPE r);
void drop_rtype		(RTYPE r);
