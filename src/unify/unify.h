/**************************************************************
 * File:    unify/unify.h
 * Purpose: Exports from uniy.c: unification and related functions
 * Author:  Karl Abrahamson
 **************************************************************/


#define DISJOINT_OV		0
#define BAD_OV			1
#define CONTAINED_IN_OV		2
#define CONTAINS_OV		3
#define EQUAL_OV		4

#define CONTAINS_OR_BAD_OV	 CONTAINS_OV
#define EQUAL_OR_CONTAINED_IN_OV EQUAL_OV


#define UNIFY(x,y,r) 		unify_u(&(x), &(y), r)

#define BASIC_UNIFY(x,y,r,t) 	basic_unify_u(&(x), &(y), r, t)

#define FIND_U(t)\
  (((t) == NULL || TKIND(t) < FIRST_BOUND_T) ? (t) : find_u(t))

#define FIND_U_NONNULL(t)\
  ((TKIND(t) < FIRST_BOUND_T) ? (t) : find_u(t))

#define IN_PLACE_FIND_U_NONNULL(t)\
  if(TKIND(t) >= FIRST_BOUND_T) (t) = find_u(t)

#define IN_PLACE_FIND_U(t)\
  if((t) != NULL && TKIND(t) >= FIRST_BOUND_T) (t) = find_u(t)

/******************** From unify.c **********************/

extern LIST*   binding_list;
extern int     use_binding_list;

LIST*		finger_new_binding_list		(void);
void    	commit_new_binding_list		(LIST *mark);
void    	clear_new_binding_list_u	(void);
void    	have_replaced_lower_bounds	(TYPE *V, TYPE_LIST *lwb);

void    	undo_bindings_u	(LIST *mark);
TYPE*   	find_u		(TYPE *a);
TYPE*   	find_mark_u	(TYPE *a, Boolean *marked);
Boolean		bind_u		(TYPE *a, TYPE *b, Boolean record,
				 TYPE_LIST *cylist);
Boolean 	basic_unify_u	(TYPE **aa, TYPE **bb, Boolean record,
				 TYPE_LIST *cylist);
Boolean 	unify_u		(TYPE **aa, TYPE **bb, Boolean record);
Boolean 	is_member_tf    (TYPE *t, CLASS_TABLE_CELL *ctc);
Boolean 	disjoint	(TYPE *A, TYPE *B);

/******************** From overlap.c **********************/

#ifdef TRANSLATOR
int	overlap_u			(TYPE *a, TYPE *b);
int	half_overlap_u			(TYPE *a, TYPE *b);
TYPE*   get_restrictively_bound_var	(void);
#endif

/******************* From complete.c ***********************/

#ifdef TRANSLATOR
extern Boolean missing_type_use_fictitious_types;

TYPE* 	   missing_type			(TYPE_LIST *source, TYPE *t);
TYPE_LIST* reduce_type_list_with	(TYPE_LIST *keep, TYPE_LIST *l);
#endif


