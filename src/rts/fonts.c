/*********************************************************************
 * File:    rts/fonts.c
 * Purpose: Implement fonts.
 * Author:  Karl Abrahamson
 *********************************************************************/

#include <string.h>
#ifdef USE_MALLOC_H
#  include <malloc.h>
#else
#  include <stdlib.h>
#endif
#include "../misc/misc.h"
#include <windows.h>
#include "../alloc/allocate.h"
#include "../rts/rts.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../tables/tables.h"
#include "../win/font.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
extern HWND MainWindowHandle;

/****************************************************************
 *			PUBLIC VARIABLES			*
 ****************************************************************/

/**********************************************************************
 * font_table is an array of structures that describe open fonts.     *
 * file-entities (tag FILE_TAG) that represent fonts contain a	      *
 * pointer to a structure of type struct file_entity with kind	      *
 * FONT_FK.  That structure describes some aspects of the font, such  *
 * as the name of the font.  That structure in turn contains the      *
 * index of one of the structures in font_table, which gives	      *
 * information about the open font.				      *
 *								      *
 * Other than in this file, font_table is used only in the garbage    *
 * collector to free fonts.					      *
 **********************************************************************/

struct fontd font_table[MAX_OPEN_FONTS];

/****************************************************************
 *			PRIVATE VARIABLES			*
 ****************************************************************/

/****************************************************************
 * Font stamps are used to indicate use times.  next_font_stamp *
 * is the next number to stamp a font with.  It is incremented  *
 * each time it is used.					*
 ****************************************************************/

PRIVATE LONG next_font_stamp = 0;


/****************************************************************
 *			FONT_FAIL				*
 ****************************************************************
 * Set failure for failing while opening a font with given 	*
 * file name, font name and height in points.			*
 ****************************************************************/

PRIVATE void font_fail(char *filename, char *fontname, LONG pointHeight)
{
  char* arg = (char*) BAREMALLOC(strlen(filename) + strlen(fontname) + 34);

  sprintf(arg, "(file:%s, font:%s, height:%ld)",
	  filename, fontname, pointHeight);
  failure = NO_FONT_EX;
  failure_as_entity = qwrap(NO_FONT_EX, make_str(arg));
  FREE(arg);
}


/****************************************************************
 *			NEW_FONTD				*
 ****************************************************************
 * Enter font entity e into the font table.  Return the index	*
 * in the font table where it was entered.			*
 *								*
 * Note that a font entity is described by a structure of type  *
 * struct file_entity.						*
 ****************************************************************/

PRIVATE int new_fontd(struct file_entity *e)
{
  LONG stmin, st;
  int i, minpos, result;
  struct fontd *fontd;

  /*------------------------------------------------------------------*
   * Try to find an empty record.  This loop sets result to the index *
   * found.  If no index is found, it sets minpos to the index of the *
   * least recently used font descriptor, and stmin to the time-stamp *
   * stored with index.						      *
   *------------------------------------------------------------------*/

  stmin  = LONG_MAX;
  minpos = -1;
  for(i = 0; i < MAX_OPEN_FONTS; i++) {

    /*-----------------------------------------------------------------*
     * If the i-th entry has a handle field of -1, then that entry is  *
     * vacant, so use it.					       *
     *-----------------------------------------------------------------*/

    if(font_table[i].handle == -1) {result = i; goto out;}

    /*-----------------------------------*
     * Otherwise, update the time stamp. *
     *-----------------------------------*/

    st = font_table[i].stamp;
    if(st < stmin) {
      stmin  = st;
      minpos = i;
    }
  }

  /*------------------------------------------------------------*
   * There is no empty record.  Replace the least-recently used *
   * entry. 							*
   *------------------------------------------------------------*/

  result = minpos;
  fontd = font_table + minpos;

  /*--------------------------------------------------------------------*
   * The font record (to which a file entity refers) has the index of   *
   * this fontd record in it.  We need to set that index to -1, 	*
   * indicating that the font entity has no associated fontd descriptor.*
   *--------------------------------------------------------------------*/

  fontd->font_rec->descr_index = -1;

  /*------------------------------------------------------------------*
   * Close the font that we are replacing, and clear out this record. *
   *------------------------------------------------------------------*/

  closeFont(fontd->font_rec->u.font_data.fileName, (HFONT) fontd->handle);
  fontd->handle = -1;

 out:
  fontd = font_table + result;
  fontd->stamp = next_font_stamp++;
  fontd->font_rec = e;
  return result;
}


/****************************************************************
 *			GET_FONTD				*
 ****************************************************************
 * Return a struct fontd for font record r.  Fails with		*
 * exception NO_FONT_EX, and returns NULL, if there is no such  *
 * font.				                        *
 ****************************************************************/

PRIVATE struct fontd *get_fontd(struct file_entity *r)
{
  int kind = r->kind;
  int index;
  HFONT handle;
  struct fontd *result;

# ifdef GCTEST
    if(r->mark != 0) badrc("font record", r->mark, (char *) r);
# endif

  if(kind == NO_FILE_FK) {
    failure = CLOSED_FILE_EX;
    return NULL;
  }

  /*------------------------------------------------------------*
   * If this font already has an active index, then return the  *
   * font descriptor at that index.  Stamp this entry as most   *
   * recently used.  If it does not have an active index, then  *
   * get one.							*
   *------------------------------------------------------------*/

  index = r->descr_index;
  if(index >= 0) {
    result = font_table + index;
    result->stamp = next_font_stamp++;
    if(result->handle != -1) return result;
  }
  else {
    index = new_fontd(r);
    result = font_table + index;
  }

  /*-------------------------------------------------------------*
   * Open the font. Install in the file table if successful, and *
   * fail if not successful.					 *
   *-------------------------------------------------------------*/

  handle = openFont(MainWindowHandle, r->u.font_data.fileName,
		    r->u.font_data.fontName, r->u.font_data.pointHeight);

  if(handle == NULL) {
    font_fail(r->u.font_data.fileName, r->u.font_data.fontName,
	      r->u.font_data.pointHeight);
    return NULL;
  }
  else {
    result->handle = handle;
    return result;
  }
}


/****************************************************************
 *			OPEN_FONT_STDF				*
 ****************************************************************
 * Return a font with given file name, font name and height.	*
 ****************************************************************/

ENTITY open_font_stdf(ENTITY entFilename, ENTITY entFontname,
		      ENTITY entPointHeight)
{
  struct file_entity *fent;
  struct fontd *fontd;
  char* filename = (char *) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
  char* fontname = (char *) BAREMALLOC(MAX_FILE_NAME_LENGTH + 1);
  ENTITY rest1, rest2, result;
  int i;
  LONG pointHeight;

  /*-------------------------*
   * Convert the parameters. *
   *-------------------------*/

  copy_str(filename, entFilename, MAX_FILE_NAME_LENGTH, &rest1);
  copy_str(fontname, entFontname, MAX_FILE_NAME_LENGTH, &rest2);
  pointHeight = get_ival(entPointHeight, TEST_EX);
  if (failure == TEST_EX || pointHeight > INT_MAX
      || TAG(rest1) != NOREF_TAG || TAG(rest2) != NOREF_TAG) {
    if(failure == TEST_EX) pointHeight = LONG_MAX;
    font_fail(filename, fontname, pointHeight);
    result = zero;
    goto out;
  }

  /*--------------------------------------------------*
   * If this font is already in the table, return it. *
   *--------------------------------------------------*/

  for(i = 0; i < MAX_OPEN_FONTS; i++) {
    fontd = font_table + i;
    if(fontd->handle > 0) {
      fent = fontd->font_rec;
      if(fent != NULL && strcmp(fent->u.font_data.fileName, filename) == 0
	 && fent->u.font_data.pointHeight == pointHeight) {
	result =ENTP(FILE_TAG, fent);
	goto out;
      }
    }
  }

  /*---------------------------------------------------------*
   * If this font is not in the table, make an entry for it. *
   *---------------------------------------------------------*/

  fent 				= alloc_file_entity();
  fent->kind                    = FONT_FK;
  fent->u.font_data.fileName    = filename;
  fent->u.font_data.fontName    = fontname;
  fent->u.font_data.pointHeight = toint(pointHeight);
  fontd = get_fontd(fent);

  /*-----------------------------------------------------------*
   * Build the entry, if found.  Note that, above, the strings *
   * were allocated as temporaries.  Make them permanent now.  *
   *-----------------------------------------------------------*/

  if(fontd != NULL) {
    fent->u.font_data.fileName = make_perm_str(filename);
    fent->u.font_data.fontName = make_perm_str(fontname);
    FREE(filename);
    FREE(fontname);
    result = ENTP(FILE_TAG, fent);
  }
  else result = zero;

 out:
  FREE(filename);
  FREE(fontname);
  return zero;
}


/****************************************************************
 *			CLOSE_FONT_STDF				*
 ****************************************************************
 * Close font f.						*
 ****************************************************************/

ENTITY close_font_stdf(ENTITY f)
{
  struct file_entity *fent;
  struct fontd *fontd;
  int index;

  if(TAG(f) != FILE_TAG) die(158);
  fent = FILEENT_VAL(f);
  index = fent->descr_index;
  if(index >= 0) {
    HFONT handle = (HFONT) font_table[index].handle;
    closeFont(fent->u.font_data.fileName, handle);
    font_table[index].handle = -1;
    font_table[index].font_rec = NULL;
  }
  fent->kind = NO_FILE_FK;
  fent->u.font_data.fileName = NULL;
  fent->u.font_data.fontName = NULL;
  return hermit;
}


/****************************************************************
 *			CLOSE_ALL_OPEN_FONTS			*
 ****************************************************************
 * Close all fonts in font_table.				*
 ****************************************************************/

void close_all_open_fonts(void)
{
  int i;

  for(i = 0; i < MAX_OPEN_FONTS; i++) {
    int h = font_table[i].handle;
    if(h > 0) {
      struct file_entity* fent = font_table[i].font_rec;
      if(fent != NULL) {
	closeFont(fent->u.font_data.fileName, (HFONT) h);
	font_table[i].handle = -1;
      }
    }
  }
}


/****************************************************************
 *			INIT_FONTS				*
 ****************************************************************
 * This needs to be called during interpreter initialization to *
 * initialize the font table.					*
 ****************************************************************/

void init_fonts(void)
{
  int i;

  for(i = 0; i < MAX_OPEN_FONTS; i++) {
    font_table[i].handle = -1;
  }
}


