/**********************************************************************
 * File:    evaluate/evaluate.h
 * Purpose: Expression evaluator
 * Author:  Karl Abrahamson
 **********************************************************************/


/************************************************************************
 * next1(c) returns the byte at address c, and sets c to the next byte. *
 ************************************************************************/

#define next1(c) 	*((c)++)

/***********************************************************************
 * next_byte returns the byte at address the_act.program_ctr, and sets *
 * the_act.program_ctr to the next byte after that.                    *
 ***********************************************************************/

#define next_byte	next1(the_act.program_ctr)

/**************************************************************
 * next_three_bytes returns the three-byte integer at address *
 * the_act.program_ctr, and moves the_act.program_ctr to just *
 * after that integer.					      *
 **************************************************************/

#define next_three_bytes  next_int_m(&(the_act.program_ctr))

/*************************************************************
 * TIME_STEP(t) decrements *t and, if *t has reached 0, sets *
 * failure = TIME_OUT_EX.				     *
 *************************************************************/

#define TIME_STEP(t)   if(--(*(t)) <= 0 && failure < 0) failure = TIME_OUT_EX

/**********************************************************************
 * MAYBE_TIME_STEP(t) decrements time_step_count.  If time_step_count *
 * has reached 0, it does a TIME_STEP(t), and sets time_step_count to *
 * TIME_STEP_COUNT_INIT.  This is used to do a TIME_STEP(t) every     *
 * TIME_STEP_COUNT_INIT times.					      *
 *								      *
 * Note: time_step_count is typically a local variable to some	      *
 * some function.  This macro refers to that local variable.	      *
 **********************************************************************/

#define MAYBE_TIME_STEP(t) if(--time_step_count <= 0) {TIME_STEP(t); time_step_count = TIME_STEP_COUNT_INIT;}

/*********************** From evaluate.c ******************************/

extern volatile Boolean   interrupt_occurred;
extern volatile int   	  special_condition; 
extern volatile int       failure;
extern volatile char      should_pause;
extern Boolean            perform_gc;
extern ENTITY             failure_as_entity;
extern ENTITY             last_exception;
extern int                in_show;
extern Boolean            do_profile;

void 	evaluate	(LONG *time_bound);
ENTITY  set_profile	(ENTITY sw);
ENTITY  prim_trace	(ENTITY sw);
void	init_evaluate	(void);


/********************** From evalsup.c ****************************/

/*******************************************************************
 * SET_EVAL(x,e,t) does 					   *
 *								   *
 *     x = eval(e,t)						   *
 *								   *
 * thus setting x to the result of evaluating e, with time-count   *
 * variable t.  This is a macro, since a test of whether e needs   *
 * evaluation is done before any call for efficiency.		   *
 *******************************************************************/

#define SET_EVAL(x,e,t)\
  x = MEMBER(TAG(e),all_lazy_tags)? eval1(e,t) : e

/*******************************************************************
 * SET_EVAL_FAILTO(x,e,t,lab) does 				   *
 *								   *
 *  SET_EVAL(x,e,t);						   *
 *  if(failure >= 0) goto lab;					   *
 *								   *
 * but it only does the test of failure if eval is called.	   *
 *******************************************************************/

#define SET_EVAL_FAILTO(x,e,t,lab)\
  if(MEMBER(TAG(e),all_lazy_tags)) {x = eval1(e,t); if(failure >= 0) goto lab;} else x = e

/*******************************************************************
 * SET_EVAL_WITH_TAG(x,e,tag,t) is the same as 			   *
 *								   *
 *  SET_EVAL(x,e,t)						   *
 *								   *
 * but tag is given as the tag of e.				   *
 *******************************************************************/

#define SET_EVAL_WITH_TAG(x,e,tag,t)\
  x = MEMBER(tag,all_lazy_tags)? eval1(e,t) : e

/*******************************************************************
 * IN_PLACE_EVAL(x,t) is the same as 			   	   *
 *								   *
 *  SET_EVAL(x,x,t)						   *
 *******************************************************************/

#define IN_PLACE_EVAL(x,t)\
  if(MEMBER(TAG(x),all_lazy_tags)) x = eval1(x,t)

/*******************************************************************
 * IN_PLACE_EVAL_FAILTO(x,t,lab) is the same as 	   	   *
 *								   *
 *  SET_EVAL(x,x,t)						   *
 *  if(failure >= 0) goto lab;					   *
 *								   *
 * but it only does the test of failure if eval is called.	   *
 *******************************************************************/

#define IN_PLACE_EVAL_FAILTO(x,t,lab)\
  if(MEMBER(TAG(x),all_lazy_tags)) {x = eval1(x,t); if(failure >= 0) goto lab;}

/*******************************************************************
 * IN_PLACE_EVAL_WITH_TAG(x,tag,t) is the same as 		   *
 *								   *
 *  SET_EVAL(x,x,t)						   *
 *								   *
 * where the tag of x is t.					   *
 *******************************************************************/

#define IN_PLACE_EVAL_WITH_TAG(x,tag,t)\
  if(MEMBER(tag,all_lazy_tags)) x = eval1(x,t)

/*******************************************************************
 * SET_WEAK_EVAL(x,e,t) does 					   *
 *								   *
 *     x = weak_eval(e,1,t)					   *
 *								   *
 * thus setting x to the result of evaluating e, with time-count   *
 * variable t, where infiles are not evaluated.  This is a macro,  *
 * since a test of whether e needs evaluation is done before any   *
 * call, for efficiency.					   *
 *******************************************************************/

#define SET_WEAK_EVAL(x,e,t)\
  x = MEMBER(TAG(e),all_weak_lazy_tags)? weak_eval1(e,1,t) : e

/*******************************************************************
 * IN_PLACE_WEAK_EVAL(x,t) is the same as 			   *
 *								   *
 *  SET_WEAK_EVAL(x,x,t)					   *
 *******************************************************************/

#define IN_PLACE_WEAK_EVAL(x,t)\
  if(MEMBER(TAG(x),all_weak_lazy_tags)) x = weak_eval1(x,1,t)

/*******************************************************************
 * IN_PLACE_WEAK_EVAL_FAILTO(x,t,lab) is the same as 		   *
 *								   *
 *  SET_WEAK_EVAL(x,x,t)					   *
 *  if(failure >= 0) goto lab;					   *
 * 								   *
 * but the test of failure is only done when evaluation is done.   *
 *******************************************************************/

#define IN_PLACE_WEAK_EVAL_FAILTO(x,t,lab)\
  if(MEMBER(TAG(x),all_weak_lazy_tags))\
     {x = weak_eval1(x,1,t); if(failure >= 0) goto lab;}

/*******************************************************************
 * SET_EVAL_TO_ANY_UNKNOWN(x,e,t) does 				   *
 *								   *
 *     x = weak_eval(e,2,t)			   		   *
 *								   *
 * thus setting x to the result of evaluating e, with time-count   *
 * variable t, until either e is evaluated at its top level or e   *
 * is an unknown.  This is a macro, since a test of whether e	   *
 * needs evaluation is done before any call, for efficiency.	   *
 *******************************************************************/

#define SET_EVAL_TO_ANY_UNKNOWN(x,e,t)\
  x = (MEMBER(TAG(e),all_lazy_tags))? weak_eval1(e,2,t) : e

/*******************************************************************
 * IN_PLACE_EVAL_TO_ANY_UNKNOWN(x,t) is the same as 		   *
 *								   *
 *  SET_EVAL_TO_ANY_UNKNOWN(x,x,t)				   *
 *******************************************************************/

#define IN_PLACE_EVAL_TO_ANY_UNKNOWN(x,t)\
  if(MEMBER(TAG(x),all_lazy_tags)) x = weak_eval1(x,2,t)

/*******************************************************************
 * IN_PLACE_EVAL_TO_ANY_UNKNOWN_FAILTO(x,t,lab) is the same as     *
 *								   *
 *  SET_EVAL_TO_ANY_UNKNOWN(x,x,t)				   *
 *  if(failure >= 0) goto lab;					   *
 *								   *
 * but the failure test is only done if evaluation is done.	   *
 *******************************************************************/

#define IN_PLACE_EVAL_TO_ANY_UNKNOWN_FAILTO(x,t,lab)\
  if(MEMBER(TAG(x),all_lazy_tags))\
   {x = weak_eval1(x,2,t); if(failure >= 0) goto lab;}

/*******************************************************************
 * SET_EVAL_TO_UNPROT_UNKNOWN(x,e,t) does 			   *
 *								   *
 *     x = weak_eval(e,6,t)			   		   *
 *								   *
 * thus setting x to the result of evaluating e, with time-count   *
 * variable t, until either e is evaluated at its top level or e   *
 * is an unprotected unknown.  This is a macro, since a test of    *
 * whether e needs evaluation is done before any call, for	   *
 * efficiency.							   *
 *******************************************************************/

#define SET_EVAL_TO_UNPROT_UNKNOWN(x,e,t)\
  x = (MEMBER(TAG(e),all_lazy_tags))? weak_eval1(e,6,t) : e

/*******************************************************************
 * IN_PLACE_EVAL_TO_UNPROT_UNKNOWN(x,t) is the same as 		   *
 *								   *
 *  SET_EVAL_TO_UNPROT_UNKNOWN(x,x,t)				   *
 *******************************************************************/

#define IN_PLACE_EVAL_TO_UNPROT_UNKNOWN(x,t)\
  if(MEMBER(TAG(x),all_lazy_tags)) x = weak_eval1(x,6,t)

/*******************************************************************
 * IN_PLACE_EVAL_TO_UNPROT_UNKNOWN_FAILTO(x,t,lab) is the same as  *
 *								   *
 *  SET_EVAL_TO_UNPROT_UNKNOWN(x,x,t)				   *
 *  if(failure >= 0) goto lab;					   *
 *								   *
 * but the failure test is only done if evaluation id done.	   *
 *******************************************************************/

#define IN_PLACE_EVAL_TO_UNPROT_UNKNOWN_FAILTO(x,t,lab)\
  if(MEMBER(TAG(x),all_lazy_tags))\
    {x = weak_eval1(x,6,t); if(failure >= 0) goto lab;}

/*******************************************************************
 * SET_FULL_EVAL(x,e,t) does 				   	   *
 *								   *
 *     x = full_eval(e,t)			   		   *
 *								   *
 * thus setting x to the result of fully evaluating e (including   *
 * all parts, not just the top level part), with time-count	   *
 * variable t.  This is a macro, since a test of whether e	   *
 * needs evaluation is done before any call for efficiency.	   *
 *******************************************************************/

#define SET_FULL_EVAL(x,e,t)\
  x = MEMBER(TAG(e),full_eval_tags)? full_eval1(e,t) : e

/*******************************************************************
 * SET_FULL_EVAL_FAILTO(x,e,t,lab) does 			   *
 *								   *
 *   SET_FULL_EVAL(x,e,t);			   		   *
 *   if(failure >= 0) goto lab;					   *
 *								   *
 * but the test of failure is only done if full_eval is called.	   *
 *******************************************************************/

#define SET_FULL_EVAL_FAILTO(x,e,t,lab)\
  if(MEMBER(TAG(e),full_eval_tags)) {x = full_eval1(e,t); if(failure >= 0) goto lab;} else x = e

/*******************************************************************
 * IN_PLACE_FULL_EVAL(x,t) is the same as 			   *
 *								   *
 *  SET_FULL_EVAL(x,x,t)					   *
 *******************************************************************/

#define IN_PLACE_FULL_EVAL(x,t)\
  if(MEMBER(TAG(x),full_eval_tags)) x = full_eval1(x,t)

/*******************************************************************
 * IN_PLACE_FULL_EVAL_FAILTO(x,t.lab) is the same as 		   *
 *								   *
 *  SET_FULL_EVAL(x,x,t)					   *
 *  if(failure >= 0) goto lab;					   *
 *								   *
 * but it only does the test of failure if eval is called.	   *
 *******************************************************************/

#define IN_PLACE_FULL_EVAL_FAILTO(x,t,lab)\
  if(MEMBER(TAG(x),full_eval_tags)) {x = full_eval1(x,t); if(failure >= 0) goto lab;}

/*******************************************************************
 * IN_PLACE_FULL_EVAL2(xt,defer) is similar to 		   	   *
 *								   *
 *  x = full_eval(x,t)						   *
 *								   *
 * but it might not do the entire job if it runs out of time.  It  *
 * sets *defer to zero (the entity 0) if the entire job was	   *
 * finished, and to an entity that needs further evaluation if     *
 * there was a time-out.					   *
 *								   *
 * Note: to use IN_PLACE_FULL_EVAL2, be sure to set		   *
 * *defer = zero first, since that is not done in the case where   *
 * the tag of v indicates that v has no structure.		   *
 *******************************************************************/

#define IN_PLACE_FULL_EVAL2(x,t,defer)\
  if(MEMBER(TAG(x),full_eval_tags)) x = full_eval2(x,t,defer)

ENTITY  indirect_replacement(ENTITY *p);
ENTITY  eval		    (ENTITY e, LONG *time_bound);
ENTITY  eval1		    (ENTITY e, LONG *time_bound);
ENTITY  full_eval	    (ENTITY e, LONG *time_bound);
ENTITY  full_eval1	    (ENTITY e, LONG *time_bound);
ENTITY  full_eval2	    (ENTITY ee, LONG *time_bound, ENTITY *defer);
ENTITY  weak_eval	    (ENTITY e, LONG mode, LONG *time_bound);
ENTITY  weak_eval1	    (ENTITY e, LONG mode, LONG *time_bound);
void    start_new_activation(void);
ENTITY  wrap		    (ENTITY e, TYPE *t);
ENTITY  domain_wrap	    (ENTITY e, TYPE *t);
ENTITY  qwrap		    (int discr, ENTITY e);
void    qunwrap_i           (int *discr, ENTITY *a, LONG *time_bound);
ENTITY  position_stdf	    (ENTITY a);
int     qwrap_tag	    (ENTITY a);
ENTITY  qwrap_val	    (ENTITY a);
Boolean test_lazy	    (ENTITY x, int k);
ENTITY	get_stack_depth	    (ENTITY herm);
ENTITY  continuation_name   (ENTITY herm);
ENTITY  acquire_box_stdf    (ENTITY e);
void    restore_state	    (void);

/********************** From apply.c **********************************/

void 	apply_i		(int k, LONG *time_bound);
void    rev_apply_i     (int k, LONG *time_bound);
void 	tail_apply_i	(int k, LONG *time_bound);
void    rev_tail_apply_i(int k, LONG *time_bound);
void    qeq_apply_i     (LONG *time_bound);
ENTITY  short_apply	(ENTITY f, ENTITY a, LONG *time_bound);
void 	do_apply	(CONTINUATION *fa, int k);
void 	do_tail_apply	(CONTINUATION *fa, int k);
void    do_function_return_apply(CODE_PTR *cont_pc, int type_instrs_index);
void    process_demon   (ENTITY demon, ENTITY oldcontent, ENTITY newcontent);
void    return_i	(int inst);
Boolean	pause_i		(LONG *l_time);
void    pause_this_thread(LONG *time_bound);

/********************** From coroutin.c **********************************/

void          resume_i	             (void);
CONTINUATION* get_coroutine_cont     (CODE_PTR retpc);
void          push_coroutine         (CODE_PTR pc, CONTINUATION *cont);
CODE_PTR      process_coroutine_args (CODE_PTR pc, CODE_PTR *start);
Boolean       coroutine_pause	     (void);


/********************** From fail.c ***********************************/

Boolean 	 should_trap	  (int ex);
void 		 trap_ex	  (ENTITY ex, int instr, CODE_PTR pc);
Boolean          do_failure       (UP_CONTROL *c, int inst, LONG *l_time);
union ctl_or_act get_timeout_parts(UP_CONTROL *c, int to_kind, 
				   union ctl_or_act to_coa, int *result_kind,
				   UP_CONTROL **cont);
Boolean          do_timeout       (LONG *l_time);


/*********************** From lazy.c *********************************/

extern int              should_not_recompute;

ENTITY make_lazy	(CODE_PTR c, int kind, char *name,
			 CODE_PTR type_instrs, Boolean recompute,
			 int ne);
ENTITY lazy_eval	(ENTITY *e, LONG *time_bound);
ENTITY global_eval	(ENTITY *gp, LONG *time_bound);
ENTITY lazy_list_eval	(ENTITY *e, LONG *time_bound);
ENTITY lazy_list_i	(CODE_PTR c, int kind, 
			 CODE_PTR type_instrs, Boolean recompute);
ENTITY run_fun		(ENTITY fun, ENTITY arg, STATE *st, 
			 TRAP_VEC *tv, LONG *l_time);
ENTITY get_and_eval_global	(int sym, TYPE *t, LONG* l_time);

/************************* For lazyprim.c ***************************/

/************************************************************************
 * If you add anything to the _TMO list, also add it to lazy_prim_name	*
 * array in src/show/prtent.c.  Document it in lazyprim.c.		*
 ************************************************************************/

#define EQUAL_TMO		1
#define INTERN_TMO		2
#define LENGTH_TMO		3
#define SUBSCRIPT_TMO		4
#define MERGE_TMO		7
#define SUBLIST_TMO		8
#define FULL_EVAL_TMO		9

/*-----------------------------------------------------*
 * Do not reorder LAZY_HEAD_TMO through LAZY_RIGHT_TMO *
 *-----------------------------------------------------*/

#define LAZY_HEAD_TMO		11
#define LAZY_TAIL_TMO		12
#define LAZY_LEFT_TMO		13
#define LAZY_RIGHT_TMO		14

#define REVERSE_TMO		15
#define UPTO_TMO		16
#define DOWNTO_TMO		17
#define INFLOOP_TMO             18
#define CMDMAN_TMO		19
#define DOCMD_TMO		20
#define SCAN_FOR_TMO		21
#define PACK_TMO		22

/*-------------------------------------------------------------------*
 * UNKNOWN_TMO and PROTECTED_UNKNOWN_TMO must be the last TMO values *
 *-------------------------------------------------------------------*/

#define UNKNOWN_TMO             24
#define PROTECTED_UNKNOWN_TMO	25

#define is_any_unknown(e)    ((TAG(e) == INDIRECT_TAG) && unknownq(e, 3))
#define is_unprot_unknown(e) ((TAG(e) == INDIRECT_TAG) && unknownq(e, 1))
#define is_prot_unknown(e)   ((TAG(e) == INDIRECT_TAG) && unknownq(e, 2))

ENTITY make_lazy_prim	(int k, ENTITY a, ENTITY b);
ENTITY redo_lazy_prim	(ENTITY *e, LONG *time_bound, Boolean *nostore);
ENTITY ast_merge	(ENTITY a, ENTITY b, LONG *time_bound);

int    eval_lazy_prim_to_unknown	(ENTITY *e, ENTITY **loc,
			      		 Boolean any_unknown, 
					 LONG *time_bound);
ENTITY scan_through_unknowns		(ENTITY e, Boolean *saw_private);
ENTITY gen_scan_through_unknowns	(ENTITY e);
Boolean unknownq			(ENTITY a, int mode);
ENTITY private_unknown_stdf		(ENTITY herm);
ENTITY protected_private_unknown_stdf	(ENTITY herm);
ENTITY public_unknown_stdf		(ENTITY herm);
ENTITY protected_public_unknown_stdf	(ENTITY herm);
ENTITY unknownq_stdf			(ENTITY x);
ENTITY protected_unknownq_stdf		(ENTITY x);
ENTITY unprotected_unknownq_stdf	(ENTITY x);
void   bind_unknown                     (ENTITY u, ENTITY v, int mode,
                                         LONG *l_time);
ENTITY same_unknown_stdf		(ENTITY e);
ENTITY split_unknown			(ENTITY u);


/********************* From typeinst.c *************************/

extern  TYPE* HUGEPTR type_stack;
extern  TYPE* HUGEPTR type_stack_top;
extern  TYPE* HUGEPTR type_open;

/****************************************************************
 * pop_type_stk() pops the type stack and returns the type that	*
 * was on top.  If does not drop the reference count of the	*
 * type that was removed.  That is the responsibility		*
 * of the caller.						*
 ****************************************************************/

#define pop_type_stk() (*(type_stack_top--))

/****************************************************************
 * top_type_stk() returns the top of the type stack without	*
 * popping.							*
 ****************************************************************/

#define top_type_stk() (*type_stack_top)

/****************************************************************
 * type_stk_is_empty() returns true just when the type stack    *
 * is empty.							*
 ****************************************************************/

#define type_stk_is_empty() (type_stack_top < type_stack)

void remember_type_storage	(TYPE_HOLD *info);
void restore_type_storage	(TYPE_HOLD *info);
void print_type_store		(void);
void clear_type_stk		(void);
void push_a_type		(TYPE *t);
void freeze_type_stk_top	(void);
#ifdef DEBUG
 void print_type_stk		(void);
#endif

void  type_instruction_i	(int i, CODE_PTR *cc, FILE *f, 
				 ENVIRONMENT *env, Boolean use_labels);
void  eval_type_instrs		(CODE_PTR p, ENVIRONMENT *env);
TYPE* eval_type_instrs_with_binding_list(CODE_PTR p, ENVIRONMENT *env, 
					 LIST *bl);


/********************* From instinfo.c ***************************/


void read_instr_names(void);

extern char 
     ** instr_name, 
     ** un_instr_name, 
     ** bin_instr_name,
     *list1_instr_name[], 
     *list2_instr_name[],
     *ty_instr_name[],
     *unk_instr_name[],
     **prefix_instr_name[];
