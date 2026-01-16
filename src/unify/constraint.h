/**************************************************************
 * File:    unify/constraint.h
 * Purpose: Handle constraints
 * Author:  Karl Abrahamson
 **************************************************************/

#define CONSTRAIN(ub,lb,record,gg) constrain(&(ub),&(lb),record,gg)

extern Boolean should_check_primary_constraints;

TYPE_LIST* lower_bound_list	(TYPE *V);
Boolean constrain		(TYPE **ubp, TYPE **lbp, Boolean record, 
				 Boolean gg);
Boolean bind_lower_bounds_u	(TYPE *a, TYPE_LIST *lwb, Boolean record);
Boolean remove_redundant_constraints(TYPE *t, Boolean record);
Boolean unify_cycles		(TYPE *t, Boolean record);

#ifdef TRANSLATOR
void	   reduce_constraints 	(TYPE *t, Boolean record);
void       add_lower_bound	(TYPE *ub, TYPE *lb, Boolean gg, int line);
void 	   lift_all_lower_bounds(TYPE_LIST *L);
TYPE_LIST* maximal_lower_bounds (TYPE_LIST *L);
#endif

