/*******************************************************
 * File:    prof.c
 * Purpose: Get information on where files are located.
 * Author:  Karl Abrahamson
 ********************************************************/

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

/**************************************************************
 * This file is only used in the MSWIN version.  It extracts  *
 * profile information on directories, etc., from the file    *
 * astarte.ini.						      *
 **************************************************************/

#include "../misc/options.h"
#define STRICT
#include <stdio.h>
#include <windows.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include <string.h>

/****************************************************************
 * STD_DIR holds the directory where the standard library is    *
 *         put.							*
 *								*
 * MESSAGE_DIR holds the directory where the message files,     *
 *             such as messages.txt, are put.			*
 *								*
 * DEBUG_MSG_DIR holds the directory where debug message files, *
 *               such as dmsgt.txt, are put.			*
 *								*
 * BIN_DIR holds the directory where the binaries astc.exe and  *
 *         astr.exe are put.					*
 ****************************************************************/

char* MESSAGE_DIR 	= NULL;
char* DEBUG_MSG_DIR 	= NULL;
char* STD_DIR 		= NULL;
char* BIN_DIR 		= NULL;

char* ConsoleFontName 	= NULL;
char* ConsoleFontFile 	= NULL;
int   ConsoleFontSize;

/****************************************************************
 * Read profile information from the Astarte section of file    *
 * astarte.ini.							*
 ****************************************************************/

PRIVATE char sect[]          = "Astarte";
PRIVATE char ini_file_name[] = "Astarte.ini";

/****************************************************************
 *			GET_PSTRING				*
 ****************************************************************
 * Look up entry in the .ini file, and set *where to it, after  *
 * copying into the heap.					*
 ****************************************************************/

PRIVATE void get_pstring(char **where, char *entry, char *dflt)
{
  char s[129];
  GetPrivateProfileString(sect, entry, dflt, s, 128, ini_file_name);
  *where = strdup(s);
}

/****************************************************************
 *			GET_PROFILE				*
 ****************************************************************
 * Read all profile information from the .ini file.		*
 ****************************************************************/

void get_profile(void)
{
  get_pstring(&MESSAGE_DIR, "Message", "C:\ASTARTE\BIN");
  get_pstring(&DEBUG_MSG_DIR, "Debug", "C:\ASTARTE\BIN");
  get_pstring(&STD_DIR, "Standard", "C:\ASTARTE\AST");
  get_pstring(&BIN_DIR, "Exe", "C:\ASTARTE\BIN");
  get_pstring(&ConsoleFontName, "Font", "Courier New");
  get_pstring(&ConsoleFontFile, "FontFile", "C:\WINDOWS\COUR.FOT");
  {char s[13];
   GetPrivateProfileString(sect, "FontSize", "17", s, 12, ini_file_name);
   sscanf(s, "%d", &ConsoleFontSize);
  }
}


/****************************************************************
 *			FREE_PROFILE_STRINGS			*
 ****************************************************************
 * Free all of the strings that were allocated while reading	*
 * profile information.						*
 ****************************************************************/

void free_profile_strings()
{
  if(MESSAGE_DIR != NULL) {
    free(MESSAGE_DIR);
    free(DEBUG_MSG_DIR);
    free(STD_DIR);
    free(BIN_DIR);
    free(ConsoleFontName);
    free(ConsoleFontFile);
  }
}



