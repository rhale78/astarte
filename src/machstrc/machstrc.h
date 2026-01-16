/**********************************************************************
 * File:    machstrc/machstrc.h
 * Purpose: Describe operations for machine structures (stack, control, etc.)
 * Author:  Karl Abrahamson
 **********************************************************************/


/************************* Controls ***************************/

/************************************************************************
 * The following are used in the kind field of control nodes to tell    *
 * what kind of node is represented.  See control.c for details.	*
 *									*
 * Code assumes that MIX_F < BRANCH_F < TRY_F < TRYTERM_F < TRYEACH_F < *
 * TRYEACHTERM_F < MARK_F, and that the branch/mix codes are		*
 * contiguous, and that the try codes are contiguous.		        *
 *									*
 * If the following are modified, modify ctl_kind_name in control.c.    *
 ************************************************************************/

#define MIX_F			0	/* A mix node */
#define BRANCH_F		1	/* A stream node */
#define TRY_F			2	/* A try node */
#define TRYTERM_F		3	/* A try-term node */
#define TRYEACH_F		4	/* A try-each node */
#define TRYEACHTERM_F		5	/* A try-each-term node */
#define MARK_F			6	/* Marking a point for a cut */

/************************************************************************
 * The following are used to classify control nodes by code.		*
 ************************************************************************/

#define IS_TRY(k) 	     (((k) >= TRY_F) && ((k) <= TRYEACHTERM_F))
#define IS_MIX(k)	     ((k) == MIX_F)
#define IS_THREAD(k)	     ((k) <= BRANCH_F)

/************************************************************************
 * Each union ctl_or_act value coa is generally accompanied by a value 	*
 * that is CTL_F is coa is a control, and is 1 if coa is an activation.	*
 * In some cases, coa is 0 for a control and nonzero for an activation.	*
 * Using if(coa_kind == CTL_F) to test the kind is guaranteed to work	*
 * in either case.							*
 *									*
 * See control.c for the meaning of LEFT_F, RIGHT_F.			*
 ************************************************************************/

#define CTL_F			0
#define ACT_F			1
#define LEFT_F			0
#define RIGHT_F			1

/************************************************************
 * The following mask values select fields of the info	    *
 * field of a control node.  See control.c for a 	    *
 * discussion of the info field.			    *
 ************************************************************/

#define KIND_CTLMASK		7    /* Must be at low end */
#define LCHILD_CTLMASK		8
#define RCHILD_CTLMASK		16
#define UPLINK_CTLMASK		32

#define LCHILD_CTLSHIFT		3    /* Shift distance to lchild bit */
#define RCHILD_CTLSHIFT		4    /* Shift distance to rchild bit */

/****************************************************************
 * LCHILD_IS_ACT(c) is nonzero if the left child of control  	*
 * node c is an activation, and is zero if the left child is 	*
 * a control.  							*
 *								*
 * TRUE_LCHILD_KIND(c) is similar to LCHILD_IS_ACT, but is 1    *
 * if the left child is an activation, and 0 if a control.	*
 *								*
 * RCHILD_IS_ACT(c) is nonzero if the right child of control  	*
 * node c is an activation, and is zero if the right child is 	*
 * a control.  							*
 *								*
 * TRUE_RCHILD_KIND(c) is similar to RCHILD_IS_ACT, but is 1    *
 * if the right child is an activation, and 0 if a control.	*
 *								*
 * UPLINK_IS_FROM_RIGHT(c) is nonzero if the uplink to mix	*
 * or choose-mix node c (coming up from the child of this node) *
 * is from the right child, and is 0 if	the uplink is from the  *
 * left child.							*
 ****************************************************************/

#define LCHILD_IS_ACT(c)	((c)->info & LCHILD_CTLMASK)
#define TRUE_LCHILD_KIND(c)     (LCHILD_IS_ACT(c) >> LCHILD_CTLSHIFT)
#define RCHILD_IS_ACT(c)	((c)->info & RCHILD_CTLMASK)
#define TRUE_RCHILD_KIND(c)     (RCHILD_IS_ACT(c) >> RCHILD_CTLSHIFT)
#define UPLINK_IS_FROM_RIGHT(c)  ((c)->info & UPLINK_CTLMASK)


#ifdef GCTEST
# define CTLKIND(c) (ctlkindf(c))
#else
# define CTLKIND(c) (((c)->info) & KIND_CTLMASK)
#endif

#define branch_c(pc)	  push_branch_c(pc, BRANCH_F)
#define mix_c(pc)	  push_branch_c(pc, MIX_F | UPLINK_CTLMASK)
#define try_c(pc)	  push_try_c(pc, TRY_F)
#define tryeach_c(pc)	  push_try_c(pc, TRYEACH_F)
#define tryterm_c(pc)	  push_try_c(pc, TRYTERM_F)
#define tryeachterm_c(pc) push_try_c(pc, TRYEACHTERM_F)

extern union ctl_or_act NULL_CTL_OR_ACT;

LONG	    mark_c			(void);
LONG        push_try_c			(CODE_PTR c, int kind);
void        push_branch_c		(CODE_PTR c, int kind);
void 	    start_ctl_or_act    	(int k, union ctl_or_act a, 
					 UP_CONTROL *parent, LONG *l_time);
void 	    install_activation_c	(ACTIVATION *a, UP_CONTROL *c, 
					 LONG *l_time);
void 	    move_into_control_c		(DOWN_CONTROL *c, UP_CONTROL *parent, 
					 LONG *l_time);
void        count_threads		(int kind, union ctl_or_act ca);
UP_CONTROL* shift_up_control_c  	(UP_CONTROL *c);
UP_CONTROL* maybe_shift_up_control_c	(UP_CONTROL *c);
UP_CONTROL* get_up_control_c		(DOWN_CONTROL *c, UP_CONTROL *parent, 
			    		 ACTIVATION **act, LONG *l_time);
UP_CONTROL* get_up_control_from_coa_c	(union ctl_or_act coa, int coa_kind,
				     	 UP_CONTROL *parent, 
				     	 ACTIVATION **act, LONG *l_time);
UP_CONTROL* clean_up_control_c  	(UP_CONTROL *c, ACTIVATION *a);
UP_CONTROL* then_c			(UP_CONTROL *c, LONG id);
UP_CONTROL* one_then_c			(UP_CONTROL *c, LONG id);
UP_CONTROL* chop_c			(UP_CONTROL *c, LONG id);
void        cut_c			(ENTITY a);
CONTROL*    copy_control		(CONTROL *c, Boolean bumpleft);
Boolean     there_are_other_threads	(UP_CONTROL *c);
int	    ctlkindf			(CONTROL *c);
void 	    print_up_control		(FILE *f, UP_CONTROL *c, int n);
void 	    print_down_control		(FILE *f, DOWN_CONTROL *c, 
					 char *pref, int n);


/********************* Stacks ****************************/

#define the_stack    the_act.stack

/*********************************************************
 * Use top_stack() to peek at the top of the stack, and  *
 * not_empty_stack(s) to test that s is not empty.       *
 *********************************************************/

#define top_stack()  (the_stack->cells + the_stack->top)

#define not_empty_stack(s) ((s)->top != 0 || (s)->prev != NULL)

/*********************************************************
 * Macros POP_STACK() and PUSH_STACK() have the same     *
 * effect as functions pop_stack() and push_stack(), but *
 * are expanded inline.					 *
 *********************************************************/

#define POP_STACK() ((the_stack->top != 0) ? the_stack->cells[the_stack->top--] : *hard_pop_stack())

#define PUSH_STACK() ((the_stack->top != NUM_STACK_CELLS - 1) ? the_stack->cells + (++the_stack->top) : hard_push_stack())

ENTITY  pop_stack(void);
ENTITY* push_stack(void);

ENTITY* hard_push_stack	(void);
ENTITY* hard_pop_stack	(void);
ENTITY  pop_this_stack  (STACK **s);
ENTITY  pull_stack      (int k);
void    unpull_stack    (ENTITY e, int k);
STACK*  get_stack	(void);
void    make_stack	(STACK *s);
void    init_stack	(void);
STACK*  new_stack	(void);
void 	long_print_stack(STACK *s, int n);
void 	print_stack	(STACK *s);


/*************************** Environments ***************************/

#define LOCAL_ENV	1
#define GLOBAL_ENV	2

#ifdef GCTEST
# define ENVKIND(e) (envkindf(e))
#else
# define ENVKIND(e) ((e)->kind)
#endif

/****************************************************************
 * local_binding_env(e,k) produces the binding in environment   *
 * e, at offset k.  The binding is taken from the top		*
 * node in the chain e.						*
 *								*
 * gen_binding_env(e,n,k) produces the binding at offset k	*
 * in the environment n nodes down from the top in chain e.	*
 ****************************************************************/

#define		local_binding_env(e,k)	((e)->cells[k].val)
#define         gen_binding_env(e,n,k)  local_binding_env(scan_env(e,n),k)

ENTITY	 	global_binding_env	(ENVIRONMENT *env, int k);
ENVIRONMENT* 	scan_env		(ENVIRONMENT *env, int n);
void		basic_local_bind_env   	(int k, ENTITY a, CODE_PTR pc);
void		local_bind_env		(int k, ENTITY a, CODE_PTR pc);
void            copy_the_env		(int k);
void	        exit_scope_env		(int n);
TYPE*		type_env		(ENVIRONMENT *env, int n);
int 		envkindf		(ENVIRONMENT *env);
void 		long_print_env		(ENVIRONMENT *e, int n);
void 		print_env		(ENVIRONMENT *e, int ne);


/***************************** States *****************************/

#ifdef GCTEST
# define STKIND(s) (stkindf(s))
extern int stkindf(STATE *s);
#else
# define STKIND(s) ((s)->kind)
#endif

extern ULONG next_box_number;
extern STATE *initial_state;

#define DIGITS_PRECISION ((precision >= 0) ? precision : get_precision())

ENTITY  ast_box_flavor		(ENTITY b);
ENTITY  ast_new_box      	(void);
ENTITY  ast_new_place		(void);
ENTITY  ast_new_wrapped_box     (ENTITY herm, TYPE *ty);
LONG    new_boxes        	(LONG k);
LONG    box_rank		(ENTITY a);
ENTITY  ast_rank_box 		(ENTITY a);
void    make_bxpl_empty  	(ENTITY a, ENTITY *demon, ENTITY *oldcontent);
Boolean is_empty_bxpl   	(ENTITY bxpl);
ENTITY* ast_content_s		(LONG b, STATE *s, Boolean mode);
ENTITY  ast_content_stdf 	(ENTITY bxpl);
ENTITY  prcontent_stdf   	(ENTITY e);
ENTITY  ast_content_test_stdf 	(ENTITY bxpl);
ENTITY  ast_content		(ENTITY box);
ENTITY  on_assign_stdf		(ENTITY b, ENTITY d);
STATE*  ast_assign_s		(LONG b, ENTITY e, STATE *s, Boolean multref, 
				 ENTITY *demon, ENTITY *oldcontent);
STATE*  simple_ast_assign_s	(LONG b, ENTITY e, STATE *s, Boolean multref);
void    ast_assign		(ENTITY box, ENTITY e, ENTITY *demon, 
				 ENTITY *oldcontent);
void    simple_ast_assign	(ENTITY box, ENTITY e);
void    assign_bxpl      	(ENTITY a, ENTITY b, ENTITY *demon, 
			 	 ENTITY *oldcontent);
void    simple_assign_bxpl	(ENTITY a, ENTITY b);
void    assign_init_bxpl 	(ENTITY a, ENTITY b);
LONG    prec_digits		(LONG prec);
LONG    get_precision    	(void);
LONG    get_dec_precision    	(void);

void 	short_print_state	(STATE *s);
void 	print_state		(STATE *s, int n);
void 	print_states		(LIST *l);


/********************** Activations, Continuations ************************/

#ifdef GCTEST
# define ACTKIND(a) (actkindf(a))
extern int actkindf(ACTIVATION *a);
#else
# define ACTKIND(a) ((a)->kind)
#endif

/****************************************************************
 *			FIRST_TRY_ID				*
 *			FIRST_MARK_ID				*
 ****************************************************************
 * In these two functions, x is a list that would be the	*
 * embedding_tries or embedding_marks, as appropriate, list in	*
 * an activatsion.						*
 *								*
 * first_try_id(x) returns the first number in the try-identity	*
 * part of list x, or returns 0 if there is no try-identity.	*
 *								*
 * first_mark_id(x) returns the first number in the		*
 * mark-identity part of list x, or returns 0 if there is no	*
 * try-identity.						*
 ****************************************************************/

#define first_try_id(x)  ((x) == NULL ? 0 : (x)->head.i)
#define first_mark_id(x) ((x) == NULL ? 0 : (x)->head.i)

/****************************************************************
 *			PUSH_TRY				*
 *			POP_TRY					*
 ****************************************************************
 * Push_try(id) pushes try-identity id onto 			*
 * the_act.embedding_tries.					*
 *								*
 * Pop_try pops a try-identity from the_act.embedding_tries.	*
 ****************************************************************/

#define push_try(id) push_int(the_act.embedding_tries, id)
#define pop_try() pop(&(the_act.embedding_tries))

/****************************************************************
 *			PUSH_MARK				*
 *			POP_MARK				*
 ****************************************************************
 * Push_mark(id) pushes try-identity id onto 			*
 * the_act.embedding_tries.					*
 * 								*
 * Pop_mark pops a mark-identity from the_act.embedding_marks.	*
 ****************************************************************/

#define push_mark(id) push_int(the_act.embedding_marks, id)
#define pop_mark() pop(&(the_act.embedding_marks))

int         actkindf     	(ACTIVATION *a);
ACTIVATION* copy_activation	(ACTIVATION *a);
ACTIVATION* copy_the_act 	(void);
ACTIVATION* get_the_act  	(void);
TYPE_LIST*  get_act_binding_list(ACTIVATION *a);
TYPE_LIST*  get_cont_binding_list(CONTINUATION *a);
#ifdef DEBUG
 void        print_continuation	(CONTINUATION *a);
 void        print_activation	(ACTIVATION *a);
#endif

