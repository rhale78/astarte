/*******************************************************************
 * File:    alloc/mrcalloc.c
 * Purpose: Storage manager for ref-counted types that are used
 *          by the interpreter.
 * Author:  Karl Abrahamson
 *******************************************************************/

#ifdef COPYRIGHT
/************************************************************************
 * Copyright (c) 2000 Karl Abrahamson					*
 * All rights reserved.							*
 * See file COPYRIGHT.txt for details.					*
 ************************************************************************/
#endif

/************************************************************************
 * This file contains allocation and deallocation functions for		*
 * types used by the interpreter that are reference counted.  In 	*
 * general, 								*
 *									*
 *   bump_T(x) adds one to the reference count of x, of type T.  	*
 *									*
 *   drop_T(x) subtracts one from the reference count of x, and frees x *
 *             if the resulting reference count is 0.  			*
 *									*
 *   set_T(x,y) sets the cell pointed to by x to y, handling reference	*
 *              counts.  Use SET_T(x,y) if x is a variable to avoid the *
 *              need to write &x.					*
 *									*
 * Note: environments are special, since reference counts are applied   *
 * to parts or an environment node, not just the whole.			*
 *									*
 * Some of the allocation functions initialize the reference count	*
 * to 0, and some to 1. 						*
 *									*
 * initialize to 0:							*
 *	allocate_state							*
 *	allocate_trap_vec						*
 * 	allocate_control						*
 *									*
 * initialize to 1:							*
 *	allocate_stack							*
 *	allocate_continuation						*
 *      allocate_activation						*
 *	allocate_env							*
 ************************************************************************/

#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../machdata/except.h"
#include "../machdata/entity.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

/****************************************************************
 *			BUMPS					*
 ****************************************************************/

#ifndef GCTEST
  void bump_stack(STACK *s)               {if(s != NULL) s->ref_cnt++;}
  void bump_state(STATE *s)               {if(s != NULL) s->ref_cnt++;}
  void bump_control(CONTROL *s)           {if(s != NULL) s->ref_cnt++;}
  void bump_continuation(CONTINUATION *a) {if(a != NULL) a->ref_cnt++;}
  void bump_activation(ACTIVATION *a)     {if(a != NULL) a->ref_cnt++;}
  void bump_trap_vec(TRAP_VEC *t)         {if(t != NULL) t->ref_cnt++;}
#else
  void bump_stack(STACK *s)               
  { if(s != NULL) {
      if(s->ref_cnt < 0) badrc("stack", toint(s->ref_cnt), (char *) s);
      s->ref_cnt++;
    } 
  }
  void bump_state(STATE *s)               
  { if(s != NULL) {
      if(s->ref_cnt < 0) badrc("state", toint(s->ref_cnt), (char *) s);
      s->ref_cnt++;
    }
  }
  void bump_control(CONTROL *s)           
  { if(s != NULL) {
      if(s->ref_cnt < 0) badrc("control", toint(s->ref_cnt), (char *)s);
      s->ref_cnt++;
    }
  }
  void bump_continuation(CONTINUATION *s) 
  { if(s != NULL) {
      if(s->ref_cnt < 0) badrc("continuation", toint(s->ref_cnt), (char *) s);
      s->ref_cnt++;
    }
  }
  void bump_activation(ACTIVATION *s) 
  { if(s != NULL) {
      if(s->ref_cnt < 0) badrc("activation", toint(s->ref_cnt), (char *) s);
      s->ref_cnt++;
    }
  }
  void bump_trap_vec(TRAP_VEC *s)     
  { if(s != NULL) {
      if(s->ref_cnt < 0) badrc("trap_vec", toint(s->ref_cnt), (char *) s);
      s->ref_cnt++;
    }
  }

#endif


/****************************************************************
 *			STACKS					*
 ****************************************************************/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

STACK* free_stacks = NULL;

/***************************************************************
 *                 ALLOCATE_STACK                              *
 ***************************************************************
 * Return a new stack node with ref count 1 and mark 0.        *
 ***************************************************************/

STACK* allocate_stack(void)
{
  register STACK* p = free_stacks;

  if(p == NULL) {
    p = (STACK *) alloc_small(sizeof(STACK));
  }
  else free_stacks = p->prev;
  p->ref_cnt = 1;
  p->mark    = 0;

# ifdef DEBUG
    allocated_stacks++;
# endif

  return p;
}


/***************************************************************
 *                 DROP_STACK                                  *
 ***************************************************************
 * Decrement the reference count of stack S, and possibly free *
 * it.  If S is freed, the reference count of S->prev is       *
 * dropped.  S can be NULL.                                    *
 ***************************************************************/

void drop_stack(STACK *s)
{
  register STACK *p, *q;

  p = s;
  while(p != NULL && --(p->ref_cnt) == 0) {

#   ifdef DEBUG
      if(rctrace) trace_i(13, p);
#   endif

    q = p->prev;

#   ifdef DEBUG
      allocated_stacks--;
#   endif

#   ifdef GCTEST
      p->ref_cnt  = -100;
#   else
      p->prev 	  = free_stacks;
      free_stacks = p; 
#   endif

    p = q;
  }

# ifdef GCTEST
    if(p != NULL && p->ref_cnt < 0) {
      badrc("stack", toint(p->ref_cnt), (char *) p);
    }
# endif
}


/****************************************************************
 *			STATES					*
 ****************************************************************/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE STATE* free_states = NULL;

/***************************************************************
 *                 ALLOCATE_STATE                              *
 ***************************************************************
 * Return a new state node with ref count and mark 0, and the  *
 * given kind.                                    	       *
 ***************************************************************/

STATE* allocate_state(int kind)
{
  register STATE* s = free_states;

  if(s == NULL) {
    s = (STATE *) alloc_small(sizeof(STATE));
  }
  else 		free_states = s->ST_LEFT;
  s->ref_cnt = 0;
  s->mark    = 0;
  s->kind    = kind;

# ifdef DEBUG
    allocated_states++;
# endif

  return s;
}

/******************************************************************
 *                 DROP_STATE                                     *
 ******************************************************************
 * Decrement the reference count of state S, and possibly free it.*
 * If S is freed, then drop the reference counts of the children  *
 * of S as well.  S can be NULL.				  *
 ******************************************************************/

void drop_state(STATE *s)
{
  register STATE *p, *q;

  p = s;
  while(p != NULL && --(p->ref_cnt) == 0) {

#   ifdef DEBUG
      if(rctrace) trace_i(14, p);
#   endif

    if(STKIND(p) == INTERNAL_STT) {
      drop_state(p->ST_LEFT);
      q = p->ST_RIGHT;
    }
    else q = NULL;

#   ifdef DEBUG
      allocated_states--;
#   endif

#   ifdef GCTEST
      p->ref_cnt  = -100;
#   else
      p->ST_LEFT  = free_states;
      free_states = p;
#   endif

    p = q;
  }

# ifdef GCTEST
    if(p != NULL && p->ref_cnt < 0) {
      badrc("state", toint(p->ref_cnt), (char *) p);
    }
# endif
}


/***************************************************************
 *                 SET_STATE                                   *
 ***************************************************************
 * Set *X to T, keeping track of reference counts.             *
 ***************************************************************/

void set_state(STATE **x, STATE *t)
{
  if(t != NULL) t->ref_cnt++;
  drop_state(*x);
  *x = t;
}


/****************************************************************
 *			CONTROLS				*
 ****************************************************************/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE CONTROL* free_controls = NULL;

/***************************************************************
 *                 ALLOCATE_CONTROL                            *
 ***************************************************************
 * Return a new control node, with ref count and mark 0.       *
 ***************************************************************/

CONTROL* allocate_control(void)
{
  register CONTROL* s = free_controls;

  if(s == NULL) {
    s = (CONTROL *) alloc_small(sizeof(CONTROL));
  }
  else {
    free_controls  = s->left.ctl;
  }
  s->mark    = 0;
  s->ref_cnt = 0;

# ifdef DEBUG
    allocated_controls++;
# endif

  return s;
}

/*****************************************************************
 *                 DROP_CONTROL                                  *
 *****************************************************************
 * Decrement the reference count of control node S, and possibly *
 * free it.  If S is freed, drop the reference counts of nodes   *
 * to which S refers.  S can be NULL.				 *
 *****************************************************************/

void drop_control(CONTROL *s)
{
  register CONTROL *p;
  register union ctl_or_act q;
  register UBYTE info;
  register int kind;

  p = s;
  while(p != NULL && --(p->ref_cnt) <= 0) {

#   ifdef DEBUG
      if(rctrace) trace_i(15, p);
#   endif

    info = p->info;

    /*------------------------------------*
     * Drop right child, if there is one. *
     *------------------------------------*/

    kind = info & KIND_CTLMASK;
    if(kind < MARK_F) {
      if((info & RCHILD_CTLMASK) != CTL_F) drop_activation(p->right.act);
      else drop_control(p->right.ctl);
    }

    /*---------*
     * Free p. *
     *---------*/

    q = p->left;
#   ifdef DEBUG
      allocated_controls--;
#   endif
#   ifdef GCTEST
      p->ref_cnt    = -100;
#   else
      p->left.ctl   = free_controls;
      free_controls = p; 
#   endif

    /*------------------------------------------------------*
     * Drop left child.  Do this by tail recursion if it is *
     * a control. 					    *
     *------------------------------------------------------*/

    if((info & LCHILD_CTLMASK) != CTL_F) {
      drop_activation(q.act);
      return;
    }
    else p = q.ctl;   /* tail recur */
  }

# ifdef GCTEST
    if(p != NULL && p->ref_cnt < 0) {
      badrc("control", toint(p->ref_cnt), (char *) p);
    }
# endif
}


/***************************************************************
 *                 SET_CONTROL                                 *
 ***************************************************************
 * Set *S to V, keeping track of reference counts.             *
 ***************************************************************/

void set_control(CONTROL **s, CONTROL *v)
{
  if(v != NULL) v->ref_cnt++;
  drop_control(*s);
  *s = v;
}


/****************************************************************
 *			CONTINUATIONS				*
 ****************************************************************/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE CONTINUATION* free_continuations = NULL;

/***************************************************************
 *                 ALLOCATE_CONTINUATION                       *
 ***************************************************************
 * Return a new continuation node with ref count 1 and mark 0. *
 ***************************************************************/

CONTINUATION* allocate_continuation(void)
{ 
  register CONTINUATION* a = free_continuations;

  if(a == NULL) {
    a = (CONTINUATION *) alloc_small(sizeof(CONTINUATION));
  }
  else 		free_continuations = a->continuation;
  a->ref_cnt = 1;
  a->mark = 0;

# ifdef DEBUG
    allocated_continuations++;
    if(trace_mem) {
	fprintf(TRACE_FILE, 
		"ctinA: %5ld %5ld %5ld %5ld %5ld %5ld %5ld %5ld %5ld %5ld\n", 
		allocated_lists, allocated_types, allocated_stacks, 
		allocated_states, allocated_controls, allocated_continuations,
		allocated_activations, allocated_trap_vecs, 
		allocated_environments, allocated_global_envs);
      }
# endif

  return a;
}


/****************************************************************
 *                 DROP_CONTINUATION                            *
 ****************************************************************
 * Decrement the reference count of continuate AA, and 		*
 * possibly free it.  Do recursive drops if AA is freeed.       *
 * AA can be NULL.						*
 ****************************************************************/

void drop_continuation(CONTINUATION *aa)
{
  register CONTINUATION* b;
  register CONTINUATION* a = aa;

  while(a != NULL && --(a->ref_cnt) == 0) {

#   ifdef DEBUG
      if(rctrace) trace_i(16, a);
#   endif

    drop_env(a->env, a->num_entries);
    drop_list(a->exception_list);
    b 			 = a;
    a 			 = a->continuation;
    free_continuation(b);
  }

# ifdef GCTEST
    if(a != NULL && a->ref_cnt < 0) {
      badrc("continuation", toint(a->ref_cnt), (char *)a);
    }
# endif
}


/***************************************************************
 *                 FREE_CONTINUATION                           *
 ***************************************************************
 * Free continuation node a, ignoring what it contains.        *
 ***************************************************************/

void free_continuation(CONTINUATION *a)
{
  if(a != NULL) {
#   ifdef DEBUG
      if(rctrace) trace_i(17, a);
      allocated_continuations--;
#   endif

#   ifdef GCTEST
      if(a->ref_cnt < 0) {
	badrc("continuation", toint(a->ref_cnt), (char *) a);
      }
      a->ref_cnt         = -100;
#   else
      a->continuation 	 = free_continuations;
      free_continuations = a;
#   endif
  }
}


/****************************************************************
 *			ACTIVATIONS				*
 ****************************************************************/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE ACTIVATION* free_activations = NULL;

/***************************************************************
 *                 ALLOCATE_ACTIVATION                         *
 ***************************************************************
 * Return a new activation node with ref count 1 and mark 0.   *
 ***************************************************************/

ACTIVATION* allocate_activation(void)
{ 
  register ACTIVATION* a = free_activations;

  if(a == NULL) {
    a = (ACTIVATION *) alloc_small(sizeof(ACTIVATION));
  }
  else	free_activations = (ACTIVATION *) (a->continuation);
  a->actmark    = 0;
  a->ref_cnt = 1;

# ifdef DEBUG
    allocated_activations++;
# endif

  return a;
}

/***************************************************************
 *                 FREE_ACTIVATION                             *
 ***************************************************************
 * Free activation A, ignoring what it contains.               *
 ***************************************************************/

void free_activation(ACTIVATION *a)
{
  if(a != NULL) {

#   ifdef DEBUG
      if(rctrace) {
        trace_i(18, a, toint(a->kind));
      }
      allocated_activations--;
#   endif

#   ifdef GCTEST
      a->ref_cnt = -100;
      a->kind = -1;
#   else
      a->continuation  = (CONTINUATION *) free_activations;
      free_activations = a;
#   endif
  }
}


/******************************************************************
 *                 DROP_ACTIVATION_PARTS                          *
 ******************************************************************
 * Decrement the reference count of every reference counted thing *
 * that occurs in activation A.					  *
 ******************************************************************/

void drop_activation_parts(ACTIVATION *a)
{
  if(a == NULL) return;

  drop_list(a->type_binding_lists);
  drop_control(a->control);
  drop_stack(a->stack);
  drop_state(a->state_a);
  drop_list(a->state_hold);
  drop_env(a->env, a->num_entries);
  drop_trap_vec(a->trap_vec_a);
  drop_list(a->trap_vec_hold);
  drop_continuation(a->continuation);
  drop_list(a->exception_list);
  drop_list(a->embedding_tries);
  drop_list(a->embedding_marks);
  drop_list(a->coroutines);
}


/******************************************************************
 *                 DROP_ACTIVATION_PARTS_EXCEPT_CCS               *
 ******************************************************************
 * Decrement the reference count of every reference counted thing *
 * that occurs in activation A, with the exception of the         *
 * control, coroutines, state_a and state_hold fields.            *
 ******************************************************************/

void drop_activation_parts_except_ccs(ACTIVATION *a)
{
  if(a == NULL) return;

  drop_list(a->type_binding_lists);
  drop_stack(a->stack);
  drop_env(a->env, a->num_entries);
  drop_trap_vec(a->trap_vec_a);
  drop_list(a->trap_vec_hold);
  drop_continuation(a->continuation);
  drop_list(a->exception_list);
  drop_list(a->embedding_tries);
  drop_list(a->embedding_marks);
}


/***************************************************************
 *                 DROP_ACTIVATION                             *
 ***************************************************************
 * Decrement the reference count of activation a, and possibly *
 * free it.                                                    *
 ***************************************************************/

void drop_activation(ACTIVATION *a)
{
# ifdef GCTEST
    if(a != NULL && a->ref_cnt <= 0) {
      badrc("activation", toint(a->ref_cnt), (char *) a);
    }
# endif

  if(a != NULL && --(a->ref_cnt) == 0) {
    drop_activation_parts(a);
    free_activation(a);
  }
}


/******************************************************************
 *                 BUMP_ACTIVATION_PARTS_EXCEPT_STACK             *
 ******************************************************************
 * Increment the reference count of every reference counted thing *
 * that occurs in activation A, except the stack.                 *
 ******************************************************************/

void bump_activation_parts_except_stack(ACTIVATION *a)
{
  bump_list(a->type_binding_lists);
  bump_state(a->state_a);
  bump_list(a->state_hold);
  bump_env(a->env, a->num_entries);
  bump_continuation(a->continuation);
  bump_control(a->control);
  bump_trap_vec(a->trap_vec_a);
  bump_list(a->trap_vec_hold);
  bump_list(a->exception_list);
  bump_list(a->embedding_tries);
  bump_list(a->embedding_marks);
  bump_list(a->coroutines);
}  


/******************************************************************
 *                 BUMP_ACTIVATION_PARTS_EXCEPT_CCS               *
 ******************************************************************
 * Increment the reference count of every reference counted thing *
 * that occurs in activation A, with the exception of the         *
 * control, coroutines, state_a and state_hold fields.            *
 ******************************************************************/

void bump_activation_parts_except_ccs(ACTIVATION *a)
{
  if(a == NULL) return;

  bump_list(a->type_binding_lists);
  bump_stack(a->stack);
  bump_env(a->env, a->num_entries);
  bump_trap_vec(a->trap_vec_a);
  bump_list(a->trap_vec_hold);
  bump_continuation(a->continuation);
  bump_list(a->exception_list);
  bump_list(a->embedding_tries);
  bump_list(a->embedding_marks);
}


/******************************************************************
 *                 BUMP_ACTIVATION_PARTS                          *
 ******************************************************************
 * Increment the reference count of every reference counted thing *
 * that occurs in activation A.                                   *
 ******************************************************************/

void bump_activation_parts(ACTIVATION *a)
{
  if(a == NULL) return;

  bump_activation_parts_except_stack(a);
  bump_stack(a->stack);
}


/******************************************************************
 *                 BUMP_CTL_OR_ACT                                *
 ******************************************************************
 * Increment the reference count of P, which is either a control  *
 * or activation.  The kind is indicated by parameter kind, which *
 * is CTL_F when P is a control, and is anything else when        *
 * P is an activation.                                            *
 ******************************************************************/

void bump_ctl_or_act(union ctl_or_act p, int kind)
{
  if(kind != CTL_F) bump_activation(p.act);
  else bump_control(p.ctl);
}


/*****************************************************************
 *                 DROP_CTL_OR_ACT                               *
 *****************************************************************
 * Decrement the reference count of P, which is either a control *
 * or activation, and possibly free it.  The kind is indicated   *
 * by parameter kind, which is CTL_F when P is a control,        *
 * and is anything else when P is an activation.                 *
 *****************************************************************/

void drop_ctl_or_act(union ctl_or_act p, int kind)
{
  if(kind != CTL_F) drop_activation(p.act);
  else drop_control(p.ctl);
}


/******************************************************************
 *			ENVIRONMENTS				  *
 ******************************************************************
 * Environment nodes have multiple sizes.  free_environments[i]   *
 * points to a list of environment nodes of size env_size[i].     *
 * Each cell j in an environment has a reference count attached   *
 * to it, indicating the number of references to this environment *
 * that use cells 0,...,j.                                        *
 *								  *
 * free_environments and env_size are defined in envsize.c.	  *
 ******************************************************************/

#include "../machstrc/envsize.c"

/****************************************************************
 *                 ALLOCATE_ENV                                 *
 ****************************************************************
 * Return a new environment node with sz field set to N, 	*
 * ref count 1, mark 0 and most_entries 0.  			*
 * env_size[n] bindings can be stored in the result environment *
 * node.                      					*
 *								*
 * Each cell in the returned environment is initialized to have *
 * value NOTHING.						*
 ****************************************************************/

ENVIRONMENT* allocate_env(int n)
{
  register ENVIRONMENT *p;
  int size;

  if(n < 0 || n >= ENV_SIZE_SIZE) die(3, n);

  /*--------------------*
   * Allocate the node. *
   *--------------------*/

  size = env_size[n];
  p = free_environments[n];
  if(p == NULL) {
     p = (ENVIRONMENT *) alloc_small((sizeof(ENVIRONMENT) 
			       + (size - 1)*sizeof(struct envcell)));
  }
  else  free_environments[n] = p->link;
  p->ref_cnt      = 1;
  p->sz	          = n;
  p->most_entries = 0;
  p->mark	  = 0;

  /*----------------------------*
   * Set each entry to NOTHING. *
   *----------------------------*/

  {register int i;
   struct envcell* c = p->cells;
   for(i = 0; i < size; i++) c[i].val = NOTHING;
  }

# ifdef DEBUG
    allocated_environments++;
# endif

  return p;
}  


/****************************************************************
 *                 ALLOCATE_LOCAL_ENV                           *
 ****************************************************************
 * Same as allocate_env, but the new environment node is	*
 * marked local.						*
 ****************************************************************/

ENVIRONMENT* allocate_local_env(int n)
{
  register ENVIRONMENT *env;

  env = allocate_env(n);
  env->kind = LOCAL_ENV;
  return env;
}


/***************************************************************
 *                 BUMP_ENV                                    *
 ***************************************************************
 * Bump_env(E,NE) bumps the ref count of E, and also bumps E's *
 * refs[NE-1] field.                                           *
 ***************************************************************/

void bump_env(ENVIRONMENT *e, int ne)    
{
  if(e != NULL) {

#   ifdef GCTEST
      if(e != NULL && e->ref_cnt < 0) {
	badrc("environment", toint(e->ref_cnt), (char *) e);
      }
#   endif

    e->ref_cnt++;
    if(ne > 0) e->cells[ne-1].refs++;
  }
}


/******************************************************************
 *                 DROP_ENV                                       *
 ******************************************************************
 * Drop_env(E,NE) drops the ref count of E, freeing if the new    *
 * ref count is 0.  If the new ref count is not 0, then refs[NE-1]*
 * in E is decremented as well, and the most_entries field is     *
 * updated.                                                       *
 ******************************************************************/

void drop_env(ENVIRONMENT *env, int ne)
{
  for(;;) {
    if(env == NULL) return;

#   ifdef GCTEST
      if(env->ref_cnt <= 0) {
	badrc("environment", toint(env->ref_cnt), (char *)env);
      }
#   endif

    if(ne < 0 || ne > env->most_entries) {
      die(138, toint(ne), toint(env->most_entries));
    }

    /*----------------------------------------------------------*
     * If this environment node survives the drop, then do 	*
     * a local drop in cell ne.  Update most_entries if that	*
     * is necessary.						*
     *----------------------------------------------------------*/

    if(--(env->ref_cnt) > 0) {
      register int i;
      register struct envcell *q;

      if(ne > 0) env->cells[ne-1].refs--;
      for(i = env->most_entries, q = env->cells + i - 1; 
	  i > 0 && q->refs == 0; i--,q--);
      env->most_entries = i;
      return;
    }

    /*----------------------------------------------------------------*
     * If this node does not survive the drop, then place this node   *
     * in the free space list and drop the link, but don't free	      *
     * global environments, since they can be referred 		      *
     * to by entities.                                                *
     *----------------------------------------------------------------*/

    else {
      ENVIRONMENT *p;
      register int i;

      if(env->kind == GLOBAL_ENV) return;

      p  = env->link;
      ne = env->num_link_entries;

#     ifdef DEBUG
        if(rctrace) trace_i(19, env);
        allocated_environments--;
#     endif

#     ifdef GCTEST
        env->ref_cnt            = -100;
#     else
        i 			= env->sz;
        env->link 	 	= free_environments[i];
        free_environments[i] 	= env;
#     endif

      env 		  	= p;
    }
  }
}


/***************************************************************
 *                 DROP_ENV_REF                                *
 ***************************************************************
 * Just drop the reference in environment ENV to offset NE.    *
 ***************************************************************/

void drop_env_ref(ENVIRONMENT *env, int ne)
{
  if(ne > 0) env->cells[ne-1].refs--;
}


/***************************************************************
 *                 BUMP_ENV_REF                                *
 ***************************************************************
 * Just bump the reference in environment ENV to offset NE.    *
 ***************************************************************/

void bump_env_ref(ENVIRONMENT *env, int ne)
{
  if(ne > 0) env->cells[ne-1].refs++;
}


/****************************************************************
 *			TRAP VECTORS				*
 ****************************************************************/

/********************************************************
 *			FREE SPACE LIST			*
 ********************************************************/

PRIVATE TRAP_VEC* free_trap_vecs = NULL;

/***************************************************************
 *                 ALLOCATE_TRAP_VEC                           *
 ***************************************************************
 * Return a new trap_vec node with ref count 0.                *
 ***************************************************************/

TRAP_VEC* allocate_trap_vec(void)
{
  TRAP_VEC* tr = free_trap_vecs;
  if(tr == NULL) {
    tr = (TRAP_VEC *) 
	 alloc_small((sizeof(TRAP_VEC) 
			      + (trap_vec_size-1) * sizeof(LONG)));
  }
  else free_trap_vecs = (TRAP_VEC *) (tr->component[0]);
  tr->ref_cnt = 0;

# ifdef DEBUG
    allocated_trap_vecs++;
# endif

  return tr;
}


/***************************************************************
 *                 DROP_TRAP_VEC                               *
 ***************************************************************
 * Decrement the reference count of trap_vec node TR, and      *
 * possibly free it.  TR can be NULL.                          *
 ***************************************************************/

void drop_trap_vec(TRAP_VEC *tr)
{
# ifdef GCTEST
    if(tr != NULL && tr->ref_cnt <= 0) {
      badrc("trap_vec", toint(tr->ref_cnt), (char *) tr);
    }
# endif

  if(tr != NULL) {
    if(--(tr->ref_cnt) == 0) {

#     ifdef DEBUG
        if(rctrace) trace_i(20, tr);
        allocated_trap_vecs--;
#     endif

#     ifdef GCTEST
        tr->ref_cnt = -100;
#     else
        tr->component[0] = tolong(free_trap_vecs);
        free_trap_vecs   = tr;
#     endif
    }
  }
}

/***************************************************************
 *                 SET_TRAP_VEC                                *
 ***************************************************************
 * Set variable *X to TV, keeping track of reference counts.   *
 ***************************************************************/

void set_trap_vec(TRAP_VEC **x, TRAP_VEC *tv)
{
  bump_trap_vec(tv);
  drop_trap_vec(*x);
  *x = tv;
}


/****************************************************************
 *			BADRC					*
 ****************************************************************
 * Complain that a bad reference count has been encountered.    *
 * This is only used in GCTEST mode.				*
 ****************************************************************/

#ifdef GCTEST
void badrc(char *where, int rc, char *p)
{
  fprintf(TRACE_FILE, "\nBad ref count %d in %s on %p\n\n", rc, where, p);
  die(8);
}
#endif
