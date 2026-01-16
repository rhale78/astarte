/**********************************************************************
 *    File:    error/newrole.c
 *    Purpose: Role handling.
 *    Author:  Karl Abrahamson
 **********************************************************************/

#include <string.h>
#include <memory.h>
#include "../misc/misc.h"
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../classes/classes.h"
#include "../tables/tables.h"
#include "../exprs/expr.h"
#include "../error/error.h"
#include "../unify/unify.h"
#include "../generate/generate.h"
#include "../evaluate/instruc.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif

Boolean role_error_occurred = FALSE;
Boolean ignore_trapped_exceptions = TRUE;
Boolean forever_ignore_trapped_exceptions = TRUE;
ROLE *colon_equal_role; /* Role -(destination,source) */
char *pot_produce_stream, *act_produce_stream;
char *pot_fail, *act_fail;
char *pot_side_effect, *act_side_effect;
char *is_pure_role, *is_lazy_role, *is_lazy_list_role;
char *select_left_role, *select_right_role;
char *kill_act_produce_stream;

/* trapped_exceptions is a list indicating which exceptions have
 * been explicitly trapped or untrapped.  An untrapped exception
 * has tag STR_L, and a trapped exception has tag STR1_L.
 */

STR_LIST *trapped_exceptions = NULL;


/****************************************************************
 *				INIT_ROLES			*
 ****************************************************************/

void init_roles(void)
{
  bump_role(colon_equal_role =
    pair_role(basic_role(stat_id_tb("destination")),
	      basic_role(stat_id_tb("source"))));
}


/************************************************************************
 * 			INIT_PROPERTIES					*
 ************************************************************************
 * Initialize property variables.
 */

void init_properties(void)
{
  pot_produce_stream     = stat_id_tb("pot__produceStream");
  act_produce_stream	 = stat_id_tb("act__produceStream");
  kill_act_produce_stream = stat_id_tb("kill__act__produceStream");
  pot_fail		 = stat_id_tb("pot__fail");
  act_fail		 = stat_id_tb("act__fail");
  pot_side_effect	 = stat_id_tb("pot__sideEffect");
  act_side_effect	 = stat_id_tb("act__sideEffect");
  is_pure_role		 = stat_id_tb("is__pure");
  is_lazy_role		 = stat_id_tb("is__lazy");
  is_lazy_list_role	 = stat_id_tb("is__lazyList");
  select_left_role	 = stat_id_tb("select__left");
  select_right_role	 = stat_id_tb("select__right");

  add_role_completion_tm(pot_side_effect, "no__is__pure");
  add_role_completion_tm(is_pure_role, "no__pot__sideEffect");
  add_role_completion_tm(is_lazy_role, "no__act__produceStream");
  add_role_completion_tm(is_lazy_role, "no__act__fail");
  add_role_completion_tm("suceeds__", "kill__act__fail");
  add_role_completion_tm("safe__", "kill_pot_fail");
}


/****************************************************************
 *				RKINDF				*
 ****************************************************************
 * Return the kind of role r.  Only used in allocator test mode.
 */

#ifdef GCTEST
int rkindf(ROLE *r)
{
  if(r->ref_cnt < 0) die(6,(char *)(long) r->ref_cnt);
  return r->kind;
}
#endif


/****************************************************************
 *			SUBROLE		 			*
 ****************************************************************
 * Return true if r1 is a subrole of r2.  That is, all of the	*
 * role information of r1 is contained in r2.			*
 ****************************************************************/

Boolean subrole(ROLE *r1, ROLE *r2)
{
  if(r1 == NULL) return TRUE;
  if(r2 == NULL) return FALSE;
  if(RKIND(r1) != RKIND(r2)) return FALSE;
  if(!str_list_subset(r1->namelist, r2->namelist)) return FALSE;
  return subrole(r1->role1, r2->role1) && subrole(r1->role2, r2->role2);
}


/****************************************************************
 *			ROLE_EQUAL	 			*
 ****************************************************************
 * Return true if r1 and r2 are identical roles.		*
 ****************************************************************/

Boolean role_equal(ROLE *r1, ROLE *r2)
{
  if(r1 == NULL) return r2 == NULL;
  if(r2 == NULL) return FALSE;
  if(RKIND(r1) != RKIND(r2)) return FALSE;
  if(!str_list_equal_sets(r1->namelist, r2->namelist)) return FALSE;
  return role_equal(r1->role1, r2->role1) && role_equal(r1->role2, r2->role2);
}


/****************************************************************
 *			CHECK_FOR_UNDECLARED_ROLE		*
 ****************************************************************
 * Check for a role that occurs in this_namelist, but not in    *
 * declared_namelist.  If one is found, report it for id	*
 * spec_name, of type ty, at line line.				*
 *								*
 * XREF: This function is called below to check that a 		*
 * defined symbol does not refer to a role that has no meaning, *
 ****************************************************************/

PRIVATE void 
check_for_undeclared_role
     (STR_LIST *this_namelist, STR_LIST *declared_namelist,
      char *spec_name, TYPE *ty, int line)
{
  STR_LIST *p;

  for(p = this_namelist; p != NIL; p = p->tail) {
    char* name_with_tag = p->head.str;
    char* role_name = name_with_tag + 1;
    int mode = *name_with_tag;
    if(is_potential(role_name) && is_present(mode)) {
      if(!is_suppressed_property(role_name) &&
	 !rolelist_member(role_name, declared_namelist, 0)) {
	def_prop_warn(spec_name, role_name, ty, line);
      }
    }
  }
}


/****************************************************************
 *			DO_DEF_DCL_ROLE_CHECK			*
 ****************************************************************
 * Check the roles on a define or let dcl sse, which defines	*
 * identifier id for each of the types in list types. spec_name *
 * is the print name of id, and line is the line for error	*
 * messages.							*
 ****************************************************************/

void do_def_dcl_role_check(EXPR *sse, EXPR *id, char *spec_name,
			   TYPE_LIST *types, int line)
{
  /* Install is__lazy role on body. */

  {ROLE *lazy_role;
   bump_role(lazy_role = basic_role(is_lazy_role));
   SET_ROLE(lazy_role, complete_role(lazy_role, 0));
   SET_ROLE(sse->E2->role, checked_meld_roles(sse->role, lazy_role, NULL));
   drop_role(lazy_role);
  }

  /* Delete kill__, check__ and no__ roles from the identifier's role.  *
   * We only do the root, because other parts were done in		*
   * issue_define_p. 							*/

  SET_ROLE(id->role, remove_knc(id->role, 0));

  /* Check for undeclared roles, and check the body for actually *
   * producing a stream or actually failing. 			 */

  {ROLE* this_dcl_role = id->role;
   bump_role(this_dcl_role);
   if(types != NIL
      && this_dcl_role != NULL
      && this_dcl_role->namelist != NULL
      && !should_suppress_warning(err_flags.suppress_property_warnings)) {

     STR_LIST* this_namelist = this_dcl_role->namelist;
     TYPE_LIST *ts;

     for(ts = types; ts != NIL; ts = ts->tail) {
       LIST* binding_list_mark;
       bump_list(binding_list_mark = finger_new_binding_list());
       if(unify_u(&(ts->head.type), &(id->ty), TRUE)) {
         ROLE* declared_role = get_role_tm(id, NULL);
         STR_LIST* declared_namelist =
	   (declared_role == NULL) ? NULL : declared_role->namelist;

#        ifdef DEBUG
	   if(trace_role) {
	     trace_t(77, id->STR);
	     trace_ty(ts->head.type);
	     trace_t(78);
	     trace_rol(declared_role);
	     trace_t(80);
	     trace_rol(this_dcl_role);
	     tracenl();
	   }
#        endif

         /* Check that each potential role in the root of l is declared. */

         check_for_undeclared_role(this_namelist, declared_namelist,
				   spec_name, ts->head.type, line);

         /* Check whether the body can produce a stream. */

	 if(!is_suppressed_property(act_produce_stream) &&
	    rolelist_member(act_produce_stream, this_namelist, 0)) {
	   warn1(DCL_PRODUCE_STREAM_ERR, spec_name, line);
	 }

	 /* Check whether the body can fail.  Try all   	*
	  * act__fail__ex roles. 				*/

	 if(!is_suppressed_property(act_fail) && has_act_fail(this_namelist)) {
	   warn1(DCL_FAIL_ERR, spec_name, line);
	 }

	 undo_bindings_u(binding_list_mark);

       } /* end if(unify...) */

     } /* end for(ts = ...) */

   } /* end if(types != NIL...) */
  } /* end block */
}


/****************************************************************
 *			ATTACH_PROPERTY	 			*
 ****************************************************************
 * Indicate that identifier name of type ty has role named r.
 *
 * ### This looks highly suspect.
 */

void attach_property(char *r, char *name, TYPE *ty)
{
  ROLE_CHAIN *rc;
  GLOBAL_ID_CELL *gic = get_gic_tm(name, TRUE);

  if(gic == NULL) return;
  for(rc = gic->role_chain; rc != NULL; rc = rc->next) {
    if(!disjoint(rc->type, ty)) {
      ROLE *basic, *rol;
#     ifdef DEBUG
	if(trace_role) {
	  trace_t(404, r, name);
	  trace_ty(ty);
	  trace_t(78);
	  trace_rol(rc->role);
	  tracenl();
	}
#     endif
      bump_role(basic = basic_role(r));
      rol = checked_meld_roles(rc->role, basic, NULL);
      SET_ROLE(rc->role, rol);
      drop_role(basic);
    }
  }
}


/****************************************************************
 *			NEW_ROLE	 			*
 ****************************************************************/

ROLE *new_role(int kind, ROLE *r1, ROLE *r2)
{
  ROLE *newr = allocate_role();

  newr->kind = kind;
  bump_role(newr->role1 = r1);
  bump_role(newr->role2 = r2);
  newr->namelist = NIL;
  newr->mode = NULL;
  return newr;
}


/****************************************************************
 *			BASIC_ROLE	 			*
 ****************************************************************
 * Return a role with name and no children.
 */

ROLE *basic_role(char *name)
{
  char *mod_name;
  ROLE *result = allocate_role();
  memset(result, 0, sizeof(ROLE));
  result->kind = BASIC_ROLE_KIND;
  mod_name = make_tagged_string(ROLE_ROLE_MODE, name);
  bump_list(result->namelist = str_cons(mod_name, NIL));
  return result;
}


/****************************************************************
 *			PAIR_ROLE	 			*
 ****************************************************************
 * Return a pair role with name name and children r1 and r2.
 */

ROLE *pair_role(ROLE *r1, ROLE *r2)
{
  if(r1 == NULL && r2 == NULL) return NULL;
  return new_role(PAIR_ROLE_KIND, r1, r2);
}


/****************************************************************
 *			FUN_ROLE	 			*
 ****************************************************************
 * Return a function role with children r1 and r2.
 */

ROLE *fun_role(ROLE *r1, ROLE *r2)
{
  if(r1 == NULL && r2 == NULL) return NULL;
  return new_role(FUN_ROLE_KIND, r1, r2);
}


/****************************************************************
 *			BUILD_ROLE	 			*
 ****************************************************************
 * Return a role with given kind, root namelist, role1 and role2
 * values.  If there are properties in namelist, and if property
 * checking is turned off, delete properties from namelist
 */
 
PRIVATE STR_LIST *remove_properties(STR_LIST *l);

PRIVATE ROLE *build_role(int kind, STR_LIST *namelist, ROLE *r1, ROLE *r2)
{
  ROLE *result;

  bump_list(namelist);
  if(should_suppress_warning(err_flags.suppress_property_warnings)) {
    SET_LIST(namelist, remove_properties(namelist));
  }

  if(r1 == NULL && r2 == NULL) {
    if(namelist == NULL) return NULL;
    else kind = BASIC_ROLE_KIND;
  }
  result = new_role(kind, r1, r2);
  result->namelist = namelist;  /* inherits ref from namelist */
  return result;
}


/****************************************************************
 *			MAKE_CHECK_ROLE				*
 ****************************************************************
 * Return string no__r.
 */

PRIVATE char *make_check_role(char *r)
{
  return concat_id("check__", r);
}


/****************************************************************
 *			MAKE_NO_ROLE				*
 ****************************************************************
 * Return string no__r.
 */

PRIVATE char *make_no_role(char *r)
{
  return concat_id("no__", r);
}


/************************************************************************
 * 			GET_EXCEPTION_NAME				*
 ************************************************************************
 * Return the name of the exception represented by expression e, or
 * NULL can't tell.
 */

PRIVATE char *get_exception_name(EXPR *e)
{
  EXPR_TAG_TYPE kind;

  if(e == NULL) return NULL;
  e = skip_sames(e);
  kind = EKIND(e);
  if(kind == APPLY_E) {
    e = skip_sames(e->E1);
    kind = EKIND(e);
  }
  if(kind != GLOBAL_ID_E) return NULL;
  return e->STR;
}


/************************************************************************
 * 			USUALLY_TRAPPED_NAME				*
 ************************************************************************
 * Return 0 if expression exc_name is an exception that is usually
 * not trapped, 1 if exc_name is usually trapped, and 2 if it is not
 * possible to say.
 *
 * List trapped_exceptions indicates
 * exceptions that have been explicitly trapped or untrapped, with
 * priority to values closer to the front.  A tag of STR_L indicates
 * an exception that is untrapped, and a tag of STR1_L indicates
 * an exception that is trapped.
 */

PRIVATE int usually_trapped_name(char *exc_name)
{
  STR_LIST *p;

  if(exc_name == NULL) return 2;
  for(p = trapped_exceptions; p != NIL; p = p->tail) {
    if(strcmp(p->head.str, exc_name) == 0){
      if(LKIND(p) == STR_L) return 0;
      else return 1;
    }
  }
  return usually_trapped_tm(exc_name);
}


/****************************************************************
 *			CONVERT_ACT_POT				*
 ****************************************************************
 * s is the name of a potential or actual role, with a leading
 * mode byte.  Return the name of a corresponding role, with the
 * same mode byte, according to mode.
 *   mode	new string prefix
 *     0            act__
 *     1            pot__
 */

PRIVATE char *convert_act_pot(char *s, int mode)
{
  char str[MAX_ID_SIZE + 2];
  str[0] = s[0];
  strcpy(str+1, mode ? "pot__" : "act__");
  strcpy(str+6, s + 6);
  return id_tb0(str);
}


/****************************************************************
 *			HAS_ACT_FAIL				*
 ****************************************************************
 * Return TRUE just when l contains an act__fail... role for
 * an exception that is not known to be trapped.
 */

Boolean has_act_fail(STR_LIST *l)
{
  STR_LIST *p;
  for(p = l; p != NIL; p = p->tail) {
    char *p_name = p->head.str + 1;
    if(prefix(act_fail, p_name) &&
       is_present(*(p->head.str))
       && usually_trapped_name(p_name) != 1) {
      return TRUE;
    }
  }
  return FALSE;
}


/****************************************************************
 *			IS_PRESENT				*
 ****************************************************************
 * Return true if mode indicates a present name.
 */

Boolean is_present(int mode)
{
  return (mode & ROLE_ROLE_MODE) && !(mode & KILL_ROLE_MODE);
}


/****************************************************************
 *			IS_SINGLETON_ROLE			*
 ****************************************************************
 * Return TRUE if name is the name of a singleton role.
 */

Boolean is_singleton_role(char *name)
{
  char *p = strchr(name, '_');
  if(p != NULL && p[1] == '_') return FALSE;
  return TRUE;
}

/****************************************************************
 *			IS_POTENTIAL				*
 ****************************************************************
 * Return true if s is a potential role (starting with "pot__").
 */

Boolean is_potential(char *s)
{
  return prefix("pot__", s);
}


/****************************************************************
 *			IS_ACTUAL				*
 ****************************************************************
 * Return true if s is a potential role (starting with "act__").
 */

PRIVATE Boolean is_actual(char *s)
{
  return prefix("act__", s);
}


/****************************************************************
 *			GET_BASE	 			*
 ****************************************************************
 * If name is not of the form no__s, check__s or kill__s, set
 * base_name to r and base_mode to mode.  Otherwise, set base_name to
 * s and base_mode to a mode that corresponds to the prefix.
 */

PRIVATE void get_base(char *name, int mode, char **base_name, int *base_mode)
{
  if(prefix("kill__", name)) {
    *base_mode = (0x40 | KILL_ROLE_MODE);
    *base_name = name + 6;
  }
  else if(prefix("check__", name)) {
    *base_mode = (0x40 | CHECK_ROLE_MODE);
    *base_name = name + 7;
  }
  else if(prefix("no__", name)) {
    *base_mode = (0x40 | NO_ROLE_MODE);
    *base_name = name + 4;
  }
  else {
    *base_name = name;
    *base_mode = mode;
  }
}


/****************************************************************
 *			MAKE_TAGGED_STRING 			*
 ****************************************************************
 * Return name with an initial byte indicating mode mode.
 */

char *make_tagged_string(int mode, char *name)
{
  char s[MAX_ID_SIZE + 2];
  char *base_name;
  int base_mode;

  if((mode & 0xf) == ROLE_ROLE_MODE) {
    get_base(name, mode, &base_name, &base_mode);
  }
  else {
    base_name = name;
    base_mode = mode;
  }

  s[0] = base_mode | 0x40;
  strcpy(s+1, base_name);
  return id_tb0(s);
}


/****************************************************************
 *		        REMOVE_FAILS				*
 ****************************************************************
 * Remove_fails removes act___fail... roles from the root of r,
 * and returns the result.  Mode controls which are removed.
 *
 *   mode      remove
 *     1       all roles that begin act__fail...
 *     2       only role act___fail.
 */

PRIVATE STR_LIST *remove_fails_from_namelist(STR_LIST *l, int mode)
{
  char *this_name;
  STR_LIST *rest;

  if(l == NULL) return NULL;
  rest = remove_fails_from_namelist(l->tail, mode);
  this_name = l->head.str + 1;
  if(prefix(act_fail, this_name) &&
     (mode == 1 || strlen(this_name) == 9)) return rest;
  else if(rest == l->tail) return l->tail;
  else return str_cons(l->head.str, rest);
}

PRIVATE ROLE *remove_fails(ROLE *r, int mode)
{
  STR_LIST *new_root_list;

  if(r == NULL) return NULL;
  new_root_list = remove_fails_from_namelist(r->namelist, mode);
  if(new_root_list == r->namelist) return r;
  else return build_role(r->kind, new_root_list, r->role1, r->role2);
}


/****************************************************************
 *		        REMOVE_PROPERTIES			*
 ****************************************************************
 * Remove_properties removes all roles of the form *__* from
 * the root of r, and returns the result.
 */

PRIVATE STR_LIST *remove_properties(STR_LIST *l)
{
  char *this_name;
  STR_LIST *rest;

  if(l == NULL) return NULL;
  rest = remove_properties(l->tail);
  this_name = l->head.str + 1;
  if(!is_singleton_role(this_name)) return rest;
  else if(rest == l->tail) return l;
  else return str_cons(l->head.str, rest);
}


/****************************************************************
 *		        REMOVE_KNC				*
 ****************************************************************
 * Return the result of removing all kill__, no__ and check__ roles
 * from the root or from every node, controlled by sel as follows.
 *      sel	what to do
 *       0        remove kill__, no__, check__ from  root only
 *       1        remove kill__, no__, check__ from all nodes
 *       2        remove kill__, no__ from root only
 *       3        remove kill__, no__ from all nodes
 */

PRIVATE STR_LIST *remove_knc_namelist(STR_LIST *l, int sel)
{
  if(l == NULL) return NULL;
  else {
    int nc_mask = (sel & 2) ? NO_ROLE_MODE : NO_ROLE_MODE | CHECK_ROLE_MODE;
    char *name_with_tag = l->head.str;
    char *name = name_with_tag + 1;
    int mode = *name_with_tag;
    STR_LIST *tail_list = remove_knc_namelist(l->tail, sel);
    if(mode & KILL_ROLE_MODE) {
      return tail_list;
    }
    if(mode & nc_mask) {
      if(mode & ROLE_ROLE_MODE) {
	return str_cons(make_tagged_string(ROLE_ROLE_MODE, name), tail_list);
      }
      else return tail_list;
    }
    else {
      if(tail_list == l->tail) return l;
      else return str_cons(name_with_tag, tail_list);
    }
  }
}

ROLE *remove_knc(ROLE *r, int mode)
{
  ROLE *new_role1, *new_role2;
  STR_LIST *new_namelist;
  if(r == NULL) return NULL;
  new_namelist = remove_knc_namelist(r->namelist, mode);
  if(mode & 1) {
    new_role1 = remove_knc(r->role1, mode);
    new_role2 = remove_knc(r->role2, mode);
  }
  else {
    new_role1 = r->role1;
    new_role2 = r->role2;
  }
  if(new_namelist == r->namelist && new_role1 == r->role1
     && new_role2 == r->role2) return r;
  else return build_role(r->kind, new_namelist, new_role1, new_role2);
}


/****************************************************************
 *			MAKE_POTS_FROM_ACTS 			*
 ****************************************************************
 * Return a basic role whose namelist consists of a potential role
 * for each actual role in the namelist of r.  Exception: if r
 * is null, return null.
 */

PRIVATE STR_LIST *get_acts_pots_namelist(STR_LIST *l, int mode);

PRIVATE STR_LIST *make_pots_from_acts_namelist(STR_LIST *l)
{
  if(l == NULL) return NULL;
  else return str_cons(convert_act_pot(l->head.str, 1),
		       make_pots_from_acts_namelist(l->tail));
}

PRIVATE ROLE *make_pots_from_acts(ROLE *r)
{
  STR_LIST *l;

  if(r == NULL) return NULL;
  bump_list(l = get_acts_pots_namelist(r->namelist, 2));
  return build_role(BASIC_ROLE_KIND, make_pots_from_acts_namelist(l),
		    NULL, NULL);
}


/****************************************************************
 *			KEEP_ONLY_ACTUALS   			*
 ****************************************************************
 * Remove all roles from the root of r that are not of the form
 * act__r, and return the resulting role.
 */

PRIVATE STR_LIST *keep_only_actuals_namelist(STR_LIST *l)
{
  STR_LIST *rest;

  if(l == NULL) return NULL;
  rest = keep_only_actuals_namelist(l->tail);
  {char *name_with_tag = l->head.str;
   if(prefix("act__", name_with_tag + 1)) {
     if(rest == l->tail) return l;
     else return str_cons(name_with_tag, rest);
   }
   else return rest;
  }
}

PRIVATE ROLE *keep_only_actuals(ROLE *r)
{
  if(r == NULL) return NULL;
  {STR_LIST *new_namelist = keep_only_actuals_namelist(r->namelist);
   if(new_namelist == r->namelist) return r;
   else return build_role(r->kind, new_namelist, r->role1, r->role2);
  }
}


/****************************************************************
 *			REMOVE_ACTUALS	 			*
 ****************************************************************
 * Return the role resulting by deleting all roles of the
 * form act__r from all nodes of r.
 */

PRIVATE STR_LIST *remove_actuals_from_namelist(STR_LIST *l)
{
  if(l == NULL) return NULL;
  else {
    char *name_with_tag = l->head.str;
    STR_LIST *tail_list = remove_actuals_from_namelist(l->tail);
    if(is_actual(name_with_tag + 1)) {
      return tail_list;
    }
    else {
      if(tail_list == l->tail) return l;
      else return str_cons(name_with_tag, tail_list);
    }
  }
}


PRIVATE ROLE *remove_actuals(ROLE *r)
{
  ROLE *r1, *r2;
  STR_LIST *new_namelist;
  
  if(r == NULL) return NULL;
  new_namelist = remove_actuals_from_namelist(r->namelist);

  r1 = remove_actuals(r->role1);
  r2 = remove_actuals(r->role2);

  if(new_namelist == r->namelist && r1 == r->role1 && r2 == r->role2) {
    return r;
  }
  else return build_role(r->kind, new_namelist, r1, r2);
}


/****************************************************************
 *	      GET_SINGLETON, GET_SINGLETON_FROM_NAMELIST	*
 ****************************************************************
 * Return the singleton string in list l, if there is any, or
 * return NULL if there is none.
 */

PRIVATE char *get_singleton_from_namelist(STR_LIST *l)
{
  STR_LIST *p;
  for(p = l; p != NIL; p = p->tail) {
    char *name_with_tag = p->head.str;
    if(is_singleton_role(name_with_tag + 1) &&
       is_present(*name_with_tag)) {
      return name_with_tag + 1;
    }
  }
  return NULL;
}

char *get_singleton(ROLE *r)
{
  if(r == NULL || r->namelist == NULL) return NULL;
  return get_singleton_from_namelist(r->namelist);
}


/****************************************************************
 *			STRIP_SINGLETONS 			*
 ****************************************************************
 * Return the result of removing any singleton role from to_strip
 * that is in a position matched by a singleton role in strip_from.
 * If singleton roles mismatch, report an error at expression e.
 */

PRIVATE STR_LIST *remove_singleton_from_namelist(STR_LIST *l)
{
  STR_LIST *rev_result = NULL;
  STR_LIST *p, *result;

  for(p = l; p != NIL; p = p->tail) {
    char *name_with_tag = p->head.str;
    if(is_singleton_role(name_with_tag + 1) &&
       is_present(*name_with_tag)) {
      p = p->tail;
      break;
    }
    SET_LIST(rev_result, str_cons(name_with_tag, rev_result));
  }
  result = p;
  for(p = rev_result; p != NIL; p = p->tail) {
    result = str_cons(p->head.str, result);
  }
  drop_list(rev_result);
  return result;
}

PRIVATE ROLE *strip_singletons(ROLE *to_strip, ROLE *strip_from, EXPR *e)
{
  ROLE *r1, *r2, *result;
  char *s, *t;

  if(to_strip == NULL || strip_from == NULL) return to_strip;

  if(RKIND(to_strip) == PAIR_ROLE_KIND && RKIND(strip_from) == PAIR_ROLE_KIND) {
    r1 = strip_singletons(to_strip->role1, strip_from->role1, e);
    r2 = strip_singletons(to_strip->role2, strip_from->role2, e);
  }
  else {
    r1 = to_strip->role1;
    r2 = to_strip->role2;
  }
  bump_role(r1);
  bump_role(r2);

  {STR_LIST *new_namelist;
   s = get_singleton(strip_from);
   t = get_singleton(to_strip);
   if(s == NULL || t == NULL) {
     new_namelist = to_strip->namelist;
   }
   else {
     if(strcmp(s, t) != 0) {
       role_error(e, s, t, 0);
     }
     new_namelist = remove_singleton_from_namelist(to_strip->namelist);
   }
   result = build_role(to_strip->kind, new_namelist, r1, r2);
  }

  drop_role(r1);
  drop_role(r2);
  return result;
}


/****************************************************************
 *			STRIP_ALL_SINGLETONS 			*
 ****************************************************************
 * Return the role that results from role to_strip by deleting  *
 * all singleton roles.						*
 ****************************************************************/

PRIVATE ROLE *strip_all_singletons(ROLE *to_strip)
{
  ROLE *r1, *r2, *result;
  STR_LIST *nl;

  if(to_strip == NULL || RKIND(to_strip) == FUN_ROLE_KIND) return to_strip;

  if(RKIND(to_strip) == PAIR_ROLE_KIND) {
    r1 = strip_all_singletons(to_strip->role1);
    r2 = strip_all_singletons(to_strip->role2);
  }
  else {
    r1 = to_strip->role1;
    r2 = to_strip->role2;
  }
  bump_role(r1);
  bump_role(r2);

  bump_list(nl = remove_singleton_from_namelist(to_strip->namelist));
  result = build_role(to_strip->kind, nl, r1, r2);

  drop_role(r1);
  drop_role(r2);
  drop_list(nl);
  return result;
}


/****************************************************************
 *			MERGE_NAMELISTS	 			*
 ****************************************************************
 * Merge name lists l1 and l2, collapsing duplicates and
 * setting the mode.  (The mode indicates whether the role
 * is present (ROLE_ROLE_MODE), whether a check__r is present
 * (CHECK_ROLE_MODE), whether a no__r is present (NO_ROLE_MODE)
 * and whether a kill__r is present (KILL_ROLE_MODE).  The mode
 * is the first byte of a name.
 *
 * Lists l1 and l2 must be in alphabetical order.
 */

PRIVATE STR_LIST *merge_namelists(STR_LIST *l1, STR_LIST *l2)
{
  if(l1 == NIL) return l2;
  if(l2 == NIL) return l1;

  {char *name_with_tag1 = l1->head.str;
   char *name_with_tag2 = l2->head.str;
   int mode1 = *name_with_tag1;
   int mode2 = *name_with_tag2;
   char *name1 = name_with_tag1 + 1;
   char *name2 = name_with_tag2 + 1;
   int cmp = strcmp(name1, name2);
   STR_LIST *result;

   if(cmp == 0) {
     result = str_cons(make_tagged_string(mode1 | mode2, name1),
		     merge_namelists(l1->tail, l2->tail));
   }
   else if(cmp< 0) {
     result = str_cons(name_with_tag1,
		       merge_namelists(l1->tail,l2));
   }
   else {
     result = str_cons(name_with_tag2,
		       merge_namelists(l1, l2->tail));
   }
   return result;
  }
}


/****************************************************************
 *			SORT_NAMELIST	 			*
 ****************************************************************
 * Return l, sorted with duplicates merged by mode.
 * partial_sort_namelist(x,k,y,r)  sets y to the result of
 * sorting the first k members of list x, and sets r to that
 * part of x following its first k members.  Both y and
 * r are reference counted.
 */

PRIVATE void partial_sort_namelist(STR_LIST *in, int k,
				  STR_LIST **out, STR_LIST **rest)
{
  if(k == 1) {
    bump_list(*out = str_cons(in->head.str, NIL));
    bump_list(*rest = in->tail);
  }
  else {
    int half = k >> 1;
    STR_LIST *out1, *out2, *rest1;
    partial_sort_namelist(in, half, &out1, &rest1);
    partial_sort_namelist(rest1, k-half, &out2, rest);
    bump_list(*out = merge_namelists(out1, out2));
    drop_list(out1);
    drop_list(rest1);
    drop_list(out2);
  }
}


PRIVATE STR_LIST *sort_namelist(STR_LIST *l)
{
  STR_LIST *out, *rest;
  if(l == NIL) return NIL;
  partial_sort_namelist(l, list_length(l), &out, &rest);
  out->ref_cnt--;
  return out;
}


/****************************************************************
 *			MELD_ROLES	 			*
 ****************************************************************
 * Return the role obtained by melding roles r1 and r2.  Return
 * NULL if r1 and r2 cannot be sensibly melded.
 */

PRIVATE STR_LIST *close_fail_roles(STR_LIST *l, int inmode);

ROLE *meld_roles(ROLE *r1, ROLE *r2)
{
  int kind1, kind2;
  ROLE *result = NULL;

  /* A null role melds with anything */

  if(r2 == NULL) return r1;
  if(r1 == NULL) return r2;

  /* Get the kinds.  A BASIC_ROLE can become a PAIR_ROLE or
     a FUNCTION_ROLE upon melding. */

  kind1 = RKIND(r1);
  kind2 = RKIND(r2);
  if(kind1 != kind2) {
    if(kind1 == BASIC_ROLE_KIND) r1->kind = kind1 = kind2;
    else if(kind2 == BASIC_ROLE_KIND) r2->kind = kind2 = kind1;
  }

  /* Can't meld different kinds of roles, unless one was basic. */

  if(kind1 != kind2) goto out;

  {ROLE *lft, *rgt;
   STR_LIST *root_namelist;

  /* Meld the children */

   bump_role(lft = meld_roles(r1->role1, r2->role1));
   bump_role(rgt = meld_roles(r1->role2, r2->role2));

   /* Meld the root namelists.  Close the fail roles. */

   bump_list(root_namelist = merge_namelists(r1->namelist, r2->namelist));
   SET_LIST(root_namelist, close_fail_roles(root_namelist, 0));
   result = build_role(kind1, root_namelist, lft, rgt);
   drop_list(root_namelist);
   drop_role(lft);
   drop_role(rgt);
  }

out:
# ifdef DEBUG
    if(trace_role) {
      trace_t(1);
      trace_rol(r1);
      fprintf(TRACE_FILE, "\n  r2   = ");
      trace_rol(r2);
      fprintf(TRACE_FILE, "\n  meld = ");
      trace_rol(result); tracenl();
    }
# endif

  return result;
}


/****************************************************************
 *			CHECKED_MELD_ROLES     			*
 ****************************************************************
 * Return meld_roles(r1,r2), but also check the result for
 * consistency.
 */

ROLE *checked_meld_roles(ROLE *r1, ROLE *r2, EXPR *e)
{
  ROLE *result;
  bump_role(result = meld_roles(r1, r2));
  if(result != NULL) {
    SET_ROLE(result, complete_role(result, 0));
    check_role(result, e);
    if(result != NULL) result->ref_cnt--;
  }
  return result;
}


/****************************************************************
 *			CHECK_ROLE	 			*
 ****************************************************************
 * Check that roles r is consistent.  Report an error
 * at expression e if not.  If e is null, print the
 * error with no expression.  If r is inconsistent,
 * then try to patch it up to prevent further messages.
 *
 * If no_check_checks is true, then suppress checking of
 * check__ roles.
 *
 * check_namelist is a helper function that checks a single
 * name list.
 */

PRIVATE int no_check_checks = 0;

void check_namelist(STR_LIST *l, EXPR *e)
{
  STR_LIST *p;
  char *name_with_tag, *name, *singletons[2];
  Boolean present;
  int mode;
  int num_singletons = 0;

# ifdef DEBUG
    if(trace_role) {
      trace_t(44);
      print_str_list_nl(l);
    }
# endif
  for(p = l; p != NULL; p = p->tail) {
    name_with_tag = p->head.str;
    mode          = name_with_tag[0];
    present       = is_present(mode);
    name          = name_with_tag + 1;

    /* Check if there is a check__r, but not r. */

    if(!no_check_checks) {
      if((mode & CHECK_ROLE_MODE) && !present) {
	role_error(e, make_check_role(name), concat_id("no ", name), 1);
      }
    }

    /* Check if there is a no__r, but there is an r. */

    if((mode & NO_ROLE_MODE) && present) {
      role_error(e, make_no_role(name), name, 1);
    }

    /* Count singleton nodes */

    if(is_singleton_role(name)) {
      if(num_singletons <= 1) {
	singletons[num_singletons] = name;
      }
      num_singletons++;
    }
  }

  /* Check for conflicting singleton nodes */

  if(num_singletons > 1) {
    role_error(e, singletons[0], singletons[1], 0);
  }
}


void check_role(ROLE *r, EXPR *e)
{
  if(r == NULL) return;
  if(r->role1 != NULL) {
    if(RKIND(r) == FUN_ROLE_KIND) {
      no_check_checks++;
      check_role(r->role1, e);
      no_check_checks--;
    }
    else {
      check_role(r->role1, e);
    }
  }
  if(r->role2 != NULL) check_role(r->role2, e);
  check_namelist(r->namelist, e);
}


/****************************************************************
 *			CLEAN_ROLE				*
 ****************************************************************
 * Return a role namelist similar to l, but forced to be consistent.
 * If l is already consistent, this just returns l.  If l has a
 * singleton role, set sing to true.  Otherwise, set sing = false.
 */

STR_LIST *clean_role(STR_LIST *l, Boolean *sing)
{
  if(l == NIL) return NIL;

  {Boolean rest_sing;
   STR_LIST *result;
   STR_LIST *rest_l = clean_role(l->tail, &rest_sing);
   char *name_with_tag = l->head.str;
   int mode = *name_with_tag;
   char *name = name_with_tag + 1;

   /* Handle a singleton role -- only put it in the result if there isn't
      one there already. */

   if(is_singleton_role(name)) {
     if(!is_present(mode)) {
       *sing = rest_sing;
       result = rest_l;
       goto out;
     }
     else {
       *sing = TRUE;
       if(rest_sing) {result = rest_l; goto out;}
       else if(rest_l == l->tail) {result = l; goto out;}
       else {
	 result = str_cons(name_with_tag, rest_l);
	 goto out;
       }
     }
   }

   /* Handle any other kind of role. */

   *sing = rest_sing;
   {int check_is_bad = (mode & CHECK_ROLE_MODE) &&
		       ((mode & KILL_ROLE_MODE) || !(mode & ROLE_ROLE_MODE));
    int no_is_bad = (mode & (NO_ROLE_MODE | KILL_ROLE_MODE | ROLE_ROLE_MODE))
		    == (NO_ROLE_MODE | ROLE_ROLE_MODE);
    if(check_is_bad | no_is_bad) {
      result = str_cons(make_tagged_string(mode & (KILL_ROLE_MODE | ROLE_ROLE_MODE),
					 name),
		      rest_l);
      goto out;
    }
    else if(rest_l == l->tail) {result = l; goto out;}
    else {
      result = str_cons(name_with_tag, rest_l);
      goto out;
    }
   }

 out:
#  ifdef DEBUG
      if(trace_role) {
	fprintf(TRACE_FILE, "clean_role:\n in: ");
	print_str_list_nl(l);
	fprintf(TRACE_FILE, " out: ");
	print_str_list_nl(result);
      }
#  endif

   return result;
  }
}


/****************************************************************
 *			POSSIBLY_CHECK				*
 ****************************************************************
 * If suppress is false, then check role r, and return a version
 * that is clean at its root.  If suppress is true, just return
 * a version of r that is clean at its root.
 */

PRIVATE ROLE *possibly_check(ROLE *r, EXPR *e, Boolean suppress)
{
  if(r == NULL) return NULL;

  if(!suppress) {
    role_error_occurred = FALSE;
    check_role(r,e);
    if(!role_error_occurred) return r;
  }

  {Boolean sing;
   STR_LIST *clean_list = clean_role(r->namelist, &sing);
   if(clean_list == r->namelist) return r;
   else return build_role(r->kind, clean_list, r->role1, r->role2);
  }
}


/****************************************************************
 *			ROLELIST_MEMBER				*
 ****************************************************************
 * Return TRUE if name is a member of l, in the sense that it
 * is present in l.  If killed is true, then also return TRUE if
 * name has been killed in l.
 */

Boolean rolelist_member(char *name, STR_LIST *l, Boolean killed)
{
  int base_mode;
  char *base_name;
  STR_LIST *p;
  Boolean result = FALSE;

  /* We must be careful about no__r, check__r and kill__r modes, since
     they occur in l as r, with a mode indicator.  */

  get_base(name, 0, &base_name, &base_mode);

  for(p = l; p != NIL; p = p->tail) {
    char *name_with_tag = p->head.str;
    int mode = *name_with_tag;
    if(strcmp(name_with_tag + 1, base_name) == 0) {
      result = (base_mode != 0) ? (mode & base_mode) :
	       (killed)         ? (mode & (ROLE_ROLE_MODE | KILL_ROLE_MODE))
				: is_present(mode);
      goto out;
    }
  }

out:
# ifdef DEBUG
    if(trace_role) {
      trace_t(76, name, killed, result);
      print_str_list_nl(l);
    }
# endif
  return result;
}


/****************************************************************
 *			CLOSE_FAIL_ROLES 			*
 ****************************************************************
 * For each exception ex that is not recognized as trapped,
 * transfer roles as follows in list l.
 *
 *   have		get			if have
 *   no__pot__fail	no__pot__fail__ex       pot__fail__ex
 *   kill__pot__fail	kill__pot__fail__ex	pot__fail__ex
 *   no__act__fail	no__act__fail__ex	act__fail__ex
 *   kill__act__fail	kill__act__fail__ex	act__fail__ex
 *
 * Inmode adds further control.  It should be 0 when called from
 * outside.  When called from inside, mode has bit fields that tell
 * whether it should be assumed that certain things have been seen.
 * The bits of inmode, from low to high, are:
 *   0
 *   0
 *   no__pot__fail
 *   kill__pot__fail
 *   0
 *   0
 *   no__act__fail
 *   kill__act__fail
 *
 * Return the resulting list.
 */

PRIVATE STR_LIST *close_fail_roles(STR_LIST *l, int inmode)
{
  char *name_with_tag;
  char *name;
  LIST *tl, *rest;
  int mode, new_mode;

  if(l == NULL) return NULL;
  name_with_tag = l->head.str;
  name = name_with_tag + 1;
  mode = *name_with_tag & 0xC;
  tl   = l->tail;

  /* Handle pot__fail */

  if(strcmp(name, pot_fail) == 0) {
    rest = close_fail_roles(tl, inmode | mode);
    new_mode = mode;
  }

  /* Handle pot__fail__ex */

  else if(prefix("pot__fail__", name)) {
    int u = usually_trapped_name(name + 11);
    new_mode = mode;
    if((inmode & 0xC) && u != 1) new_mode = mode | (inmode & 0xC);
    if(u == 1 && ignore_trapped_exceptions) new_mode |= KILL_ROLE_MODE;
    rest = close_fail_roles(tl, inmode);
  }

  /* Handle act__fail */

  else if(strcmp(name, act_fail) == 0) {
    rest = close_fail_roles(tl, inmode | (mode << 4));
    new_mode = mode;
  }

  /* Handle act__fail__ex */

  else if(prefix("act__fail__", name)) {
    int u = usually_trapped_name(name + 11);
    new_mode = mode;
    if((inmode & 0xC0) && u != 1) new_mode = mode | ((inmode>> 4) & 0xC);
    if(u == 1 && ignore_trapped_exceptions) new_mode |= KILL_ROLE_MODE;
    rest = close_fail_roles(tl, inmode);
  }

  /* Handle other roles */

  else {
    rest = close_fail_roles(tl, inmode);
    new_mode = mode;
  }

  if(new_mode == mode) {
    if(rest == tl) return l;
    else return str_cons(name_with_tag, rest);
  }
  else {
    return str_cons(make_tagged_string(new_mode, name), rest);
  }
}


/****************************************************************
 *			COMPLETE_NAMELIST 			*
 ****************************************************************
 * Add roles to list l that are implied by rules of the form
 * Advise r => s %Advise.
 */

STR_LIST *complete_namelist(STR_LIST *l)
{
  STR_LIST *adds = NIL;
  STR_LIST *unprocessed_names;

  /* Get the role names to be added, and put them
     into list adds.
     List unprocessed_names is a list of the names
     that have not been tried for rules or act__fail
     additions. */

# ifdef DEBUG
    if(trace_role) {
      trace_t(45);
      print_str_list_nl(l);
    }
# endif
  bump_list(unprocessed_names = l);
  while(unprocessed_names != NIL) {
    STR_LIST *add, *q;
    char *name_with_tag = unprocessed_names->head.str;
    SET_LIST(unprocessed_names, unprocessed_names->tail);
    if(is_present(*name_with_tag)) {
      char *name = name_with_tag + 1;

      /* Handle rules r => s */

      bump_list(add = get_role_completion_list_tm(name));
      for(q = add; q != NIL; q = q->tail) {
	char *add_name = q->head.str;
	if(!rolelist_member(add_name, l, TRUE) &&
	   !rolelist_member(add_name, adds, TRUE)) {
	  char *tagged_name = make_tagged_string(ROLE_ROLE_MODE, add_name);
	  SET_LIST(unprocessed_names, str_cons(tagged_name, unprocessed_names));
	  SET_LIST(adds, str_cons(tagged_name, adds));
	}
      }
      drop_list(add);
    }
  }

  /* If anything was added, then merge it with l.  Otherwise, just
     return l. */

  if(adds == NIL) {
#   ifdef DEBUG
      if(trace_role) trace_t(405);
#   endif
    return l;
  }

  {STR_LIST *result, *sorted_adds;

   bump_list(sorted_adds = sort_namelist(adds));
   bump_list(result = merge_namelists(sorted_adds, l));
   drop_list(adds);
   drop_list(sorted_adds);
   if(result != NULL) result->ref_cnt--;
#  ifdef DEBUG
     if(trace_role) {
       trace_t(406);
       print_str_list_nl(result);
     }
#  endif
   return result;
  }
}


/****************************************************************
 *			COMPLETE_ROLE	 			*
 ****************************************************************
 * Complete the namelist of the root of l if full is false.
 * Complete all of the namelists in l if full is true.
 * But just return r if not checking properties.
 */

ROLE *complete_role(ROLE *r, Boolean full)
{
  STR_LIST *completed_namelist, *namelist;
  ROLE *lchild, *rchild;

  if(r == NULL) return NULL;

  if(should_suppress_warning(err_flags.suppress_property_warnings)) return r;

  namelist = r->namelist;
  if(namelist == NIL) return r;

  completed_namelist = complete_namelist(namelist);
  if(!full) {
    if(completed_namelist == namelist) return r;
    lchild = r->role1;
    rchild = r->role2;
  }
  else {
    lchild = complete_role(r->role1, TRUE);
    rchild = complete_role(r->role2, TRUE);
    if(completed_namelist == namelist &&
       lchild == r->role1 && rchild == r->role2) return r;
  }

  return build_role(r->kind, completed_namelist, lchild, rchild);
}


/****************************************************************
 *			MATCH_TRANSFER_ROLE 			*
 ****************************************************************
 * Set the roles of the pattern variables in e1 according to the
 * corresponding roles in r.  If there is an error, report it
 * at match.
 */

PRIVATE void match_transfer_role(EXPR *e1, ROLE *inr, EXPR *match)
{
  ROLE *r, *pat_role;
  EXPR *sse1;

  if(e1 == NULL_E || inr == NULL) return;

  bump_role(r = inr);
# ifdef DEBUG
    if(trace_role > 1) {
      trace_t(6);
      trace_rol(r);
      fprintf(TRACE_FILE, ", expr =\n");
      print_expr(e1,0);
    }
# endif

  /* Role-check the pattern, to collapse roles from SAME_E nodes. */

  bump_role(pat_role = role_check(e1, FALSE));

  /* Now strip singletons from r that are matched by singletons in
     pat_role */

  SET_ROLE(r, strip_singletons(r, pat_role, e1));
  SET_ROLE(r, remove_actuals(r));

  /* At a pattern variable, meld with input role */

  sse1 = skip_sames(e1);
  if(EKIND(sse1) == PAT_VAR_E) {
    ROLE *s;
    bump_role(s = checked_meld_roles(pat_role, r, match));
    if(sse1->E1 != NULL_E) {
      SET_ROLE(r, strip_all_singletons(r));
      SET_ROLE(sse1->E1->role, meld_roles(pat_role, r));
    }

#   ifdef DEBUG
      if(trace_role) {
	trace_t(7, sse1->STR);
	fprintf(TRACE_FILE, "role = ");
	trace_rol(s);
	tracenl();
      }
#   endif
    drop_role(s);
  }

  /* At pairs, recur on parts */

  else if(EKIND(e1) == PAIR_E && r != NULL && RKIND(r) == PAIR_ROLE_KIND) {
    match_transfer_role(e1->E1, r->role1, match);
    match_transfer_role(e1->E2, r->role2, match);
  }

  /* At SAME_E nodes propagate downwards */

  else if(EKIND(e1) == SAME_E) {
    match_transfer_role(e1->E1, r, match);
  }

  drop_role(r);
  drop_role(pat_role);
}


/****************************************************************
 *			EXPR_TRANSFER_ROLE 			*
 ****************************************************************
 * Attach role r to expression e, attaching the parts of r to the
 * parts of e.
 */

PRIVATE void expr_transfer_role(ROLE *r, EXPR *e)
{
  SET_ROLE(e->role, r);
  if(r != NULL && e != NULL
     && EKIND(e) == PAIR_E && RKIND(r) == PAIR_ROLE_KIND) {
    expr_transfer_role(r->role1, e->E1);
    expr_transfer_role(r->role2, e->E2);
  }
}


/****************************************************************
 *			GET_ACTS_POTS				*
 ****************************************************************
 * Return a role with only a root role, and having actuals and
 * potentials from the root of r.  mode tells whether to get
 * actuals or potentials or both.
 *
 *     mode       get
 *      1        only potentials
 *      2        only actuals
 *      3        both
 */

PRIVATE STR_LIST *get_acts_pots_namelist(STR_LIST *l, int sel)
{
  if(l == NIL) return NIL;
  else {
    char *name_with_tag = l->head.str;
    int mode = *name_with_tag;
    char *name = name_with_tag + 1;
    STR_LIST *tail_list = get_acts_pots_namelist(l->tail, sel);
    if(is_present(mode)) {
      if((sel & 1) && is_potential(name) ||
	 (sel & 2) && is_actual(name)) {
	if(tail_list == l->tail) return l;
	else {
	  return str_cons(make_tagged_string(ROLE_ROLE_MODE, name),
			    tail_list);
	}
      }
    }
    return tail_list;
  }
}


PRIVATE ROLE *get_acts_pots(ROLE *r, int mode)
{
  if(r == NULL) return NULL;
  else {
    return build_role(BASIC_ROLE_KIND,
		      get_acts_pots_namelist(r->namelist, mode),
		      NULL, NULL);
  }
}


/****************************************************************
 *			GET_UPROLES	 			*
 ****************************************************************
 * If latent is true, then return a list of the potential roles
 * and the actual roles that occur in l.  If latent is false,
 * return a list of potential and actual roles in l, plus
 * those potential roles converted to actual roles.
 */

PRIVATE STR_LIST *get_uproles(STR_LIST *l, Boolean latent)
{
  STR_LIST *p, *result;

# ifdef DEBUG
    if(trace_role) {
      trace_t(407, latent);
      print_str_list_nl(l);
    }
# endif

  /* Get the potential and actual roles of l. */

  result = get_acts_pots_namelist(l, 3);

  /* Augment by actual roles from potential roles if not latent */

  if(!latent) {
    for(p = result; p != NIL; p = p->tail) {
      if(is_potential(p->head.str + 1)) {
	result = str_cons(convert_act_pot(p->head.str, 0), result);
      }
    }
  }

# ifdef DEBUG
    if(trace_role) {
      trace_t(408);
      print_str_list_nl(result);
    }
# endif
  return result;
}


/****************************************************************
 *			TRANSFER_POTENTIALS 			*
 ****************************************************************
 * Expression e is f(a), where f has role rf and a has role ra.
 * Transfer potential roles of rf and ra to e.  Also make potential
 * roles of rf and ra into actual roles of e, unless rf is a
 * function role with "latent__" in its domain role.  Also transfer
 * actual role in the root of rf and ra to the root of the role
 * of e.
 */

PRIVATE void transfer_potentials(EXPR *e, ROLE *rf, ROLE *ra)
{
  STR_LIST *uproles, *uproles1, *uproles2;
  ROLE *newrole;
  Boolean latent = FALSE;

  if(rf != NULL) {
    if(RKIND(rf) == FUN_ROLE_KIND && rf->role1 != NULL &&
       rolelist_member("latent__", rf->role1->namelist, 0)) latent = TRUE;
    bump_list(uproles1 = get_uproles(rf->namelist, latent));
  }
  else uproles1 = NIL;
  if(ra != NULL) bump_list(uproles2 = get_uproles(ra->namelist, latent));
  else uproles2 = NIL;

  bump_list(uproles = append(uproles1, uproles2));
# ifdef DEBUG
    if(trace_role) {
      trace_t(74);
      print_str_list_nl(uproles);
    }
# endif
  SET_LIST(uproles, sort_namelist(uproles));
# ifdef DEBUG
    if(trace_role) {
      trace_t(75);
      print_str_list_nl(uproles);
    }
# endif
  bump_role(newrole = build_role(BASIC_ROLE_KIND, uproles, NULL, NULL));
  SET_ROLE(e->role, meld_roles(e->role, newrole));
  drop_role(newrole);
  drop_list(uproles);
  drop_list(uproles1);
  drop_list(uproles2);
}


/****************************************************************
 *			TRANSFER_RULE_ROLE 			*
 ****************************************************************
 * If rhs is not null, then we have a declaration
 *    expand lhs => rhs
 * Otherwise, we have a pattern declaration
 *    pattern lhs => ...
 *
 * Transfer the role to the function being defined.
 */

PRIVATE void transfer_rule_role(EXPR *lhs, EXPR *rhs, Boolean suppress_errs)
{
  EXPR *sslhs;
  ROLE *r;

  sslhs = skip_sames(lhs);
  if(EKIND(sslhs) == APPLY_E) {
    if(rhs != NULL) {
      bump_role(r = strip_singletons(rhs->role, lhs->role, lhs));
      SET_ROLE(r, remove_actuals(r));
    }
    else {
      bump_role(r = sslhs->role);
    }
    while(EKIND(sslhs) == APPLY_E) {
      SET_ROLE(r, fun_role(sslhs->E2->role, r));
      sslhs = skip_sames(sslhs->E1);
    }
    SET_ROLE(r, meld_roles(sslhs->role, r));
    if(r != NULL) {
       SET_ROLE(r, possibly_check(r, lhs, suppress_errs));
      SET_ROLE(sslhs->role, r);
      drop_role(r);
    }
  }
}


/****************************************************************
 *			ROLE_CHECK	 			*
 ****************************************************************
 * Role_check checks that the roles in e are consistent.
 * It return a role for e.
 * It also sets the roles of expressions where appropriate.  If
 * suppress_errs is true, roles are computed but no errors are
 * reported.
 */

PRIVATE void install_role(ROLE **r, EXPR *e, Boolean suppress_errs)
{
  if(*r != NULL) {
    set_role(r, possibly_check(*r, e, suppress_errs));
    SET_ROLE(e->role, *r);
  }
}

PRIVATE void add_basic_role(char *br, EXPR *e, Boolean suppress_errs)
{
  ROLE *r;
  bump_role(r = basic_role(br));
  SET_ROLE(r, complete_role(r, 0));
  SET_ROLE(r, meld_roles(e->role, r));
  install_role(&r, e, suppress_errs);
  drop_role(r);
}


ROLE *role_check(EXPR *e, Boolean suppress_errs)
{
  EXPR *e1, *e2;
  ROLE *r1, *r2, *r, *result;
  EXPR_TAG_TYPE e_kind;

  if(e == NULL) return NULL;
  r = r1 = r2 = NULL;            /* defaults */
  bump_role(result = e->role);   /* default */
  e1 = e->E1;
  e2 = e->E2;
  e_kind = EKIND(e);

# ifdef DEBUG
   if(trace_role) trace_t(2, e, expr_kind_name[e_kind]);
# endif

  switch(e_kind) {
    case APPLY_E:

      /* Suppose the expression is f(a).  We do the following.
	   (1) If the f has a function role r1 -> r2, then

	       (a) role r1 is melded with the role of a.
	       (b) role r2 is melded with the role of f(a).

	   (2) If f is a function and either f or a has
	       a potential role pot__r at
	       its root, then f(a) gets that potential role and
	       (unless f has role latent__ at its root) f(a)
	       also gets role act_r, to indicate that r has been
	       actualized by executing the function.

	   (3) Actual roles of f and a are propagated to f(a).

	   (4) If f has role select__left, f(a) gets left-roles
	       of a.  If f has role select__right, f(a) gets
	       right-roles from a. */


      /* Check the function and its argument separately */

      bump_role(r1 = role_check(e1, suppress_errs));
      bump_role(r2 = role_check(e2, suppress_errs));

      /* Meld function domain role with argument role, and
	 transfer output role to result of application.
	 (Part (1) above.) */

      if(r1 != NULL && RKIND(r1) == FUN_ROLE_KIND) {
	SET_ROLE(r, meld_roles(r1->role1, r2));
	if(r != NULL) {
	  SET_ROLE(r, possibly_check(r, e, suppress_errs));
	  expr_transfer_role(r, e->E2);
	}
	SET_ROLE(result, meld_roles(e->role, r1->role2));
	SET_ROLE(e->role, result);
      }

      /* Transfer potential roles up from the argument and the
	function, and convert them to actual roles if appropriate.
	(Parts (2) and (3) above.) */

      if(r1 != NULL || r2 != NULL) {
	transfer_potentials(e, r1, r2);
      }

      /* If the function has role select__left, then get the left-roles
	 from the argument.  If the function has role select__right,
	 then get the right-roles from the argument */

      SET_ROLE(result, e->role);
      if(r1 != NULL && r2 != NULL && RKIND(r2) == PAIR_ROLE_KIND) {
	if(rolelist_member(select_left_role, r1->namelist, 0)) {
	  SET_ROLE(result, meld_roles(r2->role1, result));
	}
	if(rolelist_member(select_right_role, r1->namelist, 0)) {
	  SET_ROLE(result, meld_roles(r2->role2, result));
	}
      }

      /* Complete the role */

      SET_ROLE(result, complete_role(result, 0));

      /* Perform consistency check at e */

      SET_ROLE(result, possibly_check(result, e, suppress_errs));
      SET_ROLE(e->role, result);
      goto out;

    case DEFINE_E:
      /* Do a preliminary check to set the role of the identifier
	 being defined. If we are already doing such a check, don't
	 start another one though. */

      if(!suppress_errs) role_check(e1, TRUE);
      /* Continue as for a let */
    case LET_E:

      /* The expression is either let x = a %let or define x = a %define.
	 Ensure that x and a have compatible roles.  Then transfer roles
	 from a to identifier x.  Note that the left-hand side is not
	 necessarily an identifier.  It might, for example, be
	 role~>x, where x is an identifier.  Only transfer down
	 singleton roles that are not explicitly present on the left-hand
	 side.  Do not transfer any actual roles from a to x.  Instead,
	 transfer them from a to the let-expression, since they
	 belong to the act of computing x, not to x itself. */

      {EXPR *sse1;
       ROLE *newr2 = NULL;

       /* Check the body */

       bump_role(r2 = role_check(e2, suppress_errs));

       /* Do a role-check on e1, to bring up roles in SAME_E nodes */

       bump_role(r1 = role_check(e1, suppress_errs));

       /* The left-hand side and right-hand side must have compatible
	  roles */

       bump_role(r = meld_roles(r1, r2));
       if(r != NULL && !suppress_errs) check_role(r, e);

       /* Look for an explicit singleton role on the identifier being
	  bound.  Explicit singleton roles cancel transfer of roles
	  from the body. */

       bump_role(newr2 = strip_singletons(r2, r1, e1));
       SET_ROLE(newr2, remove_actuals(newr2));
       sse1 = skip_sames(e1);

       /* Transfer the role down to the identifier */

       SET_ROLE(r, strip_all_singletons(r2));
       SET_ROLE(r, meld_roles(sse1->role, r));
       if(r != NULL) {
	 SET_ROLE(r, possibly_check(r, e, suppress_errs));
	 SET_ROLE(sse1->role, r);
       }

       /* The role of a let expr includes the actual roles
	  from the right-hand side */

       if(e_kind == LET_E) {
	 ROLE *acts;
	 bump_role(acts = get_acts_pots(r2, 2));
	 SET_ROLE(result, meld_roles(result, acts));
	 install_role(&result, e, suppress_errs);
	 drop_role(acts);
       }

       /* The body of a define expr is lazy */

       else {
	 add_basic_role(is_lazy_role, e2, suppress_errs);
       }

#      ifdef DEBUG
	 if(trace_role) {
	   trace_t(3);
	   trace_rol(r);
	   trace_t(4, sse1->STR);
	 }
#      endif

       drop_role(newr2);
       goto out;
     }

    case MATCH_E:

      /* Check the pattern and the target separately */

      bump_role(r1 = role_check(e1, suppress_errs));
      bump_role(r2 = role_check(e2, suppress_errs));

      /* The target and pattern must have matching roles */

      bump_role(r = meld_roles(r1, r2));
      if(r != NULL && !suppress_errs) check_role(r, e);

      /* Transfer roles downward into the pattern */

      match_transfer_role(e1, e2->role, e);

      /* The match expression gets the actual roles of the body
	 and the pattern */

      SET_ROLE(r1, get_acts_pots(r1, 2));
      SET_ROLE(r2, get_acts_pots(r2, 2));
      SET_ROLE(result, meld_roles(result, r1));
      SET_ROLE(result, meld_roles(result, r2));
      install_role(&result, e, suppress_errs);

      goto out;

    case EXPAND_E:
    case PAT_DCL_E:

      /* Checking the role of an expand or pattern declaration
	 involves transfering the role from the definition
	 to the identifier being defined.  The case of a
	 pattern dcl with a rule is handled under PAT_RULE_E. */

      {EXPR *sse1;

       /* Check the body and the id being defined */

       bump_role(r2 = role_check(e2, suppress_errs));
       bump_role(r1 = role_check(e1, suppress_errs));

       /* For pattern f = g or expand f = g, transfer role of g to f */

       if(e->PAT_DCL_FORM != 0) {
	 sse1 = skip_sames(e1);
	 bump_role(r = strip_singletons(r2, r1, e));
	 SET_ROLE(r, remove_actuals(r));
	 SET_ROLE(r, meld_roles(r1, r));
	 if(r != NULL) {
	   SET_ROLE(r, possibly_check(r, e, suppress_errs));
	   SET_ROLE(sse1->role, r);
	   SET_ROLE(e1->role, r);
	 }
       }
       else if(e_kind == EXPAND_E) {
	 transfer_rule_role(e1, e2, suppress_errs);
       }

       goto out;
     }

    case PAT_RULE_E:

       /* Transfer the role of the argument to the function */

       bump_role(r1 = role_check(e2, suppress_errs));
       transfer_rule_role(e2, NULL, suppress_errs);
       SET_ROLE(r1, role_check(e->E3, suppress_errs));
       SET_ROLE(r1, role_check(e1, suppress_errs));
       goto out;

    case FUNCTION_E:
      no_check_checks++;
    case PAIR_E:

      /* At a pair (a,b), make a pair-role from the roles of a and b.
	 Also acquire the actual and potential roles of a and b, since
	 computation of the pair involves computation of its parts, and
	 since the pair might be take apart to get back the potential.

	 At a function (p => e), make a function-role from the roles
	 of p and e, but first remove any actual roles from the role
	 of e.  The actual roles of e become potential roles
	 of the function.
	 */

      bump_role(r1 = role_check(e1, suppress_errs));
      if(e_kind == FUNCTION_E) no_check_checks--;
      bump_role(r2 = role_check(e2, suppress_errs));
      if(r1 != NULL || r2 != NULL) {
	ROLE *acts_pots1 = NULL, *acts_pots2 = NULL;
	int role_kind;
	if(e_kind == PAIR_E) {
	  bump_role(acts_pots1 = get_acts_pots(r1, 3));
	  bump_role(acts_pots2 = get_acts_pots(r2, 3));
	  role_kind = PAIR_ROLE_KIND;
	}
	else { /* e_kind == FUNCTION_E */
	  bump_role(acts_pots1 = make_pots_from_acts(r2));
	  SET_ROLE(r2, remove_actuals(r2));
	  role_kind = FUN_ROLE_KIND;
	}
	SET_ROLE(result, new_role(role_kind, r1, r2));
	SET_ROLE(result, meld_roles(result, acts_pots1));
	SET_ROLE(result, meld_roles(result, acts_pots2));
	SET_ROLE(result, meld_roles(e->role, result));
	install_role(&result, e, suppress_errs);
	drop_role(acts_pots1);
	drop_role(acts_pots2);
      }
      goto out;

    case PAT_VAR_E:
    case SINGLE_E:

      /* Inherit any role of the identifier associated with a pattern
	 variable, or of the body of an atomic, etc., expression, with
	 the exception of kill__, no__ and check__ roles. At a cut
	 expression, don't inherit act__produceStream.  */

      bump_role(r1 = role_check(e1, suppress_errs));
      SET_ROLE(r1, remove_knc(r1, 0));

      /* Handle a Unique. */

      if(e_kind == SINGLE_E && e->SINGLE_MODE == FIRST_ATT) {
	ROLE *rr;
	bump_role(rr = basic_role(kill_act_produce_stream));
	SET_ROLE(r1, meld_roles(r1, r));
      }

      SET_ROLE(result, meld_roles(e->role, r1));
      install_role(&result, e, suppress_errs);
      goto out;

    case SAME_E:

      /* If this is a team, do a preliminary check to get the
	 roles of the identifiers being defined.  If we are already
	 in the process of such a check, don't to another one though. */

      if(e->SCOPE == 1 && !suppress_errs) role_check(e, TRUE);

      /* Check body */

      bump_role(r1 = role_check(e1, suppress_errs));

      /* If this node's role is null, just get r1. */

      bump_role(r = e->role);
      if(r == NULL) {
	SET_ROLE(result, r1);
	SET_ROLE(e->role, result);
	goto out;
      }

      /* Meld the role of this SAME_E expr with the role of the body,
	 complete and check the resulting role.  */

      role_error_occurred = FALSE;
      SET_ROLE(result, meld_roles(r, r1));
      if(result != NULL) {
	SET_ROLE(result, complete_role(result, 0));
	SET_ROLE(result, possibly_check(result,e, suppress_errs));
	SET_ROLE(e->role, result);
      }
      if(role_error_occurred) {
	err_print(MARKED_WITH_ROLE_ERR);
	err_print_role(r);
	err_print(POSSIBLY_ROLE_ERR);
      }
      goto out;

    case RECUR_E:

      /* At continue(e), check that the role of e is compatible with
	 the role of the pattern in the heading of the loop. */

      bump_role(r1 = role_check(e1, suppress_errs));
      if(e2->E1 != NULL && r1 != NULL) {
	bump_role(r2 = role_check(e2->E1->E1, suppress_errs));
	bump_role(r = meld_roles(r1, r2));
	install_role(&r, e1, suppress_errs);
      }
      goto out;

    case OPEN_E:

      /* Meld roles of corresponding ids, and inherit the role
	 of the body. */

      {LIST *p = e->EL1;
       LIST *q = e->EL2;
       bump_role(r1 = role_check(e1, suppress_errs));
       if(p != NIL && p->head.expr->STR[0] == 1) goto out;
       if(q != NIL && q->head.expr->STR[0] == 1) goto out;
       for(; p != NIL; p = p->tail, q = q->tail) {
	 EXPR *p_expr = p->head.expr;
	 EXPR *q_expr = q->head.expr;
	 r2 = meld_roles(p_expr->role, q_expr->role);
	 if(r2 != NULL) {
	   SET_ROLE(r2, possibly_check(r2, p_expr, suppress_errs));
	   SET_ROLE(p_expr->role, r2);
	   SET_ROLE(q_expr->role, r2);
	 }
       }

       /* The open expr has the same role as its body */

       SET_ROLE(result, meld_roles(e1->role, result));
       install_role(&result, e, suppress_errs);
       goto out;
      }

    case AWAIT_E:

      /* At an await such as (:e2:), inherit the role of e2, with
	 the exception that actual roles are not inherited since the
	 expression will not be executed now, and kill__, check__ and
	 no__ roles are not inherited.  Also add role
	 is__lazy to the role of e2, to generate any warnings that
	 are associated with roles in lazy context. */

      {ROLE *up;
       bump_role(r1 = role_check(e1, suppress_errs));
       bump_role(r2 = role_check(e2, suppress_errs));
       bump_role(up = remove_knc(r2, 0));
       SET_ROLE(up, remove_actuals(up));
       add_basic_role(is_lazy_role, e2, suppress_errs);
       SET_ROLE(result, meld_roles(up, result));
       install_role(&result, e, suppress_errs);
       drop_role(up);
       break;
      }

    case LAZY_LIST_E:

      /* At [:e:], inherit only the potential roles of e.  Attach
	 is__lazy__list to the role of e, to allow for checking of
	 roles that should not occur in a lazy list. */

      {ROLE *up;
       bump_role(r1 = role_check(e1, suppress_errs));
       bump_role(up = get_acts_pots(r2, 1));
       add_basic_role(is_lazy_list_role, e1, suppress_errs);
       bump_role(r = basic_role(is_lazy_list_role));
       SET_ROLE(result, meld_roles(up, result));
       install_role(&result, e, suppress_errs);
       drop_role(up);
       break;
      }

    case IF_E:
    case TRY_E:
    case FOR_E:

      /* At for p from l do e %for, inherit actuals from l, e and p. */

      /* The role of an if or try expression such as if a then b else c
	 is the meld of the roles of b and c, plus any actual roles
	 of a, except that kill__, check__ and no__ roles are not
	 inherited from b and c.  For a try expression, do not
	 propagate act__fail roles of a. */

       bump_role(r1 = role_check(e1, suppress_errs));
       bump_role(r2 = role_check(e2, suppress_errs));
       bump_role(r  = role_check(e->E3, suppress_errs));
       SET_ROLE(r1, get_acts_pots(r1, 2));
       if(e_kind == FOR_E) {
	 SET_ROLE(r2, get_acts_pots(r2, 2));
	 SET_ROLE(r, get_acts_pots(r, 2));
       }
       else { /* e_kind == IF_E or TRY_E */
	 SET_ROLE(r2, remove_knc(r2, 0));
	 SET_ROLE(r, remove_knc(r, 0));
	 if(e_kind == TRY_E) {
	   SET_ROLE(r1, remove_fails(r1, 1));
	 }
       }
       SET_ROLE(result, meld_roles(result, r1));
       SET_ROLE(result, meld_roles(result, r2));
       SET_ROLE(result, meld_roles(result, r));
       install_role(&result, e, suppress_errs);
       break;

    case TRAP_E:
       /* At expression trap a => b or untrap a => b,
	  inherit actuals from a, and all but kill__,
	  check__ and no__ roles from b.  While processing
	  b, set trapped_exceptions to include info from a. */

       /* Handle a and b */

       {char *exc_name = get_exception_name(e1);
	bump_role(r1 = role_check(e1, suppress_errs));
	if(exc_name != NULL) {
	  if(e->TRAP_FORM == TRAP_ATT) {
	    HEAD_TYPE h;
	    h.str = exc_name;
	    SET_LIST(trapped_exceptions,
		     general_cons(h, trapped_exceptions, STR1_L));
	  }
	  else {
	    SET_LIST(trapped_exceptions,
		     str_cons(exc_name, trapped_exceptions));
	  }
	}
	bump_role(r2 = role_check(e2, suppress_errs));
	if(exc_name != NULL) {
	  SET_LIST(trapped_exceptions, trapped_exceptions->tail);
	}
       }

       SET_ROLE(r1, get_acts_pots(r1, 2));
       SET_ROLE(r2, remove_knc(r2, 0));
       SET_ROLE(result, meld_roles(result, r1));
       SET_ROLE(result, meld_roles(result, r2));
       install_role(&result, e, suppress_errs);
       break;

    case STREAM_E:
      add_basic_role(act_produce_stream, e, suppress_errs);
      SET_ROLE(result, e->role);
    case LAZY_BOOL_E:
    case LOOP_E:
    case WHERE_E:
    case TEST_E:

      /* The role of a stream such as stream a then b is the meld of
	 the roles of a and b, except that kill__, check__ and
	 no__ roles are not inherited.  It also includes act__produceStream,
	 but that is added above. */

       /* The role of a and b or of {a else b} consists of the
	  actual roles of a and the actual roles of b.  For {a}, b
	  will be NULL.  In that case, get actual role
	  act__fail__testEx. */

       /* At expression loop p = i ... cases %loop, inherit only
	  actuals from match p = i, and all but kill__, check__ and
	  no__ roles from cases. */

       /* Expression a where b inherits actual roles of b and
	  all roles of a except kill__, check__ and no__ roles. */

       bump_role(r1 = role_check(e1, suppress_errs));
       bump_role(r2 = role_check(e2, suppress_errs));
       if(e_kind == STREAM_E) {
	 SET_ROLE(r1, remove_knc(r1, 0));
	 SET_ROLE(r2, remove_knc(r2, 0));
       }
       else if(e_kind == LOOP_E || e_kind == OPEN_E) {
	 SET_ROLE(r1, get_acts_pots(r1, 2));
	 SET_ROLE(r2, remove_knc(r2, 0));
       }
       else if(e_kind == WHERE_E) {
	 SET_ROLE(r1, remove_knc(r1, 0));
	 SET_ROLE(r2, get_acts_pots(r2, 2));
       }
       else {/* e_kind == LAZY_BOOL_E or TEST_E */
	 SET_ROLE(r1, get_acts_pots(r1, 2));
	 if(e2 == NULL) {
	   SET_ROLE(r2, basic_role("act__fail__testEx"));
	 }
	 else {
	   SET_ROLE(r2, get_acts_pots(r2, 2));
	 }
       }
       SET_ROLE(result, meld_roles(result, r1));
       SET_ROLE(result, meld_roles(result, r2));
       install_role(&result, e, suppress_errs);
       break;

    case EXECUTE_E:
      bump_role(r1 = role_check(e1, suppress_errs));
      SET_ROLE(e->role, r1);
    default:
      {}
  }

 out:
   /* An expression whose type is () can only have actual roles */

   if(is_hermit_type(e->ty)) {
     SET_ROLE(e->role, keep_only_actuals(e->role));
     SET_ROLE(result, e->role);
   }

# ifdef DEBUG
    if(trace_role) {
      trace_t(5, e);
      trace_rol(result);
      tracenl();
    }
# endif
  drop_role(r1);
  drop_role(r2);
  drop_role(r);
  if(result != NULL) result->ref_cnt--;
  return result;
}


/****************************************************************
 *			DO_ROLE_CHECK	 			*
 ****************************************************************/

#pragma argsused
void do_role_check(int kind, EXPR *e)
{
  ROLE *r;

  if(should_suppress_warning(FALSE)) return;

# ifdef DEBUG
    if(trace_role > 1) {
      trace_t(289);
      print_expr(e,0);
    }
# endif

  SET_LIST(trapped_exceptions, NIL);
  bump_role(r = role_check(e,FALSE));
  SET_LIST(trapped_exceptions, NIL);
  drop_role(r);

# ifdef DEBUG
    if(trace_role > 1) {
      trace_t(79);
      print_expr(e,0);
    }
# endif
}


/****************************************************************
 *			COPY_MAIN_ROLES				*
 ****************************************************************
 * Copy the roles of the defined ids in a to corresponding
 * ids in b.
 */

void copy_main_roles(EXPR *a, EXPR *b)
{
  EXPR *pa = skip_sames(a);
  EXPR *pb = skip_sames(b);
  EXPR_TAG_TYPE pa_kind = EKIND(pa);
  EXPR_TAG_TYPE pb_kind = EKIND(pb);

  if(pa_kind == APPLY_E && pb_kind == APPLY_E) {
    copy_main_roles(pa->E1, pb->E1);
    copy_main_roles(pa->E2, pb->E2);
  }
  else if((pa_kind == DEFINE_E || pa_kind == LET_E) &&
	  (pb_kind == DEFINE_E || pb_kind == LET_E)) {
    SET_ROLE(pb->E1->role, pa->E1->role);
  }
}


/****************************************************************
 *			ROLE_POWER				*
 ****************************************************************
 * Returns r^^k.
 */

ROLE *role_power(ROLE *r, int k)
{
  if(r == NULL) return NULL;
  if (k == 1) return r;
  else return pair_role(r, role_power(r, k-1));
}


/****************************************************************
 *			DROP_HASH_ROLE				*
 ****************************************************************/

void drop_hash_role(HASH2_CELLPTR h)
{
  drop_role(h->val.role);
}


/****************************************************************
 *			BUMP_HASH_ROLE				*
 ****************************************************************/

void bump_hash_role(HASH2_CELLPTR h)
{
  bump_role(h->val.role);
}


