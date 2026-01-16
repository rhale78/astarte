/**********************************************************************
 * File:    machstrc/control.c
 * Purpose: Implement of the control of the abstract machine.
 * Author:  Karl Abrahamson
 **********************************************************************/

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

/****************************************************************
 * This file implements the control, which keeps information 	*
 * about suspended threads, and information for try constructs, *
 * telling where to go when there is a failure.			*
 *								*
 * At any given point, the "activation" is really a tree 	*
 * describing a collection of threads.  The control is that 	*
 * tree. The internal nodes of the tree have type CONTROL, and	*
 * the leaves have type ACTIVATION.  Controls are stored in two *
 * forms.  							*
 *								*
 * 1. A DOWN-CONTROL is the tree, held on to at its root.  	*
 * Each control has a (possibly empty) left subtree and a	*
 * (possibly empty) right subtree, and the pointers point	*
 * downwards in the tree. Type DOWN_CONTROL is used for such	*
 * controls.  							*
 *								*
 * 2. When an activation is running, its control is held onto	*
 * from the leaf that is currently running, with parent links	*
 * pointing up toward the root.  It is still the same tree, but *
 * stored differently, with the links pointing different	*
 * directions.  The up links are only found on the path from    *
 * the current leaf to the root.  All other pointers point 	*
 * downwards, just as they would in a down-control. 		*
 * Type UP_CONTROL is used for such controls.			*
 *								*
 * Type CONTROL can be either UP_CONTROL or DOWN_CONTROL.  The	*
 * only difference between UP_CONTROLs and DOWN_CONTROLs is the *
 * way the links are arranged.					*
 *								*
 * Union type ctl_or_act is a union of DOWN_CONTROL and		*
 * ACTIVATION.  It is used for activations that might have	*
 * additional control information and might not.  Since the 	*
 * children of a control node might be either controls or 	*
 * activations, type ctl_or_act is used for them.		*
 *								*
 * Nodes in the control tree are of the following kinds.	*
 *								*
 * mix:       A "mix" construct.  In a down-control, there is	*
 *	      a left child and a right child, each a ctl_or_act.*
 *            The children are computations that are		*
 *	      running in parallel.  During execution, the left	*
 *	      child will be run first.				*
 *								*
 *	      In an up-control, there is a child and a parent.  *
 *	      The child is the sibling of the child from which  *
 *            this node is pointed to from below.  It can be    *
 *            either a left sibling or a right sibling.  See    *
 *            below on how to tell which it is.			*
 *	      The parent pointer is in the left field, and the  *
 *            other child is in the right field of the control. *
 *								*
 * branch:    A "stream" construct.  In a down-control, there	*
 *	      is a left child and a right child, each a		*
 *	      ctl_or_act.  The left child is the currently	*
 *            active computation, and the right child is        *
 *            suspended until the left child fails.  		*
 *								*
 *	      In an up-control, there is a parent and a child.  *
 *            The link to this node is always pointed to by the *
 *            left child, and the child is the right child.     *
 *            The left pointer points to the parent of this	*
 *	      node.						*
 *								*
 * try:       A "try" construct.  In a down-control, there is	*
 *	      left child and a right a child .  The left child  *
 *	      is the computation that is running within the     *
 *            test of the try.  The right child is the 		*
 *	      computation to be resumed if this try-node	*
 *	      catches a failure.  				*
 *								*
 *	      In an up-control, there is a parent and a child.  *
 *								*
 *	      A try node has an identity that is used so that   *
 *	      an activation can know which tries it is inside.	*
 *	      The embedding_tries field of the activation 	*
 *	      tells which tries are active.			*
 *	 							*
 *	      A try node can be inactivated.  If its right	*
 *	      child (in a down-control) or its child	        *
 *	      (in an up-control) is NULL, then the try has been *
 *	      deactivated.  A deactivated try cannot be used	*
 *	      to catch a failure.				*
 *								*
 * try-each:  A "try catchingEachThread" construct.  This is	*
 *	      similar to a try node, but it is not deactivated 	*
 *	      when the first thread exits it.  It applies to	*
 *	      all threads that are created in the body as well	*
 *	      as the one that entered.				*
 *								*
 * try-term:  A "try catchingAllExceptions" construct.  This is *
 *	      just like a try node, but it can catch a		*
 *	      terminateX exception, while a try cannot.		*
 *								*
 * try-each-term: A try-each-term node is just like a try-each	*
 *	      node, but it can catch a terminateX 		*
 *	      exception, while a try cannot.			*
 *								*
 * mark:      A mark node marks the beginning of a CutHere	*
 *            construct.  In a down-control, there is a child   *
 *            that is the control beneath the mark.  In an	*
 *            up-control, there is a parent pointer		*
 *								*
 *	      A mark node has an identity that is used to allow *
 *	      an activation to know which cuthere constructs	*
 *	      it is inside.  The embedding_marks field of	*
 *	      the activation tells that information.		*
 *								*
 *            When a cut occurs, the control is always stored   *
 *	      as an up-control.  The control is searched, via   *
 *            parent pointers, for a mark node, and everything 	*
 *	      beneath the mark node except the thread that did  *
 *            the cut is terminated.				*
 *								*
 * A node that is pointed to from below uses its left child 	*
 * link to point to its parent, and its right child link to 	*
 * point downwards.  PARENT_CONTROL can be used to name the	*
 * left.ctl field when it is used as a parent pointer.		*
 *								*
 * The info field in a control node describes properties of	*
 * the control node.  						*
 *								*
 * Bits 0,1,2: (KIND_CTLMASK)					*
 *  MIX_F:        a mix node					*
 *  BRANCH_F :    a branch node					*
 *  TRY_F:        a try node					*
 *  TRYEACH_F:	  a try-each node				*
 *  TRYTERM_F:	  a try-term node				*
 *  TRYEACHTERM_F: a try-each-term node				*
 *  MARK_F:       a mark node					*
 *								*
 * Bit 3: (LCHILD_CTLMASK)					*
 *   CTL_F(0):     left child or parent points to another 	*
 *		   control.  This is always the case when the   *
 *                 pointer is a parent pointer.			*
 *								*
 *   ACT_F(1):     left child is an activation.			*
 *								*
 * Use macro LCHILD_IS_ACT(c) to get a value that is 0 if	*
 * the left child is a control and nonzero if the left child is *
 * an activation.  Use TRUE_LCHILD_KIND(c) to get a value that	*
 * is 0 if the left child is a control and 1 if the left child	*
 * is an activation.						*
 *								*
 * Bit 4: (RCHILD_CTLMASK)					*
 *  CTL_F(0):     right child is a control.			*
 *  ACT_F(1):     right child is an activation.			*
 *								*
 * Use macro RCHILD_IS_ACT(c) to get a value that is 0 if 	*
 * the right child is a control and nonzero if the right child	*
 * is an activation.  Use TRUE_RCHILD_KIND(c) to get a value	*
 * that is 0 if the right child is a control and 1 if the right	*
 * child is an activation.					*
 *								*
 * Bit 5: (UPLINK_CTLMASK)					*
 * Bit 5 is used for mix nodes in up-controls.  It tells        *
 * whether the link up to this node comes from the left or      *
 * the right.  							*
 *  LEFT_F(0):	the up-link comes from the left child.  So	*
 * 		the child of this node must be the right child.	*
 *								*
 *  RIGHT_F(1): up link to this node comes up from the right.	*
 *		So the child is the left child.			*
 *								*
 * Use UPLINK_IS_FROM_RIGHT(c) to get a value that is 0 if the	*
 * uplink is  from the left, and is nonzero if the uplink is	*
 * from the right.						*
 *								*
 * Other fields are:						*
 *								*
 * ref_cnt	Reference count for storage management.		*
 * mark		For garbage collection traversal.		*
 * identity     Identity of a mark, try node.  			*
 *		This is used to keep track of nodes.		*
 * left.act	Left activation					*
 * left.ctl	Left control					*
 * right.act	Right activation				*
 * right.ctl	Right control					*
 ****************************************************************/

#include "../misc/misc.h"
#include "../utils/lists.h"
#include "../utils/miscutil.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

PRIVATE union ctl_or_act 
get_down_control_c(UP_CONTROL *c, union ctl_or_act leaf, int leaf_kind,
		   int *res_kind);

PRIVATE union ctl_or_act 
down_eaches(union ctl_or_act dc, int dc_kind, UP_CONTROL *eaches, 
	   int* res_kind);

PRIVATE CONTROL* new_control(int kind, LONG id, 
			    union ctl_or_act left, int left_kind,
			    union ctl_or_act right, int right_kind);

PRIVATE union ctl_or_act 
terminate_ctl_or_act_help(union ctl_or_act coa, int coa_kind, 
			  int *res_kind, Boolean force);

PRIVATE UP_CONTROL* copy_up_control_to_try (UP_CONTROL* c);
PRIVATE UP_CONTROL* extract_eaches	   (UP_CONTROL* c);
PRIVATE UP_CONTROL* push_down_eaches_at_try(UP_CONTROL *c, UP_CONTROL *eaches);

PRIVATE void set_termination_marks  (DOWN_CONTROL* dc);
PRIVATE void clear_termination_marks(DOWN_CONTROL* dc);


/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			NULL_CTL_OR_ACT				*
 ****************************************************************
 * NULL_CTL_OR_ACT is a ctl_or_act value that has a NULL pointer*
 * in it.  It is initialized in intrprtr.c:init_interpreter,	*
 * because not all compilers allow a cast to a union.		*
 * (Isn't this a silly language?)				*
 ****************************************************************/

union ctl_or_act NULL_CTL_OR_ACT;

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 *			next_control_identity			*
 ****************************************************************
 * next_control_identity is the next identity to give to a	*
 * mark or try node. The initial value should be 		*
 * positive, since 0 is used as an indication that there is	*
 * no identity when getting an identity from an activation.	*
 ****************************************************************/

PRIVATE LONG next_control_identity = 1;

/****************************************************************
 *			ctl_kind_name				*
 ****************************************************************
 * ctl_kind_name[k] is the name of tag k for controls.		*
 ****************************************************************/

#ifdef DEBUG
 PRIVATE char *ctl_kind_name[] = 
  {"MIX", "BRANCH", "TRY", "TRY-TERM", "TRY-EACH", "TRY-EACH-TERM", "MARK"};
#endif


/****************************************************************
 *			PUSH_CONTROL				*
 ****************************************************************
 * Push a control node beneath the_act.control.  Since		*
 * the_act.control is an up-control, this adds a new leaf to	*
 * the tree.							*
 *								*
 * Parameter info tells what to put in the info field of this	*
 * new leaf node.						*
 *								*
 * Return a pointer to the new control node.			*
 ****************************************************************/

PRIVATE UP_CONTROL* push_control(int info)
{
  register UP_CONTROL* p = allocate_control();

  p->info           = info;
  p->PARENT_CONTROL = the_act.control;  /* Ref from the_act.control. */
  bmp_control(the_act.control  = p);
  return p;
}


/****************************************************************
 *			MARK_C					*
 ****************************************************************
 * Push a MARK_F node onto the the_act.control.  This is used   *
 * to start a CutHere construct.  A new identity is assigned to	*
 * the node, and that identity is returned.			*
 ****************************************************************/

LONG mark_c()
{
  register UP_CONTROL* c= push_control(MARK_F);
  c->right.ctl = NULL;
  return c->identity = next_control_identity++;
}
  

/****************************************************************
 *			PUSH_TRY_C				*
 ****************************************************************
 * Push a new try node onto the_act.control, with its down link	*
 * going to a copy of the_act, except that the program counter	*
 * of that copy is pc.  The kind of try node (TRY_F, etc.) is	*
 * given in parameter kind.  					*
 *								*
 * The return value is the identity of the new node.		*
 *								*
 * Use the following  macros from machstrc.h for easier access.	*
 *								*
 *   try_c(pc)		Start a try construct, where pc is the	*
 *			address of the 'else' part.		*
 *			In this case, the node is given a new	*
 *			identity, and that identity is returned.*
 *								*
 *   tryeach_c(pc)	Start a try-all construct, where pc is	*
 *			the address of the 'else' part.		*
 *			In this case, the node is given a new	*
 *			identity, and that identity is returned.*
 *								*
 *   tryterm_c(pc)	Start a try-term construct, where pc is *
 *			the address of the 'else' part.		*
 *			In this case, the node is given a new	*
 *			identity, and that identity is returned.*
 *								*
 *   tryeachterm_c(pc)	Start a try-each-term construct, where  *
 *			pc is the address of the 'else' part.	*
 *			In this case, the node is given a new	*
 *			identity, and that identity is returned.*
 ****************************************************************/

LONG push_try_c(CODE_PTR pc, int kind)
{
  register ACTIVATION *p;
  register UP_CONTROL *c;

  p = copy_the_act();        /* Ref from copy_the_act */
  p->program_ctr = pc;
  c = push_control(kind | RCHILD_CTLMASK);
  c->right.act = p;          /* Ref from p */
  return c->identity = next_control_identity++;
}



/****************************************************************
 *			PUSH_BRANCH_C				*
 ****************************************************************
 * Push a new branch or mix node onto the_act.control,		*
 * with its down link pointing to a copy of the_act, except 	*
 * that the program counter of that copy is pc.  		*
 *								*
 * Parmater kind tells which kind of branch node (BRANCH_F or	*
 * MIX_F) to push.						*
 *								*
 * Use the following macros from machstrc.h for easier access.	*
 *								*
 *   branch_c(pc)	Start a steam construct where pc is	*
 *			the address of the deferred code.	*
 *								*
 *   mix_c(pc)		Start a mix construct where pc is the	*
 *			address of code forthe second thread.   *
 *								*
 * When a mix or branch node is pushed, it must move above any  *
 * TRYEACH or TRYEACHTERM nodes, since those nodes apply	*
 * separately to each thread.  For example, the following    	*
 * tranformations are done, where M is a mix or branch node and *
 * E is a TRYEACH or TRYEACHTERM node.				*
 *								*
 *       E             M					*
 *       |            / \					*
 *       M      =>   E   E					*
 *      / \          |   |					*
 *     X   Y         X   Y					*
 *								*
 * where the two E's are copies of the original E.		*
 * If there is a chain of TRYEACH or TRYEACHTERM nodes, then	*
 * the entire chain is moved below the mix or branch node.	*
 *								*
 * Mark nodes are left in place but looked across when doing	*
 * this transformation.  For example, the following change is	*
 * made, where C is a MARK_F node.				*
 *								*
 *       E             C					*
 *       |             |					*
 *       C             M					*
 *       |            / \					*
 *       M      =>   E   E					*
 *      / \          |   |					*
 *     X   Y         X   Y					*
 *								*
 ****************************************************************/

void push_branch_c(CODE_PTR pc, int kind)
{
  ACTIVATION *this_act;
  UP_CONTROL *c;

  /*--------------------------------------------------------------------*
   * First clean the control to get rid of any extraneous tries.	*
   *--------------------------------------------------------------------*/

  SET_CONTROL(the_act.control, clean_up_control_c(the_act.control, &the_act));

# ifdef DEBUG
    if(trace_control > 1) {
      trace_i(322);
      trace_i(85);
      print_up_control(TRACE_FILE, the_act.control, 1);
    }
# endif

  /*--------------------------------------------------------------------*
   * Check whether there is a TRYEACH_F or TRYEACHTERM_F node		*
   * that needs to be pushed down.  To do that check, find the first	*
   * non-MARK_F node.  If it is a TRYEACH_F or TRYEACHTERM_F node, then	*
   * there is one to push down.						*
   *--------------------------------------------------------------------*/

  c = the_act.control;
  while(c != NULL && CTLKIND(c) == MARK_F) c = c->PARENT_CONTROL;
  
  /*--------------------------------------------------------------------*
   * If there are no each nodes to move down, then just push the new	*
   * node.								*
   *--------------------------------------------------------------------*/

  if(c == NULL || CTLKIND(c) < TRYEACH_F) {
    this_act = copy_the_act();        /* Ref from copy_the_act */
    this_act->program_ctr = pc;
    c = push_control(kind | RCHILD_CTLMASK);
    c->right.act = this_act;          /* Ref from this_act */
    c->identity = 0;
  }

  /*--------------------------------------------------------------------*
   * If there is a TRYEACH_F or TRYEACHTERM_F node, then it needs to	*
   * be moved down.							*
   *									*
   * Scan the_act.control, producing three chains.			*
   *									*
   *   parnt : the first node found that is not a MARK_F		*
   *           or TRYEACH_F or TRYEACHTERM_F node.			*
   *									*
   *   marks : the chain of mark nodes encountered, in the order	*
   *           in which they were encountered.  marksTop points to	*
   *           the end of this chain, if there is one, or is		*
   *           NULL if this chain is empty.				*
   *									*
   *   eaches: the chain of TRYEACH_F and TRYEACHTERM_F nodes		*
   *           encountered, in the order encountered.  eachesTop	*
   *           points to the end of this chain, or is NULL if the	*
   *           chain is empty.						*
   *--------------------------------------------------------------------*/

  else {
    UP_CONTROL* parnt     = the_act.control;
    UP_CONTROL* marks     = NULL;
    UP_CONTROL* marksTop  = NULL;
    UP_CONTROL* eaches    = NULL;
    UP_CONTROL* eachesTop = NULL;
    CONTROL *ctl;
    int right_kind;

    while(parnt != NULL && CTLKIND(parnt) >= TRYEACH_F) {
      bump_control(ctl = copy_control(parnt, FALSE));
      if(CTLKIND(parnt) == MARK_F) {
	if(marksTop == NULL)  marks = ctl;    /* ref from ctl */
	else marksTop->PARENT_CONTROL = ctl;  /* ref from ctl */
        marksTop = ctl;
      }
      else {
	if(eachesTop == NULL)  eaches = ctl;  /* ref from ctl */
	else eachesTop->PARENT_CONTROL = ctl;  /* ref from ctl */
	eachesTop = ctl;
      }
      parnt = parnt->PARENT_CONTROL;
    }

    /*----------------------------------------------------------*
     * Now combine a new branch or mix node with parnt and the	*
     * marks and eaches chains to build the tree.		*
     *								*
     * The tree needs to be as follows.				*
     * 								*
     *               parnt					*
     *                 |					*
     *                 C					*
     *                 |					*
     *                ...  chain marks (if not null)		*
     *                 |					*
     *                 C					*
     *                 | 					*
     *                 M   new MIX_F or BRANCH_F node c		*
     *                / \					*
     *               E   E					*
     *               |   |					*
     *   chain     ...  ...  chain eaches, but with pointers	*
     *   eaches      E   E   reversed so that they point down.	*
     *               |   |					*
     *               |   this_act (copy of the_act with new pc)	*
     *               |						*
     *               ----<this is the result			*
     *----------------------------------------------------------*/

    {union ctl_or_act c_parent;   /* Always a control */
     union ctl_or_act this_act_u; /* Always an activation */

     if(marks == NULL) c_parent.ctl = parnt;
     else {
       c_parent.ctl = marks;
       bump_control(marksTop->PARENT_CONTROL = parnt);
     }
     this_act = copy_the_act();        /* Ref from copy_the_act */
     this_act->program_ctr = pc;
     this_act_u.act = this_act;
     eachesTop->PARENT_CONTROL = NULL;
     c = new_control(kind, 0, c_parent, CTL_F,
		     get_down_control_c(eaches, this_act_u, 
					ACT_F, &right_kind), 
		     CTL_F);
     bump_control(eachesTop->PARENT_CONTROL = c);
     SET_CONTROL(the_act.control, eaches);
     drop_activation(this_act);
     drop_control(eaches);
    }
  }

# ifdef DEBUG
    if(trace_control > 1) {
      trace_i(322);
      trace_i(323);
      print_up_control(TRACE_FILE, the_act.control, 1);
    }
# endif
}


/****************************************************************
 *			GET_DOWN_CONTROL_C			*
 ****************************************************************
 * Return the down-control that is the same as up-control c,    *
 * with control or activation leaf as the leaf that is		*
 * at the bottom of the main path (by which c is being held	*
 * onto).  It is allowed for leaf actually to be a tree (a	*
 * down-control).						*
 *								*
 * leaf_kind is the kind of leaf (CTL_F or ACT_F).		*
 * 								*
 * If c is NULL then return leaf.				*
 *								*
 * *res_kind is set to the kind of the result.			* 
 ****************************************************************/

PRIVATE union ctl_or_act 
get_down_control_c(UP_CONTROL *c, union ctl_or_act leaf, int leaf_kind,
		   int *res_kind)
{
  union ctl_or_act result;
  int              result_kind;
  UP_CONTROL*      p;

  result = leaf;
  result_kind = leaf_kind;
  p = c;
  while(p != NULL) {
    result.ctl = new_control(CTLKIND(p), p->identity, result, result_kind,
			     p->right, RCHILD_IS_ACT(p));
    result_kind = CTL_F;
    p = p->PARENT_CONTROL;
  }

  *res_kind = result_kind;
  return result;
}


/****************************************************************
 *			GET_UP_CONTROL_C			*
 ****************************************************************
 * This function is used to convert a down-control into an	*
 * up-control.							*
 *								*
 * Convert down-control c with parent 'parent' into a pair 	*
 * consisting of an activation A and an up-control uc.  Set 	*
 * *act to A and return uc.  Neither has its reference count 	*
 * bumped.  It is important for the caller to bump the parts of *
 * *act.							*
 * 								*
 * Set *l_time to SLICE_TIME if a mix node is encountered on 	*
 * the path from c down to the activation A, or if the bottom   *
 * node in parent is a mix node. and *l_time is currently 	*
 * greater than SLICE_TIME.  This is important to limit the	*
 * execution time when there is another thread that wants to 	*
 * run.  Note that, if parent contains a mix node above its	*
 * bottom node, it the responsibility of the caller to see that *
 * l_time is no larger than SLICE_TIME.				*
 *								*
 * It is the responsibility of the caller to set the info at 	*
 * parent, for c to point up to it.  That is, the parent must   *
 * say how it is related to c.  				*
 ****************************************************************/

UP_CONTROL* 
get_up_control_c(DOWN_CONTROL *c, UP_CONTROL *parent, 
		 ACTIVATION **act, LONG *l_time)
{
  UP_CONTROL *result, *newp;
  union ctl_or_act p;
  int p_kind;

  /*---------------------------------*
   * The following should not occur. *
   *---------------------------------*/

  if(c == NULL) {
    *act = NULL;
    return NULL;
  }

  /*------------------------------------------------------------*
   * Build the up control, result.  start it at parent, and	*
   * build downwards from there.  Note that the parent might	*
   * be a MIX_F node, and we need to take that into account	*
   * by cutting down the time.					*
   *------------------------------------------------------------*/

  bump_control(result = parent);
  if(parent != NULL && CTLKIND(parent) == MIX_F && *l_time > SLICE_TIME) {
    *l_time = SLICE_TIME;
  }
  p.ctl  = c;
  p_kind = CTL_F;

  while(p_kind == CTL_F) {

    /*----------------------------------------------------------*
     * Copy node p to result, but skip over BRANCH and MiX	*
     * nodes with null right child, since they are of no 	*
     * interest. (They are present because a thread failed.) 	*
     *----------------------------------------------------------*/

    if(CTLKIND(p.ctl) > BRANCH_F || p.ctl->right.ctl != NULL) {
      newp = allocate_control();
      newp->info = p.ctl->info & ~LCHILD_CTLMASK; /* Parent is a control. */
      newp->identity = p.ctl->identity;
      newp->PARENT_CONTROL = result; 		  /* Install parent.      */
      						  /* Ref from result.     */
      newp->right = p.ctl->right;		  /* Install right child  */
						  /* as only child.       */
      bmp_ctl_or_act(newp->right, RCHILD_IS_ACT(p.ctl));
      bmp_control(result = newp);
      if(CTLKIND(result) == MIX_F && *l_time > SLICE_TIME) {
	*l_time = SLICE_TIME;
      }
    }

    /*------------------------*
     * Move down to the left. *
     *------------------------*/

    p_kind = LCHILD_IS_ACT(p.ctl);
    p = p.ctl->left;
  }

  *act = p.act;
  if(result != NULL) result->ref_cnt--;
  return result;
}
      

/****************************************************************
 *			GET_UP_CONTROL_FROM_COA_C		*
 ****************************************************************
 * Similar to get_up_control_c, but gets from a control or 	*
 * activation.  If c is an activation, then *act is set to c,	*
 * and parent is returned.					*
 ****************************************************************/

UP_CONTROL* get_up_control_from_coa_c(union ctl_or_act coa, int coa_kind,
				      UP_CONTROL *parent, 
				      ACTIVATION **act, LONG *l_time)
{
  if(coa_kind == CTL_F) {
    return get_up_control_c(coa.ctl, parent, act, l_time);
  }
  else {
    *act = coa.act;
    return parent;
  }
}


/****************************************************************
 *			SHIFT_UP_CONTROL_C			*
 ****************************************************************
 * Return a version of UP_CONTROL c that is obtained by making	*
 * it so that failing into it causes the next thread to be 	*
 * evaluated.  This involves scanning up until a mix node is 	*
 * encountered, coming up from the left.  Then that mix node	*
 * has its children transposed, so that the right-child will 	*
 * take over execution.  If no such mix node is found,  then    *
 * try to restart c at the first thread in c.  If there are no	*
 * other threads, then return c (unchanged). 			*
 * 								*
 * maybe_shift_up_control_c returns either c or 		*
 * shift_up_control_c(c), depending on a psuedo-random bit.	*
 ****************************************************************/


UP_CONTROL* maybe_shift_up_control_c(UP_CONTROL *c)
{
  if(sortof_random_bit()) {
    return shift_up_control_c(c);
  }
  else return c;
}

/*---------------------------------------------------------------*/

UP_CONTROL* shift_up_control_c(UP_CONTROL *c)
{
  union ctl_or_act coa, left_side;
  UP_CONTROL *top, *result, *cont;
  CONTROL *p;
  ACTIVATION *act;
  LONG l_time;
  int left_side_kind;

# ifdef DEBUG
    if(trace) {
      trace_i(210);
      trace_i(205);
      print_up_control(TRACE_FILE, c, 1);
      tracenl();
    }
# endif

  /*------------------------------------------------*
   * If there are no mix nodes in c, then return c. *
   * Label there_are_mixes is just below this 	    *
   * paragraph.					    *
   *------------------------------------------------*/

  for(p = c; p != NULL; p = p->PARENT_CONTROL) {
    if(CTLKIND(p) == MIX_F) goto there_are_mixes;
  }

# ifdef DEBUG
    if(trace) {
      trace_i(210);
      trace_i(206);
    }
# endif

  return c;

 there_are_mixes:

  /*--------------------------------------------------------------------*
   * Try to find a mix node to swap.  get_timeout_parts does that.	*
   * If a mix node is found, the return value is the left-child of	*
   * that mix node, and top is set to the mix node where the		*
   * search stopped.  If no such mix node is found, then the return	*
   * value is the full control, which should then be restarted. 	*
   *--------------------------------------------------------------------*/

  coa.ctl = NULL;
  left_side = get_timeout_parts(c, CTL_F, coa, &left_side_kind, &top);

# ifdef DEBUG
    if(trace) {
      trace_i(210);
      trace_i(207);
      if(left_side_kind == CTL_F) {
	print_down_control(TRACE_FILE, left_side.ctl, "", 1);
      }
      else fprintf(TRACE_FILE, "<%s>", left_side.act->name);
      fprintf(TRACE_FILE, "\ntop =\n");
      print_up_control(TRACE_FILE, top, 1);
      tracenl();
    }
# endif
  
  /*--------------------------------------------------------------------*
   * If there are no appropriate mix nodes, then just move back down 	*
   * into left_side. 							*
   *--------------------------------------------------------------------*/

  l_time = SLICE_TIME;
  if(top == NULL) {

#   ifdef DEBUG
      if(trace) {
	trace_i(210);
        trace_i(208);
      }
#   endif

    bump_control(cont = 
	 get_up_control_from_coa_c(left_side, left_side_kind, 
				   NULL, &act, &l_time));
    bump_activation(act);
  } 

  /*----------------------------------------------------------------------*
   * If there is an appropriate mix node, then move into its right child. *
   *----------------------------------------------------------------------*/

  else { /* top != NULL */ 

    /*---------------------------------------------------------------------*
     * Set p to the parent, above the child of top.  If left_side != NULL, *
     * this is a copy of mix node top, with up child set to come from	   *
     * the right. Otherwise, p is the parent of top. 			   *
     *---------------------------------------------------------------------*/

    bump_control(top);
    if(left_side.ctl == NULL) {
      bump_control(p = top->PARENT_CONTROL);
    }
    else {
      bmp_control(p = allocate_control());
      p->info = (top->info & KIND_CTLMASK) | UPLINK_CTLMASK;
      if(left_side_kind != CTL_F) p->info |= RCHILD_CTLMASK;
      p->identity = top->identity;
      bump_ctl_or_act(p->right = left_side, left_side_kind);
      bump_control(p->left.ctl = top->left.ctl);
    }
    bump_control(cont =
        get_up_control_from_coa_c(top->right, RCHILD_IS_ACT(top), 
				  p, &act, &l_time));
    bump_activation(act);
    drop_control(p);
    drop_control(top);
  } /* end else(top != NULL) */

  /*--------------------------------------------------------------------*
   * Build the node at the bottom of the control holding the activation *
   * that will start at failure. 					*
   *--------------------------------------------------------------------*/

  if(cont == NULL && act == NULL) {
#   ifdef DEBUG
      if(trace) {
	trace_i(210);
        trace_i(209);
      }
#   endif
    return NULL;
  }

  result            = allocate_control();
  result->info      = BRANCH_F | RCHILD_CTLMASK;
  result->identity  = 0;
  result->right.act = act;      	/* ref from act */
  result->PARENT_CONTROL  = cont;       /* ref from cont */

# ifdef DEBUG
    if(trace) {
      trace_i(210);
      trace_i(211);
      print_up_control(TRACE_FILE, result, 1);
      tracenl();
    }
# endif

  return result;
}

  
/********************************************************
 *			START_CTL_OR_ACT		*
 ********************************************************
 * Start up_control or activation a, of kind k (0 for a *
 * control, nonzero for an activation).  The parent 	*
 * control is parent. 					*
 *							*
 * Set l_time to SLICE_TIME if necessary.  This will 	*
 * only be done if a mix node is seen in a, or the	*
 * bottom node of parent is a mix.  If parent contains	*
 * a mix node above its bottom node, it is the 		*
 * responsibility of the called to set l_time low.	*
 ********************************************************/

void start_ctl_or_act(int k, union ctl_or_act a, UP_CONTROL *parent, 
		      LONG *l_time)
{
  if(k == CTL_F) {

#   ifdef DEBUG
      if(trace) {
        trace_i(98);
	if(trace_control) {
          print_down_control(TRACE_FILE, a.ctl, "", 1);
	}
	tracenl();
      }
#   endif

    move_into_control_c(a.ctl, parent, l_time);
  }

  else {
    install_activation_c(a.act, parent, l_time);
  }

  start_new_activation();
}


/****************************************************************
 *			INSTALL_ACTIVATION_C			*
 ****************************************************************
 * We would like to begin executing activation a.		*
 * Install activation a into the_act, with control c. 		*
 * (So c is really the parent control of a in the tree.)	*
 *								*
 * Set l_time to slice_time if c is a mix control node.		*
 * If c contains a mix node above its bottom node, then it	*
 * is the responsibility of the caller to set l_time low.	*
 ****************************************************************/

void install_activation_c(ACTIVATION *a, UP_CONTROL *c, LONG *l_time)
{
  bump_activation(a);
  bump_control(c);
  drop_activation_parts(&the_act);
  the_act = *a;	   
  bump_activation_parts(&the_act);
  SET_CONTROL(the_act.control, clean_up_control_c(c, &the_act));
  if(c != NULL && CTLKIND(c) == MIX_F && *l_time > SLICE_TIME) {
    *l_time = SLICE_TIME;
  }
  drop_activation(a);
  drop_control(c);
}
    

/****************************************************************
 *			MOVE_INTO_CONTROL_C			*
 ****************************************************************
 * Start up an activation for down-control node c, putting the  *
 * activation into the_act.  The parent of c is parent.		*
 * It is the responsibility of the caller to set the info at	*
 * parent, for c to point up to it.  				*
 *								*
 * If parent is a mix node, or if a mix node is seen below c,	*
 * set l_time to SLICE_TIME.  If parent contains a mix node	*
 * above its bottom, it is the responsibility of the caller	*
 * to set l_time low.						*
 ****************************************************************/

void move_into_control_c(DOWN_CONTROL *c, UP_CONTROL *parent, LONG *l_time)
{
  ACTIVATION *a;
  register UP_CONTROL *uc;

  bump_control(uc = get_up_control_c(c, parent, &a, l_time));
  bump_activation(a);
  if(a != NULL) {
    install_activation_c(a, uc, l_time);
  }
  drop_activation(a);
  drop_control(uc);
}
  

/****************************************************************
 *			CLEAN_UP_CONTROL_C			*
 ****************************************************************
 * Return the up-control obtained from c by deleting leading    *
 * try and mark nodes that are not active, and leading mix and  *
 * branch nodes that are cancelled.	  			*
 *								*
 * Activation a is the activation that holds the embedding_tries*
 * and embedding_marks lists that determine what is to be	*
 * kept.							*
 ****************************************************************/

UP_CONTROL* clean_up_control_c(UP_CONTROL *c, ACTIVATION *a)
{
  register UP_CONTROL* p = c;
  LONG try_id  = -1; /* Set to first_try_id(a->embedding_tries) when needed */
  LONG mark_id = -1; /* Set to first_mark_id(a->embedding_marks) when needed */

# ifdef DEBUG
    if(trace_control) {
      trace_i(122);
      print_up_control(TRACE_FILE, c, 1);
      trace_i(319);
      print_str_list_nl(a->embedding_tries);
      trace_i(321);
      print_str_list_nl(a->embedding_marks);
    }
# endif

  /*--------------------------------------------------------------------*
   * Scan up the control, stopping at the first node that is one of 	*
   * the following:							*
   *    Not a try or a mark node					*
   *    A try node with identity		 			*
   *      first_try_id(a->embedding_tries)				*
   *    A mark node with identity from 					*
   *      first_mark_id(a->embedding_marks)				*
   *--------------------------------------------------------------------*/

  while(p != NULL) {
    int kind = CTLKIND(p);
    switch(kind) {
      case MIX_F:
      case BRANCH_F:
        if(p->right.act != NULL) goto out;
	break;

      case TRY_F:
      case TRYTERM_F:
      case TRYEACH_F:
      case TRYEACHTERM_F:
        if(try_id < 0) try_id = first_try_id(a->embedding_tries);

        if(p->identity == try_id) goto out;
        break;

      case MARK_F:

        if(mark_id < 0) mark_id = first_mark_id(a->embedding_marks);

        if(p->identity == mark_id) goto out;
	break;
    }
    p = p->PARENT_CONTROL;
  }
      
 out:

# ifdef DEBUG
    if(trace_control) {
      trace_i(122);
      trace_i(211);
      print_up_control(TRACE_FILE, p, 1);
      tracenl();
    }
# endif

  return p;
}


/****************************************************************
 *			THEN_C					*
 ****************************************************************
 * This is used when the test of a try is successful.		*
 *								*
 * If the first node in c is a TRY_F or TRYEACH_F or TRYTERM_F 	*
 * or TRYEACHTERM_F node with identity id, then just delete	*
 * that node, and return its parent.				*
 *								*
 * Otherwise, locate the try node with identity id.  If 	*
 * it is a TRY_F or TRYTERM_F node, then set its right child to *
 * NULL.  This deactivates it.  Do not deactivate TRYEACH_F or  *
 * TRYEACHTERM_F nodes.  Return c (leaving the possibily 	*
 * deactivated try in place for other threads.)			*
 *								*
 * If a deactivated try node has a try-each node above it, and 	*
 * there is a branch or mix node below the try node, then	*
 * the try-each node (and others in a chain of them) must 	*
 * be moved below the branch and mix nodes.  For example, if 	*
 * a try node T is found in a tree like the following, then	*
 * the indicated modification must be made.			*
 *								*
 *        E              M					*
 *        |             / \					*
 *        T            E   E					*
 *        |       =>   |   |					*
 *        M            T   T	<-- both cancelled		*
 *       / \           |   |					*
 *      .   A          .   A					*
 *								*
 * where . is the currently running activation, the each-node	*
 * E has been duplicated, and the try node T has been cancelled	*
 * at each occurrence.						*
 ****************************************************************/

UP_CONTROL* then_c(UP_CONTROL *c, LONG id)
{
  int c_kind;

  /*----------------------------------*
   * The following should not happen. *
   *----------------------------------*/

  if(c == NULL) return NULL;

# ifdef DEBUG
    if(trace_control > 1) {
      trace_i(324, id);
      trace_i(85);
      print_up_control(TRACE_FILE, c, 1);
    }
# endif

  /*------------------------------------------------------------*
   * If the bottom node is the desired try node, then it can	*
   * be deleted from the control.  Just return its parent.	*
   *------------------------------------------------------------*/

  c_kind = CTLKIND(c);
  if(IS_TRY(c_kind) && c->identity == id) {

#   ifdef DEBUG
      if(trace_control > 1) {
	trace_i(325);
      }
#   endif

    return c->PARENT_CONTROL;
  }

  else {

    /*------------------------------------------------------------*
     * Search for the try node that is to be cancelled, possibly. *
     * Keep track of the maximum reference count that is seen	  *
     * along the way.  If it is greater than 1, then we cannot	  *
     * make in-place modifications.  Also keep track of whether	  *
     * any BRANCH_F or MIX_F nodes are seen, since their presence *
     * might influence how the control is modified.		  *
     *------------------------------------------------------------*/

    CONTROL* q = NULL;   /* Initialization suppresses warning */
    CONTROL* p = c;
    Boolean ref_cnts_are_all_one = TRUE;
    Boolean has_mixes = FALSE;
    int p_kind = CTLKIND(p);

    do {
      if(p->ref_cnt > 1) ref_cnts_are_all_one = FALSE;
      if(p_kind <= BRANCH_F) has_mixes = TRUE;
      p = p->PARENT_CONTROL;
      if(p == NULL) break;
      p_kind = CTLKIND(p);
    } while(p->identity != id || !IS_TRY(p_kind));

    /*--------------------------------------------------*
     * If this node does not need cancelling, then	*
     * there is nothing to do. Return c unchanged.	*
     *--------------------------------------------------*/

    if(p == NULL || p_kind >= TRYEACH_F) {

#     ifdef DEBUG
        if(trace_control > 1) trace_i(326);
#     endif

      return c;
    }

    /*-----------------------------------------------------------*
     * If we get here, then this is a TRY_F or TRYTERM_F node,   *
     * and it needs to be cancelled.				 *
     *								 *
     * If there are no BRANCH_F or MIX_F nodes on the path up	 *
     * to this try node, then all we need to do is cancel the	 *
     * node.  No rearrangement of the control is required.	 *
     *								 *
     * Similarly, if there are no each nodes just above this 	 *
     * try, then no rearrangement is necessary.			 *
     *-----------------------------------------------------------*/

    /*-----------------------------------------------------------*
     * Find out whether there are any TRYEACH_F or TRYEACHTERM_F *
     * nodes above this node that need to be moved downward.	 *
     * This information is only relevant if there are mix/branch *
     * nodes beneath the try.					 *
     *-----------------------------------------------------------*/

    if(has_mixes) {
      q = p->PARENT_CONTROL;
      while(q != NULL && CTLKIND(q) == MARK_F) q = q->PARENT_CONTROL;
    }

    /*-------------------------------------------------------------*
     * If there are no each nodes to worry about, or if there	   *
     * are no mix/branch nodes, then just worry about cancelling   *
     * this node.  We can cancel in-place if all of the reference  *
     * counts are 1, and must copy otherwise. 			   *
     *-------------------------------------------------------------*/

    if(!has_mixes || q == NULL || CTLKIND(q) < TRYEACH_F) {
      if(ref_cnts_are_all_one && p->ref_cnt <= 1) {

#       ifdef DEBUG
          if(trace_control > 1) trace_i(327);
#       endif

        drop_ctl_or_act(p->right, RCHILD_IS_ACT(p));
        p->right.ctl = NULL;
        return c;
      }

      else {
        UP_CONTROL* result = copy_up_control_to_try(c);

#       ifdef DEBUG
          if(trace_control > 1) {
	    trace_i(328);
	    print_up_control(TRACE_FILE, result, 1);
          }
#       endif

        return result;
      }
    }

    /*------------------------------------------------------------------*
     * If there is a try-each node that needs to be moved downward,	*
     * then move all such nodes.					*
     *------------------------------------------------------------------*/

    else {
      UP_CONTROL* eaches = extract_eaches(p);
      UP_CONTROL* result = push_down_eaches_at_try(c, eaches);

#     ifdef DEBUG
        if(trace_control > 1) {
	  trace_i(329);
	  print_up_control(TRACE_FILE, result, 1);
        }
#     endif

      return result;

    }
  }
}


/****************************************************************
 *		        COPY_UP_CONTROL_TO_TRY			*
 ****************************************************************
 * Return the result of cancelling the first try-node in 	*
 * control c, in the case the nodes up to that try node must	*
 * be copied.							*
 ****************************************************************/

PRIVATE UP_CONTROL* copy_up_control_to_try(UP_CONTROL* c)
{
  UP_CONTROL* ctl;
  UP_CONTROL* result = NULL;
  UP_CONTROL* resultTop = NULL;
  UP_CONTROL* p = c;

  /*------------------------------------------------------------*
   * Build up a copy (result) of the chain leading to the first *
   * try node.							*
   *------------------------------------------------------------*/

  while(p != NULL) {
    int p_kind = CTLKIND(p);
    if(IS_TRY(p_kind)) break;

    bump_control(ctl = copy_control(p, FALSE));
    if(resultTop == NULL) result = ctl;    /* ref from ctl */
    else resultTop->PARENT_CONTROL = ctl;  /* ref from ctl */
    resultTop = ctl;
    p = p->PARENT_CONTROL;
  }

  /*--------------------------------------------------------------*
   * Copy the try node, and add it to the chain.  The copy has    *
   * NULL right child, since it is cancelled.  Note that p should *
   * never be NULL here.			  		  *
   *--------------------------------------------------------------*/

  ctl            = allocate_control();
  ctl->info      = p->info;
  ctl->identity  = p->identity;
  ctl->right.ctl = NULL;
  bump_control(ctl->PARENT_CONTROL = p->PARENT_CONTROL);
  if(resultTop == NULL) result = ctl;    /* ref from ctl */
  else resultTop->PARENT_CONTROL = ctl;  /* ref from ctl */
  result->ref_cnt--;
  return result;
}


/****************************************************************
 *		        EXTRACT_EACHES				*
 ****************************************************************
 * Control c is a chain of nodes that begins with a TRY_F or    *
 * TRYTERM_F node.						*
 *								*
 * Return a chain that begins with a cancelled copy of that     *
 * TRY_F or TRYTERM_F node, followed by the chain of TRYEACH_F  *
 * and TRYEACHTERM_F nodes that follow that node in c, up to	*
 * the first node that is not a TRYEACH_F, TRYEACHTERM_F or	*
 * MARK_F node.  Skip over MARK_F npdes.			*
 ****************************************************************/

PRIVATE UP_CONTROL* extract_eaches(UP_CONTROL* c)
{
  UP_CONTROL *result, *resultTop, *p;

  /*------------------------------------------------------------*
   * Build the first node, a cancelled copy of the first node	*
   * in chain c.						*
   *------------------------------------------------------------*/

  result = resultTop = allocate_control();
  result->info       = CTLKIND(c);
  result->identity   = c->identity;
  result->right.ctl  = NULL;
  bump_control(result);
  
  /*-------------------------------------------*
   * Now get the TRYEACH and TRYEACHTERM nodes *
   *-------------------------------------------*/

  p = c->PARENT_CONTROL;
  while(p != NULL && CTLKIND(p) >= TRYEACH_F) {
    UP_CONTROL *ctl;
    bump_control(ctl = copy_control(p, FALSE));
    resultTop->PARENT_CONTROL = ctl; /* ref from ctl */
    resultTop = ctl;
    p = p->PARENT_CONTROL;
  }

  resultTop->PARENT_CONTROL = NULL;
  return result;
}


/****************************************************************
 *		        CAT_UP_CONTROL				*
 ****************************************************************
 * Destructively change the NULL at the end of up-control uc	*
 * to point to parnt, and return uc.  If uc is NULL, then       *
 * return parnt.						*
 ****************************************************************/

PRIVATE UP_CONTROL* cat_up_control(UP_CONTROL* uc, UP_CONTROL* parnt)
{
  if(uc == NULL) return parnt;
  else {
    UP_CONTROL* p = uc;
    while(p->PARENT_CONTROL != NULL) p = p->PARENT_CONTROL;
    bump_control(p->PARENT_CONTROL = parnt);
    return uc;
  }
}


/****************************************************************
 *		        PUSH_DOWN_EACHES_AT_TRY			*
 ****************************************************************
 * A try-node is being cancelled at a then of the try.  c is	*
 * the control, and the node to be cancelled has try-eaches 	*
 * above it.  The try-eaches are given by chain eaches.		*
 *								*
 * Return the new control, obtained by moving the try-each	*
 * nodes down below all branch and mix nodes to which they	*
 * apply.  For example, if c is the control at the left below,	*
 * then the result is the modified control shown.  T is the try *
 * node that is being cancelled, and . is the current		*
 * activation.							*
 *								*
 *                         M					*
 *                        / \					*
 *        E              M   E					*
 *        |             / \   \					*
 *        T            E   E   T    <-- cancelled		*
 *        |       =>   |   |   |				*
 *        M            T   T   A    <-- both tries cancelled	*
 *       / \           |   |					*
 *      M*  A          .   B	(see code for meaning of *)	*
 *     / \							*
 *    .   B							*
 *								*
 ****************************************************************/

PRIVATE UP_CONTROL* push_down_eaches_at_try(UP_CONTROL *c, UP_CONTROL *eaches)
{
  /*--------------------------------------------------------------*
   * First build the control above the eaches on the main branch. *
   * This starts at the node M* in the illustration above.	  *
   *--------------------------------------------------------------*/

  UP_CONTROL* ctl;
  UP_CONTROL* main_control = NULL;
  UP_CONTROL* main_control_top = NULL;
  UP_CONTROL* p = c;

  while(p != NULL) {
    int down_kind;
    union ctl_or_act down;

    int p_kind = CTLKIND(p);
    if(p_kind == TRY_F || p_kind == TRYTERM_F) break;

    down = down_eaches(p->right, RCHILD_IS_ACT(p), eaches, &down_kind);
    bump_control(ctl = new_control(CTLKIND(p), p->identity, 
				   NULL_CTL_OR_ACT, CTL_F,
				   down, down_kind));
				   
    if(main_control_top == NULL) main_control = ctl;   /* ref from ctl */
    else main_control_top->PARENT_CONTROL = ctl;       /* ref from ctl */
    main_control_top = ctl;
    p = p->PARENT_CONTROL;
  }

  /*---------------------------------------------------------------*
   * Now p is pointing to the try node that needs to be cancelled. *
   * That node is in chain eaches, and so should not be copied.    *
   * All that is left is to copy MARK_F nodes that are above this  *
   * TRY_F node, until the end of the chain is encountered or a	   *
   * node that is not a MARK_F, TRYEACH_F or TRYEACHTERM_F node    *
   * is encountered.						   *
   *---------------------------------------------------------------*/

  p = p->PARENT_CONTROL;
  while(p != NULL && CTLKIND(p) >= TRYEACH_F) {
    if(CTLKIND(p) == MARK_F) {
      bump_control(ctl = allocate_control());
      ctl->info = MARK_F;
      ctl->identity = p->identity;
      if(main_control_top == NULL) main_control = ctl;   /* ref from ctl */
      else main_control_top->PARENT_CONTROL = ctl;       /* ref from ctl */
      main_control_top = ctl;
    }
    p = p->PARENT_CONTROL;
  }

  /*--------------------------------------------------------------------*
   * The copy is made.  Install the pointer to p in main_control_top,	*
   * and install eaches just below main_control.  If main_control is    *
   * NULL, then just install eaches below p.				*
   *--------------------------------------------------------------------*/

  if(main_control == NULL) {
    return cat_up_control(eaches, p);
  }
  else {
    bump_control(main_control_top->PARENT_CONTROL = p);
    main_control->ref_cnt--;
    return cat_up_control(eaches, main_control);
  }
}


/****************************************************************
 *		        DOWN_EACHES				*
 ****************************************************************
 * Return the result of modifying activation or down-control	*
 * dc so that each thread activation A is replaced by a copy of *
 * chain eaches with A at the bottom.  The eaches are not	*
 * pushed below a try node of any kind.  For example, if control*
 * dc is as follows, then it is modified as indicated.	E is 	*
 * chain eaches, as a down-control.  X, Y, Z and W are		*
 * activations, T is a try node and M is a mix/branch node.	*
 *								*
 *            M                     M				*
 *          /   \                 /   \				*
 *         T     M               E     M			*
 *        /     / \    =>       /     / \			*
 *       M     T   W           T     E   E			*
 *      / \    |               |     |   |			*
 *     X   Y   Z               M     T   W			*
 *                            / \    |  			*
 *                           X   Y   Z				*
 *       							*
 * *res_kind is set to the kind of the result (CTL_F or		*
 * ACT_F).							*
 ****************************************************************/

PRIVATE union ctl_or_act 
down_eaches(union ctl_or_act dc, int dc_kind, UP_CONTROL *eaches, 
	   int* res_kind)
{
  /*------------------------------------------------------------*
   * The result tree is build by traveling down the left links.	*
   * The result tree is result, with resultEnd pointing to the	*
   * bottommost control node that has been built so far.	*
   *------------------------------------------------------------*/

  union ctl_or_act result;
  DOWN_CONTROL*    resultEnd = NULL;
  int              current_kind = dc_kind;
  union ctl_or_act current = dc;
  result.ctl = NULL;

  while(current_kind == CTL_F) {

    /*------------------------------------------------------------------*
     * If we have reached a try node, then stop moving downwards.	*
     * Just exit the loop to do that.					*
     *------------------------------------------------------------------*/

    int current_ctl_kind = CTLKIND(current.ctl);
    if(IS_TRY(current_ctl_kind)) break;

    /*----------------------------------------------------------*
     * If the current node is not a try, then copy it.  For a   *
     * branch or mix, also push the eaches chain down to the	*
     * right.							*
     *----------------------------------------------------------*/

    {DOWN_CONTROL* ctl;
     union ctl_or_act right;
     int right_kind;

     if(current_ctl_kind <= BRANCH_F) {
       right = down_eaches(current.ctl->right, RCHILD_IS_ACT(current.ctl), 
			   eaches, &right_kind);
     }
     else {
	right = current.ctl->right;
        right_kind = RCHILD_IS_ACT(current.ctl);
     }

     bump_control(ctl = new_control(current_ctl_kind, current.ctl->identity, 
		                    NULL_CTL_OR_ACT, 0, right, right_kind));
     if(resultEnd == NULL) result.ctl = ctl; /* ref from ctl */
     else resultEnd->left.ctl = ctl;     /* ref from ctl */
     resultEnd = ctl;

     current_kind = LCHILD_IS_ACT(current.ctl);
     current = current.ctl->left;
    }
  }

  /*---------------------------------------------------------------*
   * On exit from the loop, current is either an activation or a   *
   * try control node.  Install current at the end of the result   *
   * chain, with the eaches chain above it.			   *
   *---------------------------------------------------------------*/

  {int subtree_kind;
   union ctl_or_act subtree;

   subtree = get_down_control_c(eaches, current, current_kind, &subtree_kind);
   if(result.ctl == NULL) {
     *res_kind = subtree_kind;
     return subtree;
   }
   else {
     bump_ctl_or_act(resultEnd->left = subtree, subtree_kind);
     *res_kind = CTL_F;
     result.ctl->ref_cnt--;
     return result;
   }
  }
}


/****************************************************************
 *		        COUNT_THREADS				*
 ****************************************************************
 * Set num_threads to the number of threads in ctl_or_act	*
 * ca, of kind kind.  kind should be CTL_F(0) for a control and *
 * anything else for an activation.				*
 ****************************************************************/

PRIVATE void count_threads_help(int kind, union ctl_or_act ca)
{
  int ctl_kind;

  if(ca.ctl == NULL) return;

  /*-------------------------------------*
   * If this is an activation, count it. *
   *-------------------------------------*/

  if(kind != CTL_F) {
    num_threads++;
    return;
  }

  ctl_kind = CTLKIND(ca.ctl);

  /*---------------------------------------------------------*
   * Always count threads in the left-branch of a control.   *
   *---------------------------------------------------------*/

  count_threads_help(LCHILD_IS_ACT(ca.ctl), ca.ctl->left);

  /*------------------------------------------------------------*
   * Only count threads in the right branch for MIX_F nodes. 	*
   *------------------------------------------------------------*/

  if(ctl_kind == MIX_F) {
    count_threads_help(RCHILD_IS_ACT(ca.ctl), ca.ctl->right);
  }
}

/*-------------------------------------------------------------*/

void count_threads(int kind, union ctl_or_act ca)
{
  num_threads = 0;
  count_threads_help(kind, ca);
}


/****************************************************************
 *		      SET_TERMINATION_MARKS			*
 ****************************************************************
 * Set the mark field of each control node in tree dc to 1	*
 * if there is at least one TRYTERM_F or TRYEACHTERM_F node	*
 * in the subtree rooted at that node, or to 0 otherwise.	*
 ****************************************************************/

PRIVATE void set_termination_marks(DOWN_CONTROL* dc)
{
  Boolean left_mark = 0, right_mark = 0;
  if(CTLKIND(dc) < MARK_F && !RCHILD_IS_ACT(dc)) {
    set_termination_marks(dc->right.ctl);
    right_mark = dc->right.ctl->mark;
  }

  if(!LCHILD_IS_ACT(dc)) {
    set_termination_marks(dc->left.ctl);
    left_mark = dc->left.ctl->mark;
  }
  {int dc_kind = CTLKIND(dc);
   dc->mark = left_mark || right_mark || 
              dc_kind == TRYTERM_F || dc_kind == TRYEACHTERM_F;
  }
}


/****************************************************************
 *		      CLEAR_TERMINATION_MARKS			*
 ****************************************************************
 * Set the mark field in every control node in tree dc to 0.	*
 ****************************************************************/

PRIVATE void clear_termination_marks(DOWN_CONTROL* dc)
{
  while(1) {
    dc->mark = 0;  
    if(CTLKIND(dc) < MARK_F && !RCHILD_IS_ACT(dc)) {
      clear_termination_marks(dc->right.ctl);
    }

    if(LCHILD_IS_ACT(dc)) break;
  
    dc = dc->left.ctl;
  }
}


/****************************************************************
 *		      TERMINATE_CTL_OR_ACT			*
 ****************************************************************
 * Return a control or activation that results from marking	*
 * all thread activations in down-control or activation coa as	*
 * being terminated.  coa_kind is the kind of coa.		*
 *								*
 * *res_kind is set to the kind of the result.			*
 ****************************************************************/

PRIVATE union ctl_or_act 
terminate_ctl_or_act(union ctl_or_act coa, int coa_kind, int *res_kind)
{

  /*------------------------------------------------------*
   * An activation just gets cut off.  It has no nodes to *
   * catch a terminateX.				  *
   *------------------------------------------------------*/

  if(coa_kind != CTL_F) {
     *res_kind = CTL_F;
     return NULL_CTL_OR_ACT;
  }

  /*------------------------------------------------------------*
   * Here, we process a control.				*
   * First mark the nodes so that we can tell whether a subtree *
   * contains a TRYTERM_F or TRYEACHTERM_F node.		*
   * Then extract the new control, then clear the marks.        *
   *------------------------------------------------------------*/

  else {
    union ctl_or_act result;
    set_termination_marks(coa.ctl);
    result = terminate_ctl_or_act_help(coa, CTL_F, res_kind, FALSE);
    clear_termination_marks(coa.ctl);
    return result;
  }
}


/****************************************************************
 *		      TERMINATE_CTL_OR_ACT_HELP			*
 ****************************************************************
 * Return a control or activation that results from marking	*
 * all thread activations in down-control or activation coa as	*
 * being terminated.  coa_kind is the kind of coa.		*
 *								*
 * *res_kind is set to the kind of the result.			*
 *								*
 * This is the almost same as terminate_ctl_or_act, but it has	*
 * the requirement that each control node t in tree coa has	*
 * been	marked so that its mark field holds 1 if there is a	*
 * TRYTERM_F or TRYEACHTERM_F node in the subtree rooted at t,	*
 * or 0 if there is no such node.				*
 *								*
 * An additional parameter is force.  It is true if a copy	*
 * of coa (with activations terminated) should be returned 	*
 * regardless of the presence of TRYTERM_F or TRYEACHTERM_F	*
 * nodes.  If force is false, then NULL should be returned	*
 * for a tree that has no TRYTERM_F or TRYEACHTERM_F nodes in	*
 * it.								*
 ****************************************************************/

PRIVATE union ctl_or_act 
terminate_ctl_or_act_help(union ctl_or_act coa, int coa_kind, 
			  int *res_kind, Boolean force)
{

  if(coa_kind != CTL_F) {

    /*------------------------------------------------------------*
     * Case of an activation.					  *
     *								  *
     * If not forced to copy, just kill the activation by	  *
     * returning NULL.						  *
     *								  *
     * If we are forced to do so, return a copy of this		  *
     * activation, but mark it as terminated.			  *
     *------------------------------------------------------------*/

    if(!force) {
      *res_kind = CTL_F;
      return NULL_CTL_OR_ACT;
    }
    else {
      union ctl_or_act cpy;
      cpy.act = copy_activation(coa.act);
      cpy.act->kind = 2;
      *res_kind = ACT_F;
      return cpy;
    }
  }

  /*------------------------------------------------------------*
   * Case of a control.						*
   *								*
   * If there are no TRYTERM_F or TRYEACHTERM_F nodes in this	*
   * subtree, and we are not forced to make a copy, then 	*
   * return NULL. 						*
   *								*
   * Otherwise, make a copy of this node, with terminated nodes	*
   * beneath it.  Note that the right-child of a try or branch  *
   * node is not terminated -- the right child should only be	*
   * terminated for a MIX_F node,				*
   *								*
   * Note: 							*
   * If we see a try-term or try-each-term node, then the 	*
   * left subtree should be forced.				*
   *------------------------------------------------------------*/

  else {
    if(!force && coa.ctl->mark == 0) {
      *res_kind = CTL_F;
      return NULL_CTL_OR_ACT;
    }
    else {
      union ctl_or_act right, left, result;
      int right_kind, left_kind;
      int coa_ctl_kind = CTLKIND(coa.ctl);
      
      if(coa_ctl_kind == MIX_F) {
        right = terminate_ctl_or_act_help(coa.ctl->right, 
					  RCHILD_IS_ACT(coa.ctl),
				          &right_kind, force);
      }
      else {
        right = coa.ctl->right;
        right_kind = RCHILD_IS_ACT(coa.ctl);
      }

      if(coa_ctl_kind == TRYTERM_F || coa_ctl_kind == TRYEACHTERM_F) {
        force = TRUE;
      }

      left = terminate_ctl_or_act_help(coa.ctl->left, LCHILD_IS_ACT(coa.ctl),
				       &left_kind, force);
      
      result.ctl = new_control(coa_ctl_kind, coa.ctl->identity, left, 
			       left_kind, right, right_kind);
      *res_kind = CTL_F;
      return result;
    }
  }
}


/****************************************************************
 *			CHOP_C					*
 ****************************************************************
 * This function is used to perform a cut.  It must terminate	*
 * other threads that were created since the most recent entry	*
 * into a CutHere construct.					*
 *								*
 * Scan up-control c until a MARK_F node whose identity is id   *
 * is encountered, or the end of the chain is reached.  Build	*
 * and return the result of performing the following		*
 * modifications along the scan path.				*
 *								*
 *   Delete BRANCH_F nodes.  (The deferred thread has not	*
 *   done anything, and is just removed.)			*
 *								*
 *   Replace the child of a MIX_F node by the result of 	*
 *   terminating the threads in that control.  Terminating	*
 *   a thread means forcing it to fail with exception 		*
 *   terminateX.						*
 ****************************************************************/

UP_CONTROL* chop_c(UP_CONTROL *c, LONG id)
{
  UP_CONTROL *p, *result, *build_point;

  /*-------------------------------------------------------*
   * We build a new control upwards towards the mark node. *
   * result points to the bottom node of the new chain,   *
   * and build_point points to the top node.  Initially,   *
   * both are NULL, indicating that no nodes have been	   *
   * built.						   *
   *-------------------------------------------------------*/

# ifdef DEBUG
    if(trace_control > 1) {
      trace_i(330, id);
      trace_i(85);
      print_up_control(TRACE_FILE, c, 1);
    }
# endif

  result = build_point = NULL;
  p = c;
  while(p != NULL) {
    int p_kind = CTLKIND(p);
    switch(p_kind) {

      case MIX_F:

	/*--------------------------------------------------------------*
	 * Get the activation or control that results from doing	*
	 * a terminate on all of the threads whose controls are in	*
	 * p->right.  If there are none, then just skip this MIX_F	*
	 * node.  If there are any, then install a new MIX_F node	*
	 * with that activation or control as child.			*
	 *--------------------------------------------------------------*/

	{union ctl_or_act right;
	 int right_kind;
	 UP_CONTROL *node;

	 right = terminate_ctl_or_act(p->right, RCHILD_IS_ACT(p), 
				      &right_kind);
	 node = allocate_control();
	 node->info = MIX_F;
         if(right_kind != CTL_F) node->info |= RCHILD_CTLMASK;
	 node->identity = 0;
	 bump_ctl_or_act(node->right = right, right_kind);
	 if(build_point != NULL) {
	   bmp_control(build_point->PARENT_CONTROL = node);
	 }
	 else result = node;
	 build_point = node;
	 break;
        }

      case BRANCH_F:
	break;

      case MARK_F:
        if(p->identity == id) goto loopend;
        /* No break -- install node */

      case TRY_F:
      case TRYEACH_F:
      case TRYTERM_F:
      case TRYEACHTERM_F:
	{register UP_CONTROL* node = allocate_control();
	 node->info     = p->info;
	 node->identity = p->identity;
	 bump_ctl_or_act(node->right = p->right, RCHILD_IS_ACT(p));
	 if(build_point != NULL) {
	   bmp_control(build_point->PARENT_CONTROL = node);
	 }
	 else result = node;
         build_point = node;
	}
    }
    p = p->PARENT_CONTROL;

  } /* end while(p != NULL) */

 loopend:

  /*-----------------------------------------------*
   * Attach the mark node to the top of the chain. *
   *-----------------------------------------------*/

  if(build_point != NULL) bump_control(build_point->PARENT_CONTROL = p);
  else result = p;

# ifdef DEBUG
    if(trace_control > 1) {
      trace_i(331);
      print_up_control(TRACE_FILE, result, 1);
    }
# endif

  return result;
}


/****************************************************************
 *			NEW_CONTROL				*
 ****************************************************************
 * Return a new control node with 				*
 *								*
 *   kind 	  kind						*
 *   identity 	  id						*
 *   left child   left  (of kind left_kind)			*
 *   right child  right (of kind right_kind)			*
 *								*
 * The reference count of the new node is 0.			*
 ****************************************************************/

PRIVATE CONTROL* new_control(int kind, LONG id, 
			    union ctl_or_act left, int left_kind,
			    union ctl_or_act right, int right_kind)
{
  CONTROL* ctl = allocate_control();
  UBYTE info;
  info = kind;
  if(left_kind) info |= LCHILD_CTLMASK;
  if(right_kind) info |= RCHILD_CTLMASK;
  ctl->info = info;
  ctl->identity = id;
  bump_ctl_or_act(ctl->left = left, left_kind);
  bump_ctl_or_act(ctl->right = right, right_kind);
  return ctl;
}


/****************************************************************
 *			COPY_CONTROL				*
 ****************************************************************
 * Return a copy of control node c.  The copy has ref count 0.	*
 *								*
 * The nodes pointed to by node c are not copied.  The pointers *
 * themselves are kept the same.				*
 *								*
 * Reference counts are bumped, except that the reference count *
 * of the left (or parent) link is only bumped if bumpleft is   *
 * true.							*
 ****************************************************************/

CONTROL* copy_control(CONTROL *c, Boolean bumpleft)
{
  register CONTROL* cpy = allocate_control(); 
  *cpy = *c;
  cpy->ref_cnt = 0;
  if(CTLKIND(c) < MARK_F) bump_ctl_or_act(cpy->right, RCHILD_IS_ACT(c));
  if(bumpleft) bump_ctl_or_act(cpy->left, LCHILD_IS_ACT(c));
  return cpy;
}


/****************************************************************
 *			THERE_ARE_OTHER_THREADS			*
 ****************************************************************
 * Return TRUE if up-control c has any mix nodes  		*
 * that are active.  (To be active, they must be on the upward	*
 * path from c to the root.)					*
 ****************************************************************/

Boolean there_are_other_threads(UP_CONTROL *c)
{
  UP_CONTROL *p;

  for(p = c; p != NULL; p = p->PARENT_CONTROL) {
    if(CTLKIND(p) == MIX_F) return TRUE;
  }
  return FALSE;
}


/****************************************************************
 *			CTLKINDF				*
 ****************************************************************
 * Return c->kind, but check for negative ref count.  (A 	*
 * negative ref count indicates that something has gone wrong	*
 * in memory management. Only used in GCTEST mode.		*
 ****************************************************************/

#ifdef GCTEST

int ctlkindf(CONTROL *c)
{
  if(c->ref_cnt < 0) badrc("control", toint(c->ref_cnt), (char *) c);
  return c->info & KIND_CTLMASK;
}

#endif


#ifdef DEBUG
/********************************************************
 *			PRINT_CTL_ACTIVATION		*
 ********************************************************
 * Print activation aa on file f in debug form, for	*
 * inclusion in a control.				*
 * Indent n spaces, then precede the activation by	*
 * pref.	 					*
 ********************************************************/

PRIVATE void print_ctl_activation(FILE *f, ACTIVATION *aa, char* pref, int n)
{

  indent(n);
  fprintf(f, "%s", pref);
  if(aa != NULL) {
    fprintf(f, "<%s(%p@%p:%d:%d)/rc=%d", aa->name, aa, aa->program_ctr,
	    toint(aa->progress), toint(aa->kind), toint(aa->ref_cnt));
    if(aa->env!= NULL) {
      fprintf(f, "/envrc=%d", toint(aa->env->ref_cnt));
    }
    if(aa->coroutines != NIL) {
      fprintf(f,"(cr=%p)", aa->coroutines->head.list);
    }
    if(aa->embedding_tries != NULL) {
      fprintf(f, "/et=");
      print_str_list(aa->embedding_tries);
    }
    if(aa->embedding_marks != NULL) {
      fprintf(f, "/em=");
      print_str_list(aa->embedding_marks);
    }
    fprintf(f,">\n");

#   ifdef GCTEST
      if(aa->ref_cnt < 0) {
	badrc("activation", toint(aa->ref_cnt), (char *)aa);
      }
#   endif
  }

  else fprintf(f, "<NULL>\n");
}


/********************************************************
 *			PRINT_UP_CONTROL		*
 ********************************************************
 * Print up-control c on file f in debug form.		*
 * Indent n spaces. 					*
 ********************************************************/

void print_up_control(FILE *f, UP_CONTROL *c, int n)
{
  if(c == NULL) {
    indent(n);
    fprintf(f, "NULL\n"); 
    return;
  }

  while(c != NULL) {
    int info = c->info;
    int kind = CTLKIND(c);

    indent(n);
    if(TRY_F <= kind && kind <= TRYEACHTERM_F && c->right.ctl == NULL) {
      fprintf(f, "up:%s(%p:%ld):DEAD\n", ctl_kind_name[kind], c, c->identity);
    }
    else {
      fprintf(f, "up:%s(%p:%ld)\n", ctl_kind_name[kind], c, c->identity);
    }

    /*-----------------------*
     * Print the child info. *
     *-----------------------*/

    if(kind < MARK_F) {

      /*-----------------------------------------------------------*
       * If the uplink is from the left, then the downlink must be *
       * to the right.  If the uplink is from the right, then the  *
       * downlink must be to the left.				   *
       *-----------------------------------------------------------*/

      char *child_pref;
      if((info & UPLINK_CTLMASK) == LEFT_F) child_pref = "r:";
      else child_pref = "l:";

      /*------------------*
       * Print the child. *
       *------------------*/

      if((info & RCHILD_CTLMASK) == CTL_F) {
	print_down_control(TRACE_FILE, c->right.ctl, child_pref, n+1);
      }
      else {
        print_ctl_activation(f, c->right.act, "", n+1);
      }
    }

    /*----------------------*
     * Go up to the parent. *
     *----------------------*/

    if((kind & LCHILD_CTLMASK) != CTL_F) fprintf(f, "!!!");
    c = c->PARENT_CONTROL;
  }
}

  
/********************************************************
 *			PRINT_DOWN_CONTROL		*
 ********************************************************
 * Print down-control c on file f in debug form.	*
 * Indent n spaces, and print pref at the front of the	*
 * first line.						*
 ********************************************************/
 
void print_down_control(FILE *f, DOWN_CONTROL *c, char *pref, int n)
{
  int info, kind;

  if(c == NULL) {
    indent(n);
    fprintf(f, "%sNULL\n", pref); 
    return;
  }

  info = c->info;
  kind = info & KIND_CTLMASK;

  /*-----------------*
   * Print the kind. *
   *-----------------*/

  indent(n);
  if(TRY_F <= kind && kind <= TRYEACHTERM_F && c->right.ctl == NULL) {
    fprintf(f, "%s%s(%p:%ld):DEAD\n", 
	    pref, ctl_kind_name[kind], c, c->identity);
  }
  fprintf(f, "%s%s(%p:%ld)\n", pref, ctl_kind_name[kind], c, c->identity);

  /*-----------------------------*
   * Print the right child info. *
   *-----------------------------*/

  if(kind < MARK_F) {
    if((info & RCHILD_CTLMASK) == CTL_F) {
      print_down_control(f, c->right.ctl, "r:", n+1);
    }
    else {
      print_ctl_activation(f, c->right.act, "r:", n+1);
    }
  }

  /*----------------------------*
   * Print the left child info. *
   *----------------------------*/

  if((info & LCHILD_CTLMASK) == CTL_F) {
    print_down_control(f, c->left.ctl, "l:", n+1);
  }
  else {
    print_ctl_activation(f, c->left.act, "l:", n+1);
  }

}

#endif
