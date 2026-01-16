/*********************************************************************
 * File:    show/getinfo.h
 * Purpose: Get information about environments and source code from
 *          program counter.
 * Author:  Karl Abrahamson
 **********************************************************************/

struct name_typeinstr {
  char *name;
  CODE_PTR type_instrs;
};

void 		     get_one_env_info  (struct name_type *info, 
					ENVIRONMENT *env, long pc_offset, 
					LIST *bl, int k);
void 		     get_env_info	(struct name_type *info, 
					 ENVIRONMENT *env, 
					 int num_entries, long pc_offset,
					 LIST *bl);
void 		     get_line_info	(CODE_PTR pc, char **pack_name, 
					 char **file_name, int *line);
struct line_rec     * get_linerec       (struct package_descr *pd, 
					 CODE_PTR pc);


