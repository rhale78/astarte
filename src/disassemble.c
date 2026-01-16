/****************************************************************
 * File:    disassemble.c
 * Purpose: Print a .aso file in symbolic (readable) form.
 * Author:  Karl Abrahamson
 ***************************************************************/

#define DISASSEMBLE
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#define STR_SIZE 1000
#define CODED_INT_BIAS 0x800000

/* Get the information about instructions. */

#include "evaluate/instinfo.c"

static FILE *infile;
static int indef;
static int trace;
static int global_num;

static FILE *infile;
static int indef;
static int trace;

/********************************************************
 *		       BARE_MALLOC			*
 ********************************************************
 * Same as MALLOC, but die if result is NULL.  		*
 ********************************************************/

void* bare_malloc(SIZE_T n)
{
  register void* result = MALLOC(n);
  if(result == NULL) exit(1);
  return result;
}

/****************************************************************
 *			FGETS1					*
 ****************************************************************
 * Read a null-terminated string from file f, and put it into	*
 * buffer s, which has n+1 bytes.				*
 ****************************************************************/

void fgets1(char *s, int n, FILE *f)
{
  register char *p;
  register int i, ch;

  p = s;
  i = 0;
  while(i < n && (ch = getc(f)) != 0) {
    *(p++) = ch;
    i++;
  }
  *p = '\0';
}


/****************************************************************
 *			FGETUC					*
 ****************************************************************
 * fgetuc(f) reads a single byte from file f and returns it.	*
 ****************************************************************/

#ifdef DEBUG
int fgetuc(FILE *f)
{
  int r = (unsigned) (getc(f));
  if(trace) printf("GET %d\n", r);
  return r;
}
#else
#define fgetuc(f) getc(f)
#endif

/****************************************************************
 *			THREE_BYTES				*
 ****************************************************************
 * Read and return a three byte integer from infile.		*
 ****************************************************************/

long three_bytes()
{
  long l,m,h,r;

  l = fgetuc(infile);
  m = fgetuc(infile);
  h = fgetuc(infile);
  r = l + (m << 8) + (h << 16) - CODED_INT_BIAS;
  if(trace) printf("GOT INT %d\n", r);
  return r;
}


/*******************************************************************
 *			INIT_ERR_MSGS				   *
 *******************************************************************
 * Read a message file named file_name, having num_lines lines.    *
 * *which should be set to point to the allocated array.	   *
 *								   *
 * The messages file consists of lines where the first four	   *
 * characters of each line are ignored, and are intended to hold   *
 * the line number.  Lines whose first character is #, and	   *
 * empty lines, are ignored.   If there are not four characters    *
 * in the line, the line is considered to be a null message.       *
 *******************************************************************/

void init_err_msgs(char ***which, char *file_name, int num_lines)
{
  int c;
  unsigned int buf_size, i, j, k;
  struct stat stat_buf;
  char *buf;

  /*----------------*
   * Open the file. *
   *----------------*/

  FILE *f = fopen(file_name, TEXT_READ_OPEN);
  if(f == NULL) {
    fprintf(stderr, "Canot read file %s\n", file_name);
    exit(1);
  }

  /*-----------------------------------------------------------------*
   * Allocate two arrays.  *which is an array of char* values, where *
   * (*which[i]) points to the i-th line of the file.  buf is a      *
   *  buffer where the characters of the lines are stored. 	     *
   *-----------------------------------------------------------------*/

  fstat(fileno(f), &stat_buf);
  buf_size = (unsigned int)(stat_buf.st_size - 4*num_lines + 2);
  buf = (char *) malloc(buf_size);
  *which = (char **) malloc(num_lines * sizeof(char *));

  /*------------------------------------------------------------*
   * Copy the lines from the file into the buffer, and set the  *
   * pointers in *which.					*
   *------------------------------------------------------------*/

  j = k = c = 0;
  for(;;) {
    c = getc(f);
    if(c == EOF) break;

    /*---------------------------------------------------*
     * Ignore lines that start with '#' and empty lines. *
     *---------------------------------------------------*/

    if(c == '#' || c == '\n') {
      while (c != '\n' && c != EOF) c = getc(f);
      continue;
    }

    /*-----------------------------------------------------------------*
     * Install the pointer to the current buffer position into *which. *
     *-----------------------------------------------------------------*/

    (*which)[k] = buf + j;

    /*------------------------------------------------------------------*
     * Skip over the first four characters of the line.  We have done   *
     * one read, above, so do three more.  If there are not four	*
     * characters, make a null entry.				        *
     *------------------------------------------------------------------*/

    for(i = 0; i < 3; i++) {
      if(getc(f) == '\n') goto null_terminate_the_line;
    }

    /*--------------------------------*
     * Copy the line into buffer buf. *
     *--------------------------------*/

    for(;;) {
      c = getc(f);
      if(c == '\n' || c == EOF) break;
      if(c == '\\') c = '\n';
      buf[j++] = c;
    }

 null_terminate_the_line:
    buf[j++] = '\0';

    if(j > buf_size || k >= num_lines) {
      fclose(f);
      fprintf(stderr, "File %s is too large\n", file_name);
    }
    k++;
  }
  fclose(f);
}


/****************************************************************
 *			DCL					*
 ****************************************************************
 * Read a sequence of declarations.				*
 ****************************************************************/

dcl()
{
  long a, b, c, d;
  char s[STR_SIZE];

  for(;;) {
    printf("---------------------\n");
    global_num = 0;
    c = fgetuc(infile);
    switch(c) {
      case EOF:
	printf("EOF\n");
	return;

      case STOP_PACKAGE_I:
	printf("STOP_PACKAGE_I\n");
	return;

      case ID_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("ID_DCL_I          %s\n", s);
	break;

      case NAME_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("NAME_DCL_I          %s\n", s);
	break;

      case LABEL_DCL_I:
	a = fgetuc(infile);
	printf("LABEL_DCL_I                            >%ld\n", a);
	break;

      case LONG_LABEL_DCL_I:
	a = three_bytes();
	printf("LONG_LABEL_DCL                         >%ld\n", a);
	break;

      case BEGIN_IMPLEMENTATION_DCL_I:
        printf("BEGIN_IMPLEMENTATION_DCL_I\n");
	break;

      case STRING_DCL_I:
	a = three_bytes();
	fgets1(s,STR_SIZE,infile);
	printf("STRING_DCL_I      [%ld] %s\n", a, s);
	break;

      case INT_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("INT_DCL_I         %s\n", s);
	break;

      case REAL_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("REAL_DCL_I        %s\n", s);
	break;

      case SPECIES_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("SPECIES_DCL_I        %s\n", s);
	break;

      case NEW_SPECIES_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("NEW_SPECIES_DCL_I        %s\n", s);
	break;

      case FAMILY_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("FAMILY_DCL_I      %s\n", s);
	break;

      case NEW_TRANSPARENT_FAMILY_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("NEW_TRANSPARENT_FAMILY_DCL_I      %s\n", s);
	break;

      case NEW_OPAQUE_FAMILY_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("NEW_OPAQUE_FAMILY_DCL_I      %s\n", s);
	break;

      case GENUS_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("GENUS_DCL_I     %s\n", s);
	break;

      case NEW_GENUS_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("NEW_GENUS_DCL_I     %s\n", s);
	break;

      case COMMUNITY_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("COMMUNITY_DCL_I   %s\n", s);
	break;

      case NEW_TRANSPARENT_COMMUNITY_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("NEW_TRANSPARENT_COMMUNITY_DCL_I   %s\n", s);
	break;

      case NEW_OPAQUE_COMMUNITY_DCL_I:
	fgets1(s,STR_SIZE,infile);
	printf("NEW_OPAQUE_COMMUNITY_DCL_I   %s\n", s);
	break;

      case RELATE_DCL_I:
	a = three_bytes();
	b = three_bytes();
	printf("RELATE_DCL_I      >%ld >%ld\n", a, b);
	break;

      case MEET_DCL_I:
        a = three_bytes();
        b = three_bytes();
        c = three_bytes();
        printf("MEET_DCL_I        >%ld >%ld >%ld\n", a, b, c);
	break;

      case BEGIN_EXTENSION_DCL_I:
        printf("BEGIN_EXTENSION_DCL_I\n");
        break;

      case END_EXTENSION_DCL_I:
        printf("END_EXTENSION_DCL_I\n");
        break;

      case IMPORT_I:
	fgets1(s,STR_SIZE,infile);
	printf("IMPORT_I          %s\n", s);
	break;

      case EXECUTE_I:
	a = fgetuc(infile);
	printf("EXECUTE_I         *%ld\n", a);
	executable(0);
	break;

      case HIDDEN_EXECUTE_I:
	a = fgetuc(infile);
	printf("HIDDEN_EXECUTE_I         *%ld\n", a);
	executable(0);
	break;

      case EXCEPTION_DCL_I:
        {long descr_len;
         a = fgetuc(infile);
	 fgets1(s,STR_SIZE,infile);
	 printf("EXCEPTION_DCL_I  (%ld) %s\n", a, s);
	 descr_len = three_bytes();
	 if(descr_len == 0) printf("No description\n");
	 else {
	   c = fgetuc(infile);
	   while(c != 0 && c != EOF) {
	     printf("%c", (int) c);
	     c = fgetuc(infile);
	   }
	   printf("\n");
	 }
	 executable(1);  /* type instrs */
	 break;
        }

      case IRREGULAR_DCL_I:
	a = three_bytes();
	printf("IRREGULAR_DCL_I  >%ld\n", a);
	executable(1);
	break;

      case DEFINE_DCL_I:
	a = three_bytes();
	b = fgetuc(infile);
	printf("DEFINE_DCL_I      >%ld mode %ld\n", a, b);
	executable(0);
	break;

      case DEFAULT_DCL_I:
        a = three_bytes();
        b = fgetuc(infile);
        printf("DEFAULT_DCL_I     >%ld closed: %ld\n", a, b);
        executable(1);
	break;

      case AHEAD_MEET_DCL_I:
        a = three_bytes();
        b = three_bytes();
        c = three_bytes();
        printf("AHEAD_MEET_DCL_I        >%ld >%ld >%ld\n", a, b, c);
	break;

      default:
	printf("?????(%ld)\n", c);
    }
  }
}


/****************************************************************
 *			EXECUTABLE				*
 ****************************************************************
 * Read a sequence of executable instructions.  If 		*
 * stop_at_endlet is true, stop at an END_LET_I instruction.	*
 * Otherwise, stop at and END_I instruction.			*
 ****************************************************************/

executable(int stop_at_endlet)
{
  long a, b, c;
  char *name;

  for(;;) {
    c = fgetuc(infile);
    name = (c <= LAST_NORMAL_INSTRUCTION) ? instr_name[c] :
               (c == END_LET_I) ? "END-LET" :
    	       (c == END_I)     ? "--END" :
	       (c == ENTER_I)   ? "--ENTER"
		 		: "UNKNOWN";
    printf("%-15s", name);
    switch(instinfo[c].class) {
      case END_INST:
        printf("\n");
	return;
    
      case END_LET_INST:
        if(stop_at_endlet) {printf("\n"); return;}
        if(indef) {
	  a = fgetuc(infile);
	  printf(" *%ld", a);
        }
        break;

      case PREF_INST:
        switch(c) {
          case UNARY_PREF_I:
	    a = fgetuc(infile);
            if(a <= N_UNARIES && a > 0) printf(" %s", un_instr_name[a]);
            else printf(" UNKNOWN %ld", a);
	    break;
	
          case BINARY_PREF_I:
	    a = fgetuc(infile);
            if(a <= N_BINARIES && a > 0) printf(" %s", bin_instr_name[a]);
            else printf(" UNKNOWN %ld", a);
	    break;

          case LIST1_PREF_I:
 	    a = fgetuc(infile);
            if(a <= N_LISTONES && a > 0) printf(" %s", list1_instr_name[a]);
            else printf(" UNKNOWN %ld", a);
	    break;

          case LIST2_PREF_I:
	    a = fgetuc(infile);
            if(a <= N_LISTTWOS && a > 0) printf(" %s", list2_instr_name[a]);
            else printf(" UNKNOWN %ld", a);
	    break;

          case UNK_PREF_I:
	    a = fgetuc(infile);
            if(a <= N_UNK_STDFS && a > 0) printf(" %s", unk_instr_name[a]);
            else printf(" UNKNOWN %ld", a);
	    break;

        }
        break;

      case TY_PREF_INST:
	a = fgetuc(infile);
        if(a <= N_TY_STDFS) printf(" %s", ty_instr_name[a]);
        else printf(" UNKNOWN %ld", a);
        a = fgetuc(infile);
        printf(" %ld", a);
	break;

      case NO_PARAM_INST:
      case NO_TYPE_INST:
	if(c == TYPE_ONLY_I) printf("  [%d]", global_num++);
	break;

      case LLABEL_INST:
	a = fgetuc(infile);
	printf("             :%ld", a);
	break;

      case RELET_INST:
      case BYTE_PARAM_INST:
      case BYTE_TYPE_INST:
      case LONG_LLABEL_INST:
	a = fgetuc(infile);
	printf(" %ld", a);
	break;

      case LINE_INST:
      case LONG_NUM_PARAM_INST:
        a = three_bytes();
        printf(" %ld", a);
        break;

      case ENTER_INST:
      case STOP_G_INST:
	a = fgetuc(infile);
	printf(" *%ld", a);
	break;

      case LLABEL_PARAM_INST:
	a = fgetuc(infile);
	printf(" :%ld", a);
	break;

      case TWO_BYTE_PARAMS_INST:
      case TWO_BYTE_TYPE_INST:
	a = fgetuc(infile);
	b = fgetuc(infile);
	printf(" %ld  %ld", a, b);
	break;

      case LLABEL_ENV_INST:
	indef = 1;
	a = fgetuc(infile);          /* Local label */
	printf(" :%ld", a);
	break;

#     ifdef NEVER
      case LLABEL_ENV_NUM_INST:
	indef = 1;
        b = fgetuc(infile);	     /* Byte parameter */
	a = fgetuc(infile);          /* Local label    */
	printf(" %ld :%ld", b, a);
	break;
#     endif

      case BYTE_LLABEL_INST:
	b = fgetuc(infile);
	a = fgetuc(infile);
	printf(" %ld  :%ld", a, b);
	break;

      case BYTE_GLABEL_INST:
	a = three_bytes();
        b = fgetuc(infile);
	printf(" >%ld %ld", a, b);
	break;

      case EXC_INST:
      case GLABEL_PARAM_INST:
      case GLABEL_TYPE_INST:
	a = three_bytes();
	printf(" >%ld", a);
	if(c == GET_GLOBAL_I) printf("  [%d]", global_num++);
	break;

      case LET_INST:
	indef = 0;
	a = fgetuc(infile);  /* Offset */
	printf(" %ld   (", a);
	while((a = fgetuc(infile)) != 0) printf("%c", a);
        printf(")");
	break;

      case DEF_INST:
	indef = 1;
	a = fgetuc(infile);
	b = fgetuc(infile);
	printf(" :%ld %ld  (", a, b);
	while((a = fgetuc(infile)) != 0) printf("%c", a);
        printf(")");
	break;

      default: 
	printf("******************************** (%ld)", c);
    }
    printf("\n");
  }
}


/****************************************************************
 *			MAIN					*
 ****************************************************************/

main(int argc, char **argv) 
{
  char s[51], ss[51];
  int b,k;
  
  trace = 0;

  /*-------------------------------------*
   * Process the command line arguments. *
   *-------------------------------------*/

  if(argc < 2 || argc > 3) {
    fprintf(stderr, "usage: disass [-D] file\n");
    return 1;
  }
  k = 1;
  if(argc == 3) {
    k = 2;
    if(strcmp(argv[1], "-D") == 0) trace = 1;
    else {
      fprintf(stderr, "usage: disass [-D] file\n");
      return 1;
    }
  }

  /*-----------------------*
   * Get the file to read. *
   *-----------------------*/

# ifdef MSWIN
    infile = fopen(argv[k], "rb");
# else
    infile = fopen(argv[k], "r");
# endif

  if(infile == NULL) {
    fprintf(stderr, "disass: no such file %s\n", argv[k]);
    return 1;
  }

  /*-------------------*
   * Get the preamble. *
   *-------------------*/

  fgets1(s,40,infile);
  if(strncmp(s, "@(#)Astarte byte code version ", 30) != 0) {
    fprintf(stderr, "disass: bad header (%s)\n", s);
    return 1;
  }
  {int vers, cnt;
   cnt = sscanf(s+30, "%d", &vers);
   if(cnt != 1) {
     fprintf(stderr, "disass: bad header (%s)\n", s);
     return 1;
   }
   if(vers != BYTE_CODE_VERSION) {
     fprintf(stderr, "disass: old version number %d\n", vers);
     return 1;
   }
  }
  fgets1(s, 50, infile);
  fgets1(ss, 50, infile);

  /*---------------------------*
   * Print the output heading. *
   *---------------------------*/

  printf("Disassemble output of file %s, package %s (%s)\nCode version %d\n\n",
	 argv[k], s, ss, BYTE_CODE_VERSION);


  /*-------------------------------------------------------*
   * Read the instruction names and then the declarations. *
   *-------------------------------------------------------*/

  read_instr_names();
  dcl();

  return 0;
}


