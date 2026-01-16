/*************************************************************************
 * File:    rts/system.c
 * Purpose: Implement system access command(s)
 * Author:  Karl Abrahamson
 ************************************************************************/

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

/************************************************************************
 * This file contains interfaces to system functions for the evaluator  *
 * to use.  It also contains support for the doCmd (or system) function *
 * provided by the evaluator.						*
 ************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#endif
#ifndef S_ISDIR
# define	S_ISDIR(m)	(((m)&S_IFMT) == S_IFDIR)
#endif
#include "../misc/misc.h"
#ifdef UNIX
# include <unistd.h>
# include <sys/types.h>
# include <fcntl.h>
# include <sys/wait.h>
# include <sys/time.h>
  time_t time(time_t *tloc);
# ifndef O_NONBLOCK
#   define O_NONBLOCK O_NDELAY
# endif
#endif
#ifdef MSWIN
# include <io.h>
# include <time.h>
# include <dir.h>
#endif
#include "../alloc/allocate.h"
#include "../utils/lists.h"
#include "../utils/strops.h"
#include "../utils/filename.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../tables/tables.h"
#ifdef DEBUG
# include "../debug/debug.h"
# include "../show/prtent.h"
#endif


/****************************************************************
 * When the command manager copies a string to a pipe,          *
 * it keeps track of time so that it won't run too long.	*
 * The time conter is not decremented at each iteration of the  *
 * copy loop, however.  TIME_STEP_COUNT_INIT is the number of	*
 * iterations done per decrement of the time counter		*
 ****************************************************************/

#define TIME_STEP_COUNT_INIT 10

/*********************************************************
 *			GETENV_STDF			 *
 *********************************************************
 * Return the binding of environment variable s, or fail *
 * if there is no binding.  Presumes that s is fully	 *
 * evaluated.						 *
 *********************************************************/

ENTITY getenv_stdf(ENTITY s)
{
  char *result, ss[MAX_NAME_LENGTH+1];
  ENTITY tl;

  copy_str(ss, s, MAX_NAME_LENGTH, &tl);
  if(ENT_NE(tl, nil)) {
    failure = LIMIT_EX; 
    return zero;
  }

  if(failure >= 0) return zero;

  result = getenv(ss);
  if(result == NULL) {
    failure = DOMAIN_EX;
    failure_as_entity = qwrap(DOMAIN_EX, 
			      make_str("no such environment variable"));
    return nil;
  }

  return make_str(result);
}


/********************************************************
 *			DO_CMD_STDF			*
 ********************************************************
 * This is the standard function for doCmd.  It just    *
 * creates a lazy entity.				*
 *							*
 * The MSWIN implementation does not provide this 	*
 * function.  It just returns nil.			*
 * This implementation does not deal with the		*
 * distinction between internal and external file	*
 * names!						*
 ********************************************************/

#ifdef UNIX
ENTITY do_cmd_stdf(ENTITY cmd, ENTITY s)
{
  return make_lazy_prim(DOCMD_TMO, cmd, s);
}
#endif

#ifdef MSWIN
ENTITY do_cmd_stdf(ENTITY cmd, ENTITY s)
{
  return nil;
}
#endif

#ifdef UNIX

/********************************************************
 *			DO_CMD				*
 ********************************************************
 * cmd is a list of strings whose first member is the	*
 * command to execute, and whose subsequent members are *
 * the arguments to the command.  Run cmd with standard *
 * input containing string s, and return its output	*
 * (a string).						*
 ********************************************************/

ENTITY do_cmd(ENTITY cmd, ENTITY s, LONG *l_time)
{
  ENTITY the_cmd, pair, cmdmann, result = nil, t1, t2, tl;
  char *argsp, **argv = NULL, *args = NULL, *newcmd = NULL;
  int inpipe[2], outpipe[2], nargs, cmd_len;
  LONG args_len, arg_avail, args_size;
  pid_t pid;
  LIST *path, *q;
  struct stat stat_buf;

  REG_TYPE mark = reg3(&the_cmd, &pair, &cmdmann);
  reg3(&result, &t1, &t2);
  reg2_param(&cmd, &s);

  /*---------------*
   * Evaluate cmd. *
   *---------------*/

  the_cmd = full_eval(cmd, l_time);
  if(failure >= 0) {
    if(failure == TIME_OUT_EX) {
      result = make_lazy_prim(DOCMD_TMO, cmd, s);
    }
    unreg(mark);
    return nil;
  }

  /*---------------------------------------------------------*
   * If the command is nil, then there is no command to run. *
   *---------------------------------------------------------*/

  if(IS_NIL(the_cmd)) {
    failure = DO_COMMAND_EX;
    unreg(mark);
    return nil;
  }

  /*-----------------------------------------------------------------*
   * Calculate the length of the arguments, and how many arguments   *
   * there are.							     *
   *-----------------------------------------------------------------*/

  args_len = 0;
  nargs    = 0;
  for(t1 = remove_indirection(ast_tail(the_cmd)); 
      !IS_NIL(t1) && failure < 0; 
      t1 = remove_indirection(ast_tail(t1))) {
    nargs++;
    t2        = ast_length(ast_head(t1), 0, l_time);
    args_len += get_ival(t2, LIMIT_EX);
  }
  if(failure >= 0) goto out;

  /*-----------------------------------------------------------------*
   * Allocate space for the command.  We need extra space for newcmd *
   * because a directory might be added to newcmd below.  Also,      *
   * buffer args will be used for temporary storage of the command   *
   * so get at least MAX_FILE_NAME_LENGTH space for it as well.      *
   *-----------------------------------------------------------------*/

  newcmd = (char*)  BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
  argv   = (char**) BAREMALLOC((nargs+2)*sizeof(char*));
  args_size = args_len + nargs;
  if(args_size <= MAX_FILE_NAME_LENGTH) {
    args_size = MAX_FILE_NAME_LENGTH + 1;
  }
  args = (char*)  BAREMALLOC(args_size);

  /*-------------------------------------------------------------------*
   * Copy the command name (without the arguments) into buffer newcmd. *
   *-------------------------------------------------------------------*/

  t1       = ast_head(the_cmd);     /* Command. */
  the_cmd  = ast_tail(the_cmd);     /* Command arguments. */
  if(failure >= 0) goto out;

  copy_str(newcmd, t1, MAX_FILE_NAME_LENGTH, &tl);
  if(!IS_NIL(tl)) {
    failure = DO_COMMAND_EX;
    goto out;
  }
  force_internal(newcmd);

  /*----------------------------------------------------*
   * Replace newcmd by the absolute command. 		*
   * If the command starts with /, then it is absolute 	*
   * already.  Otherwise, search the path.		*
   *----------------------------------------------------*/

  if(is_absolute(newcmd)) {
    install_standard_dir(newcmd, MAX_FILE_NAME_LENGTH);
#   ifdef UNIX
      install_home_dir(newcmd, MAX_FILE_NAME_LENGTH);
#   endif
    cmd_len = strlen(newcmd);
  }

  else {

#   ifdef DEBUG
      if(trace_docmd) trace_i(290, newcmd);
#   endif

    cmd_len = strlen(newcmd);
    path    = file_colonsep_to_list(getenv("PATH"));
    for(q = path; q != NIL; q = q->tail) {
      LONG q_len = strlen(q->head.str);
      if(q_len + cmd_len + 1 <= MAX_FILE_NAME_LENGTH) {
        strcpy(args, q->head.str);
        strcat(args + q_len, INTERNAL_DIR_SEP_STR);
        strcat(args + q_len, newcmd);

#       ifdef DEBUG
          if(trace_docmd) trace_i(291, args);
#       endif

        if(stat_file(args, &stat_buf) == 0 && S_ISREG(stat_buf.st_mode)) {
	  strcpy(newcmd, args);
	  goto found_cmd;
        }
      }
    }

    failure = DO_COMMAND_EX;
    goto out;
  }

  /*------------------------------------------------------------*
   * Get the command arguments.  Put them in argv. Buffer args  *
   * stores the arguments, and the pointers in argv point into  *
   * args.							*
   *------------------------------------------------------------*/

 found_cmd:

# ifdef DEBUG
    if(trace_docmd) trace_i(292, newcmd);
# endif

  force_external(newcmd);
  argv[0]   = newcmd;

  arg_avail = args_len + nargs;
  argsp     = args;
  nargs     = 1;
  the_cmd   = remove_indirection(the_cmd);
  while(!IS_NIL(the_cmd)) {
    int l;
    t1 = ast_head(the_cmd);
    copy_str(argsp, t1, arg_avail, &tl);
    if(!IS_NIL(tl)) {
      failure = LIMIT_EX;
      goto out;  /* result is nil */
    }
    argv[nargs++] = argsp;
    l             = strlen(argsp) + 1;
    argsp        += l;
    arg_avail    -= l;
    the_cmd       = remove_indirection(ast_tail(the_cmd));
  }
  argv[nargs] = NULL;

# ifdef DEBUG
    if(trace_docmd) {
      int qq;
      trace_i(293);
      for(qq = 0; qq < nargs; qq++) fprintf(TRACE_FILE,"%s ", argv[qq]);
      fprintf(TRACE_FILE,"\n");
    }
# endif

  /*----------------------------------------------------------*
   * Get the communication pipes. inpipe sends data from this *
   * process to the command, and outpipe sends data from the  *
   * command to this process.				      *
   *----------------------------------------------------------*/

  if(pipe(inpipe) != 0) {

#   ifdef DEBUG
      if(trace) trace_i(254);
#   endif

    failure = NO_FILE_EX;
    failure_as_entity = qwrap(NO_FILE_EX, make_str("pipe"));
    goto out;   /* result is nil */
  }

  if(pipe(outpipe) != 0) {

#   ifdef DEBUG
      if(trace) trace_i(255);
#   endif

    failure = NO_FILE_EX;
    failure_as_entity = qwrap(NO_FILE_EX, make_str("pipe"));
    close(inpipe[0]);
    close(inpipe[1]);
    goto out;  /* result is nil */
  }

  /*----------------------------------------*
   * Fork a process to execute the command. *
   *----------------------------------------*/

  pid = fork();
  if(pid == 0) {

    /*---------------------------------------------------*
     * Child process. This process executes the command. *
     *---------------------------------------------------*/

    /*------------------------------------*
     * Close the other ends of the pipes. *
     *------------------------------------*/

    close(inpipe[1]);
    close(outpipe[0]);

    /*-----------------------------------------------------------*
     * Redirect standard input and standard output to the pipes. *
     *-----------------------------------------------------------*/

    close(0);
    dup(inpipe[0]);
    close(inpipe[0]);
    close(1);
    dup(outpipe[1]);
    close(outpipe[1]);

    /*----------------------*
     * Execute the command. *
     *----------------------*/

    if(execv(argv[0], argv) < 0) {

      /*----------------------------------*
       * If we get here, the exec failed. *
       *----------------------------------*/

#     ifdef DEBUG
	if(trace) trace_i(256, newcmd);
#     endif

      /*----------------------------------*
       * Write "exec failed" to the pipe. *
       *----------------------------------*/

      printf("### %s: exec failed\n", newcmd);
      close(0);
      close(1);
      exit(0);

    }
  }

  else if(pid > 0) {

    /*------------------------------------------------------------------*
     * Parent process. This process continues execution of the program. *
     *------------------------------------------------------------------*/

    /*------------------------------------*
     * Close the other ends of the pipes. *
     *------------------------------------*/

    close(inpipe[0]);
    close(outpipe[1]);

    /*-----------------------------------*
     * Set the pipes for nonblocking io. *
     *-----------------------------------*/

    fcntl(inpipe[1], F_SETFL, O_NONBLOCK);
    fcntl(outpipe[0], F_SETFL, O_NONBLOCK);

    /*----------------------------------------------------*
     * Create the command manager. The command manager is *
     * responsible for copying s to the standard input of *
     * the forked command.				  *
     *----------------------------------------------------*/

    t1      = ast_make_int(pid);
    t2      = ast_make_int(inpipe[1]);
    pair    = ast_pair(t1, t2);
    cmdmann = make_lazy_prim(CMDMAN_TMO, pair, s);

    /*--------------------------------------------------*
     * Create the string that comes out of the command. *
     * This is the result of the doCmd call.		*
     *--------------------------------------------------*/

    result = ast_read_file(outpipe[0], VOLATILE_FM, NULL);

    /*----------------------------------------------------------*
     * Merge the result string with the command manager.  Since *
     * the command manager returns nil, this is really just the *
     * result.  But by merging them, we force the command	*
     * manager to run.						*
     *----------------------------------------------------------*/

    result = make_lazy_prim(MERGE_TMO, result, cmdmann);
    goto out;
  }

  else /* pid < 0 */ {

    /*--------------*
     * Fork failed. *
     *--------------*/

    failure = DO_COMMAND_EX;
    close(inpipe[0]);
    close(inpipe[1]);
    close(outpipe[0]);
    close(outpipe[1]);
    goto out;
  }

 out:

  if(argv != NULL) {
    FREE(argv);
    FREE(args);
    FREE(newcmd);
  }
  unreg(mark);
  return result;
}


/****************************************************************
 *			CMDMAN					*
 ****************************************************************
 * This function is called when evaluating a CMDMAN_TMO lazy	*
 * primitive.							*
 *								*
 * pr is a pair (pid, fd) where pid is the process id of the	*
 * command being run and fd is the file descriptor of the pipe	*
 * that leads to the standard input of the command.  Copy s to	*
 * fd, and then wait for process pid to terminate.  Return nil.	*
 ****************************************************************/

ENTITY cmdman(ENTITY pr, ENTITY s, LONG *l_time)
{
  char buf;
  int fd, pid, status;
  ENTITY evs, result, h, localpr;
  int time_step_count = TIME_STEP_COUNT_INIT;
  REG_TYPE mark = reg3(&evs, &h, &localpr);

  /*---------------------------*
   * Get the pipe to write to. *
   *---------------------------*/

  fd = get_ival(ast_tail(pr), 0);

  /*--------------------------------------------------*
   * evs is that part of s that remains to be copied. *
   *--------------------------------------------------*/

  evs = s;
  localpr = pr;

  while(failure < 0) {

    /*-------------------------*
     * Get a character of evs. *
     *-------------------------*/

    IN_PLACE_EVAL_FAILTO(evs, l_time, fail1);  
    goto got_evs;

  fail1:
    if(failure == TIME_OUT_EX) goto out;
    evs = nil;

  got_evs:

    /*------------------------------------------------*
     * If evs is exhausted, close the pipe and        *
     * wait for the command process to terminate.     *
     * It is possible for fd to have been closed      *
     * on a previous round, so don't close if fd = 0. *
     *------------------------------------------------*/

    if(ENT_EQ(evs, nil)) {
      if(fd != 0) {
#       ifdef DEBUG
	  if(trace_docmd) trace_i(294, fd);
#       endif
	close(fd);
      }
      h       = ast_head(localpr);
      pid     = get_ival(h, 0);
      localpr = ast_pair(h, zero);

      /*-------------------------------------------------------*
       * Check if the command process is already done.  If so, *
       * return nil.  If not, then return a command manager    *
       * that will continue to wait, and time-out.	       *
       *-------------------------------------------------------*/

      if(waitpid(pid, &status, WNOHANG) == 0) {
#       ifdef DEBUG
	  if(trace_docmd) trace_i(295);
#       endif
	num_repauses++;
	*l_time = 0;
	failure = TIME_OUT_EX;
	goto out;
      }
      unreg(mark);
#     ifdef DEBUG
        if(trace_docmd) trace_i(296);
#     endif
      return nil;
    }

    /*----------------------------------------------------------*
     * If evs is not nil, then copy a character to the command  *
     * process.							*
     *----------------------------------------------------------*/

    else {
      buf = CHVAL(remove_indirection(ast_head(evs)));
#     ifdef DEBUG
        if(trace_docmd) trace_i(297, *l_time, buf);
#     endif
      if(write(fd, &buf, 1) <= 0) {

	/*--------------------------------------------------*
	 * The write blocked. Return a command manager that *
         * will continue the copy when reactivated.         *
	 *--------------------------------------------------*/

#       ifdef DEBUG
	  if(trace_docmd) trace_i(298);
#       endif
	*l_time = 0;
	failure = TIME_OUT_EX;
	goto out;
      }

      /*----------------------------------------------*
       * Possibly time-out, if a long copy operation. *
       *----------------------------------------------*/

      MAYBE_TIME_STEP(l_time);
    }
    evs = ast_tail(evs);
  } /* end while(failure < 0) */

 out:
  result = make_lazy_prim(CMDMAN_TMO, localpr, evs);

# ifdef DEBUG
    if(trace_docmd) {
      trace_i(299);
      trace_print_entity(result);
    }
# endif

  unreg(mark);
  return result;
}

#endif


/****************************************************************
 *			DATE_STDF				*
 ****************************************************************
 * Return the date and time, as a string entity.		*
 ****************************************************************/

ENTITY date_stdf(ENTITY herm_unused)
{
  time_t tloc;
  char *s;

  time(&tloc);
  s = ctime(&tloc);
  s[strlen(s)-1] = '\0';
  return make_str(s);
}


/****************************************************************
 *			SECONDS_STDF				*
 ****************************************************************
 * Return the seconds since the beginning of time, as an	*
 * entity.							*
 ****************************************************************/

ENTITY seconds_stdf(ENTITY herm_unused)
{
  time_t tloc;

  time(&tloc);
  return ast_make_int(tloc);
}


/****************************************************************
 *			FSTAT_STDF				*
 ****************************************************************
 * Implement basicFileStatus function.				*
 ****************************************************************/

ENTITY fstat_stdf(ENTITY name_as_ent)
{
# ifdef SMALL_STACK
    char* name = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char name[MAX_FILE_NAME_LENGTH + 1];
# endif

  struct stat stat_buf;
  ENTITY a,owner,group,exec_perm,write_perm,read_perm,mod_time,kind,size,
	 modes,result;
  int uid, gid, executable, writable, readable;

  copy_str(name, name_as_ent, MAX_FILE_NAME_LENGTH, &a);
  if(ENT_NE(a, nil)) {failure = LIMIT_EX; result = nil; goto out;}

  if(stat_file(name, &stat_buf) != 0) {
    failure = NO_FILE_EX;
    failure_as_entity = qwrap(NO_FILE_EX, name_as_ent);
    result = nil;
    goto out;
  }

# ifdef UNIX
    uid = geteuid();
    gid = getegid();
    if(uid == stat_buf.st_uid) {
      executable = stat_buf.st_mode & S_IXUSR;
      writable   = stat_buf.st_mode & S_IWUSR;
      readable   = stat_buf.st_mode & S_IRUSR;
    }
    else if(gid == stat_buf.st_gid) {
      executable = stat_buf.st_mode & S_IXGRP;
      writable   = stat_buf.st_mode & S_IWGRP;
      readable   = stat_buf.st_mode & S_IRGRP;
    }
    else {
      executable = stat_buf.st_mode & S_IXOTH;
      writable   = stat_buf.st_mode & S_IWOTH;
      readable   = stat_buf.st_mode & S_IROTH;
    }
#endif

#ifdef MSWIN
  executable = stat_buf.st_mode & S_IEXEC;
  writable   = stat_buf.st_mode & S_IWRITE;
  readable   = stat_buf.st_mode & S_IREAD;
#endif

  /*-----------------------------------------------------------------------*
   * Build (kind,size,mtime,(read-perm,write-perm,exec-perm),owner,group). *
   *-----------------------------------------------------------------------*/

  owner      = ast_make_int(stat_buf.st_uid);
  group      = ast_make_int(stat_buf.st_gid);
  exec_perm  = ast_make_int(executable != 0);
  write_perm = ast_make_int(writable != 0);
  read_perm  = ast_make_int(readable != 0);
  mod_time   = ast_make_int(stat_buf.st_mtime);
  kind       = ast_make_int(S_ISDIR(stat_buf.st_mode));
  size       = ast_make_int(stat_buf.st_size);

  modes = ast_pair(read_perm, exec_perm);
  modes = ast_pair(write_perm, modes);
  result = ast_pair(owner, group);
  result = ast_pair(modes, result);
  result = ast_pair(mod_time, result);
  result = ast_pair(size, result);
  result = ast_pair(kind, result);

 out:
# ifdef SMALL_STACK
    FREE(name);
# endif
  return result;
}


/****************************************************************
 *	   RM_STDF, RMDIR_STDF, MKDIR_STDF, RENAME_STDF		*
 ****************************************************************
 * Implement remove-file, remove-directory, make-directory and  *
 * rename-file functions.					*
 ****************************************************************/

PRIVATE ENTITY sys_call(int (*f)(const char *), ENTITY s)
{
  ENTITY a;
# ifdef SMALL_STACK
    char* name = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char name[MAX_FILE_NAME_LENGTH + 1];
# endif

  copy_str(name, s, MAX_FILE_NAME_LENGTH, &a);
  if(!IS_NIL(a)) {failure = LIMIT_EX; goto out;}

  if(failure >= 0) goto out;

  if(f(name) != 0) {
    failure = NO_FILE_EX;
    failure_as_entity = qwrap(NO_FILE_EX, s);
  }

 out:
  return hermit;
}

/*-------------------------------------------------------*/

ENTITY rm_stdf   (ENTITY s) {return sys_call(unlink_file, s);}
ENTITY rmdir_stdf(ENTITY s) {return sys_call(rmdir_file, s);}
ENTITY mkdir_stdf(ENTITY s) {return sys_call(mkdir_file, s);}
ENTITY chdir_stdf(ENTITY s) {return sys_call(chdir_file, s);}

/*-------------------------------------------------------*/

ENTITY rename_stdf(ENTITY from, ENTITY to)
{
  ENTITY a, b;
# ifdef SMALL_STACK
    char* name1 = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
    char* name2 = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char name1[MAX_FILE_NAME_LENGTH + 1], 
         name2[MAX_FILE_NAME_LENGTH + 1];
# endif

  copy_str(name1, from, MAX_FILE_NAME_LENGTH, &a);
  copy_str(name2, to, MAX_FILE_NAME_LENGTH, &b);
  if(!IS_NIL(a) || !IS_NIL(b)) {failure = LIMIT_EX; goto out;}
  if(failure >= 0) goto out;

  if(rename_file(name1,name2) != 0) {
    failure = NO_FILE_EX;
    failure_as_entity = qwrap(NO_FILE_EX, from);
  }

 out:
# ifdef SMALL_STACK
    FREE(name1);
    FREE(name2);
# endif
  return hermit;
}


/*********************************************************
 *			DIRLIST_STDF			 *
 *********************************************************
 * Return a list of the files in directory d, in an	 *
 * arbitrary order.  If d is not a directory, fail with  *
 * exception NO_FILE_EX(d).  d must be fully evaluated.	 *
 *********************************************************/

ENTITY dirlist_stdf(ENTITY d)
{
# ifdef SMALL_STACK
    char* dir_name = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char dir_name[MAX_FILE_NAME_LENGTH + 1];
# endif
  ENTITY rest, result, filename;
  DIR* dfd;
  struct dirent* diren;

  copy_str(dir_name, d, MAX_FILE_NAME_LENGTH, &rest);
  if(ENT_NE(rest, nil)) goto fail;

  dfd = opendir(dir_name);
  if(dfd == NULL) goto fail;

  result = nil;
  do {
    diren = readdir(dfd);
    if(diren != NULL) {
      filename = make_str(diren->d_name);
      result   = ast_pair(filename, result);
    }
  } while (diren != NULL);

  closedir(dfd);
  goto out;

 fail:
  failure = NO_FILE_EX;
  failure_as_entity = qwrap(NO_FILE_EX, d);
  result = nil;

 out:

# ifdef SMALL_STACK
    FREE(dir_name);
# endif

  return result;
}


/*********************************************************
 *			GETCWD_STDF			 *
 *********************************************************
 * Return the current working directory.		 *
 *********************************************************/

ENTITY getcwd_stdf(ENTITY herm_unused)
{
  ENTITY result;
# ifdef SMALL_STACK
    char* dir_name = (char*) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
# else
    char dir_name[MAX_FILE_NAME_LENGTH + 1];
# endif

# ifdef MSWIN
    result = make_str(full_file_name(getcwd(dir_name, MAX_FILE_NAME_LENGTH), 1));
# else
    result = make_str(getcwd(dir_name, MAX_FILE_NAME_LENGTH));
# endif

# ifdef SMALL_STACK
    FREE(dir_name);
# endif

  return result;
}


/*********************************************************
 *			OS_ENV_STDF			 *
 *********************************************************
 * Return the OS environment.				 *
 *********************************************************/

ENTITY os_env_stdf(ENTITY herm_unused)
{
  char *p;
  ENTITY name, val;

  int    i      = 0;
  ENTITY result = nil;

  while(os_environment[i] != NULL) {
    p = strchr(os_environment[i], '=');
    if(p != NULL) {
      name   = make_strn(os_environment[i], p - os_environment[i]);
      val    = make_str(p+1);
      name   = ast_pair(name, val);
      result = ast_pair(name, result);
    }
    i++;
  }

  return result;
}


