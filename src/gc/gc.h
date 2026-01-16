/**********************************************************************
 * File:    gc/gc.h
 * Purpose: Garbage collector
 * Author:  Karl Abrahamson
 **********************************************************************/

/**********************************************************************
 * The following macros are used with registering of variables.  They *
 * are used to undo the effects of registering a variable, just       *
 * before a function exits.					      *
 **********************************************************************/

typedef LIST* STATE_REG;
typedef LIST* ACTIVATION_REG;
typedef LIST* CONTROL_REG;

#define unreg(mark) 	   num_rts_ents = mark
#define unregptr(ptrmark)  num_rts_pents = ptrmark
#define unreg_bigint(mark) SET_LIST(rts_bigints, mark)
#define unreg_state(mark)  SET_LIST(rts_states, mark)
#define unreg_activation(mark) SET_LIST(rts_activations, mark)
#define unreg_control(mark) SET_LIST(rts_controls, mark)

/**********************************************************************
 *			From gc.c
 **********************************************************************/

extern int suppress_compactify;
extern LONG get_before_gc, get_before_gc_reset;
extern Boolean alloc_phase;  
extern LIST *rts_bigints, *rts_states, *rts_controls, *rts_activations;
extern int num_rts_ents, num_rts_pents;
extern ENTITY **rts_ents;
extern CONTINUATION *fun_conts;
extern Boolean compactifying;
extern Boolean force_file_cut;

void        gc				(Boolean should_compactify);
REG_TYPE    reg1			(ENTITY *x);
REG_TYPE    reg2			(ENTITY *x, ENTITY *y);
REG_TYPE    reg3			(ENTITY *x, ENTITY *y, ENTITY *z);
REG_TYPE    reg1_param			(ENTITY *x);
REG_TYPE    reg2_param			(ENTITY *x, ENTITY *y);
REGPTR_TYPE reg1_ptr			(ENTITY **x);
REGPTR_TYPE reg1_ptrparam		(ENTITY **x);
LIST *      reg_state         		(STATE **s);
LIST *      reg_state_param    		(STATE **s);
LIST * 	    reg_activation		(ACTIVATION *a);
LIST *      reg_control			(CONTROL *c);
void        note_control		(CONTROL *c);
void        mark_entity_gc		(ENTITY *e);
void        mark_entity_parts_gc	(ENTITY *e);
void        relocate_gc			(ENTITY *p);
void        relocate_ptr_gc		(ENTITY **q);
void *      gc_alloc			(int n);
LIST *      gc_cons			(char *h, LIST *t);
int         gctag			(ENTITY e);
ENTITY      suppress_compactify_stdf 	(ENTITY x);
void        init_gc			(void);

/************************************************************************
 *			From gcstate.c					*
 ************************************************************************/

struct seen_state_struct {
  STATE *state;
  LIST *variables;
  struct seen_state_struct *next;
};

extern BOXSET *marked_boxes, *free_boxsets;
extern struct seen_state_struct *seen_states;

void 	mark_box_gc		(LONG b);
void 	mark_state_gc		(STATE **s);
void    unmark_state_gc		(STATE *s);
void    unmark_seen_states_gc   (void);
void 	mark_all_state_gc	(STATE *s);
void    mark_state_with_gc      (STATE *s, LONG min_s, LONG max_s, 
			         BOXSET *bs, LONG min_bs, LONG max_bs);
BOXSET* mark_box_range_gc       (LONG min_b, LONG max_b, BOXSET *s);
void    mark_range_gc           (LONG min_b, LONG max_b, STATE *s, 
				 LONG min_s, LONG max_s);
void    print_boxset            (BOXSET *s);
Boolean box_member		(LONG b, BOXSET *s);
BOXSET* new_boxset_node		(LONG minb, LONG maxb, 
				 BOXSET *left, BOXSET *right);
void 	free_boxset		(BOXSET *s);
BOXSET* collapse_boxset		(BOXSET *s);
BOXSET* rebalance_boxset	(BOXSET *s);
BOXSET* box_insert		(LONG b, BOXSET *s);
BOXSET* box_range_insert	(LONG a, LONG b, BOXSET *s);
LONG 	compactify_boxset	(BOXSET *s);
LONG 	box_relocation		(LONG b, BOXSET *s);
void 	relocate_box_gc		(ENTITY *p);
void    rebuild_states_gc       (void);

