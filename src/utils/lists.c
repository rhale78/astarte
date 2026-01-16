/***************************************************************
 * File:    utils/lists.c
 * Author:  Karl Abrahamson
 * Purpose: implement list and stack processing routines
 ***************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************
 * This file provides the most basic operations on lists.  For other	*
 * operations, see listutil.c.						*
 ************************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../alloc/allocate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *				LKINDF				*
 ****************************************************************
 * lkindf is only used in GCTEST mode.  It returns the tag of L,*
 * but also checks that the reference count has not gone 	*
 * negative.							*
 *								*
 * In GCTEST mode, macro LKIND(L) means lkindf(L).		*
 ****************************************************************/

#ifdef GCTEST
int lkindf(LIST *l)
{
  if(l->ref_cnt < -1) die(133, (char *)(LONG) (l->ref_cnt));
  return l->kind;
}
#endif

/****************************************************************
 *			GENERAL_CONS 				*
 ****************************************************************
 * Return a new cons cell with head H, tail T and tag KIND.	*
 ****************************************************************/

LIST* general_cons(HEAD_TYPE h, LIST *t, int kind)
{
  LIST *c;

# ifdef GCTEST
    if(t != NIL && t->ref_cnt < 0) {
      die(133,(char *)(LONG)(t->ref_cnt));
    }
# endif

  c 		= allocate_list();
  c->kind	= kind;
  c->head       = h;
  bump_head(kind, h);
  bump_list(c->tail = t);
  return c;
}


/********************************************************************
 * The following are specific cons functions.  Each is implemented  *
 * in terms of general_cons.					    *
 ********************************************************************/

/****************************************************************
 *			HASH2_CONS 				*
 ****************************************************************/

HASH2_LIST* hash2_cons(HASH2_TABLE *h, HASH2_LIST *t)
{
  HEAD_TYPE u;

  u.hash2 = h;
  return general_cons(u, t, HASH2_L);
}


/****************************************************************
 *			STR_CONS 				*
 ****************************************************************/

STR_LIST* str_cons(char *h, LIST *t)
{
  HEAD_TYPE u;

  u.str = h;
  return general_cons(u, t, STR_L);
}

/****************************************************************
 *			INT_CONS 				*
 ****************************************************************/

INT_LIST* int_cons(LONG h, INT_LIST *t)
{
  HEAD_TYPE u;

  u.i = h;
  return general_cons(u, t, INT_L);
}

/****************************************************************
 *			SHORTS_CONS 				*
 ****************************************************************/

SHORTS_LIST* shorts_cons(struct two_shorts ts, SHORTS_LIST *t)
{
  HEAD_TYPE u;

  u.two_shorts = ts;
  return general_cons(u, t, SHORTS_L);
}

/****************************************************************
 *			TYPE_CONS 				*
 ****************************************************************/

TYPE_LIST* type_cons(TYPE *h, TYPE_LIST *t)
{
  HEAD_TYPE u;

  u.type = h;
  return general_cons(u, t, TYPE_L);
}

/****************************************************************
 *			STYPE_CONS 				*
 ****************************************************************/

LIST* stype_cons(TYPE **h, LIST *t)
{
  HEAD_TYPE u;

  u.stype = h;
  return general_cons(u, t, STYPE_L);
}


/****************************************************************
 *			CTC_CONS 				*
 ****************************************************************/

CTC_LIST* ctc_cons(CLASS_TABLE_CELL *h, CTC_LIST *t)
{
  HEAD_TYPE u;

  u.ctc = h;
  return general_cons(u, t, CTC_L);
}


/****************************************************************
 *			FILE_CONS 				*
 ****************************************************************/

FILE_LIST* file_cons(FILE *h, FILE_LIST *t)
{
  HEAD_TYPE u;

  u.file = h;
  return general_cons(u, t, FILE_L);
}


/****************************************************************
 *			LIST_CONS 				*
 ****************************************************************/

LIST_LIST* list_cons(LIST *h, LIST_LIST *t)
{
  HEAD_TYPE u;

  u.list = h;
  return general_cons(u, t, LIST_L);
}



#ifdef TRANSLATOR

/****************************************************************
 *			MODE_CONS 				*
 ****************************************************************/

LIST* mode_cons(MODE_TYPE *h, LIST *t)
{
  HEAD_TYPE u;

  u.mode = h;
  return general_cons(u, t, MODE_L);
}


/****************************************************************
 *			EXPR_CONS 				*
 ****************************************************************/

EXPR_LIST* expr_cons(EXPR *h, EXPR_LIST *t)
{
  HEAD_TYPE u;

  u.expr = h;
  return general_cons(u, t, EXPR_L);
}

/****************************************************************
 *			CHOOSE_INFO_CONS 			*
 ****************************************************************/

LIST* choose_info_cons(CHOOSE_INFO *h, EXPR_LIST *t)
{
  HEAD_TYPE u;

  u.choose_info = h;
  return general_cons(u, t, CHOOSE_INFO_L);
}

/****************************************************************
 *			ROLE_CONS 				*
 ****************************************************************/

EXPR_LIST* role_cons(ROLE *h, EXPR_LIST *t)
{
  HEAD_TYPE u;

  u.role = h;
  return general_cons(u, t, ROLE_L);
}

/****************************************************************
 *			EXP_CONS 				*
 ****************************************************************/

EXPECT_LIST* exp_cons(EXPECTATION *h, EXPECT_LIST *t)
{
  HEAD_TYPE u;

  u.exp = h;
  return general_cons(u, t, EXPECT_L);
}

/****************************************************************
 *			BUF_CONS 				*
 ****************************************************************/

LIST* buf_cons(YY_BUFFER_STATE h, LIST *t)
{
  HEAD_TYPE u;

  u.buf = h;
  return general_cons(u, t, BUF_L);
}

#endif

#ifdef MACHINE

/****************************************************************
 *			ENTS_CONS 				*
 ****************************************************************/

LIST* ents_cons(ENTITY *h, LIST *t)
{
  HEAD_TYPE u;

  u.ents = h;
  return general_cons(u, t, ENTS_L);
}


/****************************************************************
 *		        STATE_CONS 				*
 ****************************************************************/

LIST* state_cons(struct state *h, LIST *t)
{
  HEAD_TYPE u;

  u.state = h;
  return general_cons(u, t, STATE_L);
}


/****************************************************************
 *		        TRAP_VEC_CONS 				*
 ****************************************************************/

LIST* trap_vec_cons(struct trap_vec *h, LIST *t)
{
  HEAD_TYPE u;

  u.trap_vec = h;
  return general_cons(u, t, TRAP_VEC_L);
}


/****************************************************************
 *		        ACT_CONS 				*
 ****************************************************************/

LIST* act_cons(ACTIVATION *h, LIST *t)
{
  HEAD_TYPE u;

  u.act = h;
  return general_cons(u, t, ACT_L);
}


/****************************************************************
 *		        ENV_DESCR_CONS 				*
 ****************************************************************/

LIST* env_descr_cons(struct env_descr *h, LIST *t)
{
  HEAD_TYPE u;

  u.env_descr = h;
  return general_cons(u, t, ENV_DESCR_L);
}

#endif

/***************** End cons functions ***************************/

/****************************************************************
 *			  PUSH_TWO_SHORTS	 		*
 ****************************************************************
 * Push two short values onto S, as a two_shorts structure.	*
 * M is put into the LINE field, and N is put into the COL 	*
 * field of the two_shorts structure.				*
 ****************************************************************/

void push_two_shorts(SHORTS_LIST **s, int m, int n)
{
  struct two_shorts ts;

  ts.line = m;
  ts.col  = n;
  set_list(s, shorts_cons(ts, *s));
}

/*******************************************************************
 *				POP				   *
 *******************************************************************
 * Set *S to the tail of *S.  This is used to pop a stack.	   *
 *******************************************************************/

void pop(LIST **s)
{
  LIST *next;

  if(*s != NIL) {
    bump_list(next = (*s)->tail);
    drop_list(*s);
    *s = next;
  }  
}


/*******************************************************************
 *			     NOREF_POP				   *
 *******************************************************************
 * Same a pop, but don't drop head.				   *
 *******************************************************************/

void noref_pop(LIST **s)
{
  LIST *next;

  if(*s != NIL) {
    next = (*s)->tail;
    free_list_node(*s);
    *s = next;
  }  
}


/****************************************************************
 *			BUMP_HEAD 				*
 ****************************************************************
 * bump_head(K,X) increments the reference count of X if type   *
 * is appropriate, where X has kind K.				*
 ****************************************************************/

void bump_head(int k, HEAD_TYPE x)
{
  switch(k){
#   ifdef TRANSLATOR
    case EXPR_L:
      bump_expr(x.expr);
      break;

    case ROLE_L:
      bump_role(x.role);
      break;

    case MODE_L:
      bump_mode(x.mode);
      break;
#   endif

    case TYPE_L:
    case TYPE1_L:
      bump_type(x.type);
      break;

    case LIST_L:
    case PAIR_L:
    case APPL_L:
      bump_list(x.list);
      break;

#   ifdef MACHINE
    case STATE_L:
      bump_state(x.state);
      break;

    case TRAP_VEC_L:
      bump_trap_vec(x.trap_vec);
      break;

    case ACT_L:
      bump_activation(x.act);
      break;

    case CONTROL_L:
      bump_control(x.control);
      break;
#   endif

    default: {}
  }
}


/****************************************************************
 *			DROP_HEAD 				*
 ****************************************************************
 * drop_head(K,X) decrements the reference count of X if type   *
 * is appropriate, possibly freeing X, where X has kind K.	*
 * Don't drop activations.					*
 ****************************************************************/

void drop_head(int k, HEAD_TYPE x)
{
  switch(k){
#   ifdef TRANSLATOR
    case EXPR_L:
      drop_expr(x.expr);
      break;

    case ROLE_L:
      drop_role(x.role);
      break;

    case CUC_L:
      free_cuc(x.cuc);
      break;

    case MODE_L:
      drop_mode(x.mode);
      break;
#   endif

    case TYPE_L:
    case TYPE1_L:
      drop_type(x.type);
      break;

    case LIST_L:
    case PAIR_L:
    case APPL_L:
      drop_list(x.list);
      break;

    case HASH2_L:
      free_hash2(x.hash2);
      break;

#   ifdef MACHINE
    case STATE_L:
      drop_state(x.state);
      break;

    case TRAP_VEC_L:
      drop_trap_vec(x.trap_vec);
      break;

    case ACT_L:
      drop_activation(x.act);
      break;

    case CONTROL_L:
      drop_control(x.control);
      break;
#   endif

    default: {}
  }
}


/****************************************************************
 *			SET_HEAD 				*
 ****************************************************************
 * set_head(L,K,X) sets the kind of list cell L to K, and the	*
 * head to X.							*
 ****************************************************************/

void set_head(LIST *l, int k, HEAD_TYPE x)
{
  bump_head(k,x);
  drop_head(LKIND(l), l->head);
  l->kind = k;
  l->head = x;
}

