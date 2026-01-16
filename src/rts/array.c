/*********************************************************************
 * File:    rts/array.c
 * Purpose: Functions for arrays and packed strings.
 * Author:  Karl Abrahamson
 *********************************************************************/

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
 * This file manages packed strings (tags CSTR_TAG and STRING_TAG) and  *
 * arrays (tag ARRAY_TAG).  The function scanForChar is also            *
 * implemented here.							*
 *									*
 * Here is a description of nodes with tag ARRAY_TAG.			*
 *									*
 * An ARRAY_TAG entity points to a header that describes		*
 * a random-access list or an integer.  The header holds		*
 * two or more values, the first being the length of the		*
 * list (or of the front part of the list),				*
 * the second being the body of the list.  The length			*
 * field must always have tag INT_TAG.  The tag of the			*
 * body value tells what kind of list is being stored.			*
 * 									*
 *  Tag BOX_TAG:							*
 * 									*
 *   There are only two header entried, (l,b).  (b has			*
 *   tag BOX_TAG.)  This list is a list of nonshared			*
 *   boxes b, b+1, ..., b+l-1.						*
 * 									*
 *  Tag PLACE_TAG:							*
 * 									*
 *   There are four entries in the header, (l,p,r,a).			*
 *   (p has tag PLACE_TAG.) This list is a list of			*
 *   shared boxes p, p+1, ..., p+l-1 followed by list r.		*
 * 									*
 *   Field a is used for garbage collection.  It is an array		*
 *   of which p, p+1, ..., p+l-1 is a subarray.				*
 *   That array is marked rather than this one.  If a is nil,		*
 *   then this is not a subarray of any other array.			*
 * 									*
 *  Tag INDIRECT_TAG:							*
 * 									*
 *   There are four entries in the header, (l,s,r,k).  This		*
 *   list is a list of entities stored at consecutive			*
 *   addresses s, s+1, ..., s+l-1, followed by list r.			*
 *   Field k is used for garbage collection.  It is the			*
 *   number of entries beyond the end of this array that		*
 *   need to be marked when this array is marked.			*
 * 									*
 *  Tag STRING_TAG							*
 * 									*
 *   The header is a quadruple of entities (l,s,r,k).  This		*
 *   list consists of the sublist of string s of length l		*
 *   beginning k bytes beyond the beginning of s, followed		*
 *   by list r.								*
 * 									*
 *  Tag ENTITY_TOP_TAG:							*
 * 									*
 *   There are three entries in the header, (l,s,r).  This array	*
 *   is the concatenation of the arrays stored at s, s+1, ...,		*
 *   s+l-1, followed by list r.  Each of the arrays stored at		*
 *   s, s+1, ... must have tag ARRAY_TAG, and each of those arrays	*
 *   must have kind INDIRECT_TAG or PLACE_TAG.				*
 *									*
 *   Each of those arrays must have a follower field of nil.		*
 *   All but the last of s, s+1, ..., s+l-1 must have			*
 *   size exactly ENT_BLOCK_GRAB_SIZE.					*
 * 									*
 ************************************************************************/

#include <string.h>
#include "../misc/misc.h"
#ifdef UNIX
# include <sys/types.h>
# include <sys/unistd.h>
# include <sys/fcntl.h>
#endif
#ifdef MSWIN
# include <io.h>
# include <fcntl.h>
#endif
#include <sys/stat.h>
#include "../utils/lists.h"
#include "../utils/filename.h"
#include "../alloc/allocate.h"
#include "../machdata/entity.h"
#include "../machdata/except.h"
#include "../machstrc/machstrc.h"
#include "../intrprtr/intrprtr.h"
#include "../evaluate/evaluate.h"
#include "../rts/rts.h"
#include "../gc/gc.h"
#include "../show/prtent.h"
#include "../tables/tables.h"
#include "../standard/stdtypes.h"
#include "../classes/classes.h"
#ifdef DEBUG
# include "../debug/debug.h"
#endif
#ifdef GCTEST
# include "../machdata/gc.h"
#endif

PRIVATE ENTITY* copy_to_rack(ENTITY *x, ENTITY aa, LONG n, ENTITY *t, LONG *m);


/***************************************************************
 * Some list functions need to keep track of how long they run *
 * so that they can time out if they run too long.  To let     *
 * these functions run reasonably long, the time counter is    *
 * not decremented at each iteration of those functions.       *
 * TIME_STEP_COUNT_INIT is the number of iterations done per   *
 * decrement of the time counter for list functions.           *
 ***************************************************************/

#define TIME_STEP_COUNT_INIT 10


/****************************************************************
 *			SPLIT_STRING				*
 ****************************************************************
 * Split string node l.  Store head(l) in *h and store		*
 * tail(l) ++ rest in *t, controlled by mode as in ast_split1:  *
 *                                                              *
 *   mode	set                                             *
 *   ----	----                                            *
 *    1         *h only                                         *
 *    2         *t only                                         *
 *    3		*h and *t                                       *
 *                                                              *
 * This function MIGHT set *t when mode = 1 and *h when         *
 * mode = 2, but is not required to do so.			*
 *								*
 * l must have tag STRING_TAG.					*
 ****************************************************************/

void split_string(ENTITY l, ENTITY *h, ENTITY *t, ENTITY rest, int mode)
{
  CHUNKPTR chunk = BIVAL(l);
  LONG     len   = STRING_SIZE(chunk);
  charptr  buff  = STRING_BUFF(chunk);

  *h = ENTCH(*buff);
  if(mode & 2) {
    if(len == 1) *t = rest;
    else {
      ENTITY* hd = allocate_entity(4);
      hd[0] = ENTI(len - 1);
      hd[1] = l;
      hd[2] = rest;
      hd[3] = ENTI(1);
      *t = ENTP(ARRAY_TAG, hd);
    }
  }
}


/****************************************************************
 *			SPLIT_CSTRING				*
 ****************************************************************
 * Split string node l, storing the head in h and the tail in	*
 * t, controlled by mode as in ast_split1 and split_string.     *
 *								*
 * l must have tag CSTR_TAG.					*
 ****************************************************************/

void split_cstring(ENTITY l, ENTITY *h, ENTITY *t, int mode)
{
  charptr buff = CSTRVAL(l);
  *h = ENTCH(*buff);
  if(mode & 2) {
    if(buff[1] == 0) *t = nil;
    else *t = make_str(buff + 1);
  }
}


/****************************************************************
 *			SPLIT_ARRAY				*
 ****************************************************************
 * Split array node l.   Store head(l) in *h, and store 	*
 * tail(l) ++ rest in *t.					*
 *								*
 * This function is controlled by mode as in ast_split1 and     *
 * split_string.					        *
 *								*
 * l must have that ARRAY_TAG.					*
 *								*
 * Helper function mem_offset returns the entity that is 	*
 * pointed to by bdy_p, as a member of an array with tag	*
 * bdy_tag, which should be either INDIRECT_TAG or PLACE_TAG.   *
 * (If bdy_tag is INDIRECT_TAG, this is just the entity pointed *
 * to by bdy_p.  But if bdy_tag is PLACE_TAG, then bdy_p is	*
 * thought of as pointing to a shared box, and so we need to	*
 * build a box.)						*
 ****************************************************************/

PRIVATE ENTITY mem_offset(int bdy_tag, ENTITY* bdy_p)
{
  return (bdy_tag == INDIRECT_TAG) 
           ? *bdy_p 
           : ENTP(PLACE_TAG, bdy_p);
}

/*-------------------------------------------------------------*/

void split_array(ENTITY l, ENTITY *h, ENTITY *t, ENTITY rest, int mode)
{
  ENTITY *hd, *bp, bdy;
  int bdy_tag;
  LONG len;

  hd      = ENTVAL(l);
  len     = IVAL(hd[0]);
  bdy     = hd[1];
  bdy_tag = TAG(bdy);

  switch(bdy_tag) {
    case BOX_TAG:

      /*---------------------------------------------------------*
       * The head of the box array is just the first box, which  *
       * is just bdy.  The tail is obtained be decrementing the  *
       * length and incrementing the first box number, unless	 *
       * the tail is nil.                       		 *
       *---------------------------------------------------------*/

      *h = bdy;
      if(mode & 2) {
	if(len == 1) *t = rest;
	else {
	  bp    = allocate_entity(2);
	  bp[0] = ENTI(len - 1);
	  bp[1] = ENT_ADD(bdy, 1);
	  *t    = ENTP(ARRAY_TAG, bp);
	  if(!IS_NIL(rest)) *t = quick_append(*t, rest);
	}
      }
      return;

    case INDIRECT_TAG:
    case PLACE_TAG:

      /*----------------------------------------------------------------*
       * The head of a place array is just the first place, which       *
       * is the same as bdy.  The head of a normal entity array         *
       * is what is pointed to by bdy.                                  *
       *                                                                *
       * The tail of a place or normal array is obtained by             *
       * decrementing the length and incrementing the start.            *
       * In the special case where there is only one thing in           *
       * the array, the tail is given by the follower, in hd[2].	*
       *----------------------------------------------------------------*/

      {ENTITY* bdy_p = ENTVAL(bdy);
       *h = (bdy_tag == INDIRECT_TAG) ? *bdy_p : bdy;
       if(mode & 2) {
         ENTITY follow = tree_append(hd[2], rest);
	 if(len == 1) *t = follow;
         else if(len == 2) {
           *t = ast_pair(mem_offset(bdy_tag, bdy_p + 1), follow);
	 }
         else if(len == 3) {
           *t = ast_triple(mem_offset(bdy_tag, bdy_p + 1),
			   mem_offset(bdy_tag, bdy_p + 2),
			   follow);
	 }
	 else {
	  bp    = allocate_entity((len == 4) ? 4 : 8);
          bp[0] = mem_offset(bdy_tag, bdy_p + 1);
          bp[1] = mem_offset(bdy_tag, bdy_p + 2);
          bp[2] = mem_offset(bdy_tag, bdy_p + 3);
          if(len == 4) bp[3] = follow;
          else {
            bp[3] = ENTP(ARRAY_TAG, bp + 4);
	    bp[4] = ENTI(len - 4);
            bp[5] = ENTP(bdy_tag, bdy_p + 4);
	    bp[6] = follow;
	    bp[7] = hd[3];
          }
	  *t = ENTP(QUAD_TAG, bp);
	}
      }
      return;
     }

    case STRING_TAG:

      /*----------------------------------------------------------------*
       * The head of a string array is just the character in the        *
       * buffer at the appropriate offset (hd[3]) from the start.       *
       *                                                                *
       * The tail of a string array is obtained by adding one to        *
       * the offset and subtracting one from the length, except         *
       * in the special case where the buffer has only one              *
       * character in it, in which case the tail is given by            *
       * the follower, in hd[2].					*
       *----------------------------------------------------------------*/

      {char* buff   = STRING_BUFF(BIVAL(bdy));
       int   offset = IVAL(hd[3]);
       *h = ENTCH(buff[offset]);
       if(mode & 2) {
         ENTITY follow = tree_append(hd[2], rest);
	 if(len == 1) *t = follow;
         else if(len == 2) {
           *t = ast_pair(ENTCH(buff[offset+1]), follow);
         }
         else if(len == 3) {
	   *t = ast_triple(ENTCH(buff[offset + 1]), 
			   ENTCH(buff[offset + 2]), 
			   follow);
	 }
	 else {
	   bp    = allocate_entity((len == 4) ? 4 : 8);
           bp[0] = ENTCH(buff[offset + 1]);
           bp[1] = ENTCH(buff[offset + 2]);
           bp[2] = ENTCH(buff[offset + 3]);
	   if(len == 4) bp[3] = follow;
           else {
             bp[3] = ENTP(ARRAY_TAG, bp + 4);
	     bp[4] = ENTI(len - 4);
	     bp[5] = hd[1];
	     bp[6] = follow;
	     bp[7] = ENTI(offset + 4);
           }
	   *t = ENTP(QUAD_TAG, bp);
	 }
       }
       return;
      }

    case ENTITY_TOP_TAG:

      /*---------------------------------------------------------*
       * The head of an ENTITY_TOP_TAG array is the head of the  *
       * first item in the top level array.                      *
       *                                                         *
       * The tail of an ENTITY_TOP_TAG array is the tail of the  *
       * first item followed by the remaining items.  In the     *
       * special case where the first item has only one member,  *
       * the tail of the ENTITY_TOP_TAG array consist only       *
       * of the remaining items.  If there is only one item,     *
       * then the tail is the follower.				 *
       *---------------------------------------------------------*/

      {ENTITY* bdy_p = ENTVAL(bdy);
       ENTITY tt;

       /*-----------------------------------------------------*
        * If the tail is not requested, it suffices to get    *
	* the head of bdy_p[0].				      *
	*-----------------------------------------------------*/

       if(!(mode & 2)) {
         split_array(bdy_p[0], h, &tt, nil, mode);
       }
       else {

	 /*-----------------------------------------------------*
	  * We will split bdy_p[0], the first of the 		*
          * concatentated arrays.  But first, build what	*
	  * follows the tail of bdy_p[0].			*
	  *-----------------------------------------------------*/

	 ENTITY follow_first_item;
	 ENTITY follow = tree_append(hd[2], rest);

	 if(len == 1) follow_first_item = follow;
	 else if(len == 2) follow_first_item = quick_append(bdy_p[1], follow);
	 else {
	   bp    = allocate_entity(3);
	   bp[0] = ENTI(len - 1);
#          ifdef SMALL_ENTITIES
	     bp[1] = ENT_ADD(bdy, 1);
#          else
	     bp[1] = ENT_ADD(bdy, sizeof(ENTITY));
#          endif
	   bp[2] = follow;
	   follow_first_item = ENTP(ARRAY_TAG, bp);
	 }

	 /*-----------------------------------------------------*
	  * Now split bdy_p[0], installing follow_first_item as *
          * its follower.					*
	  *-----------------------------------------------------*/

         split_array(bdy_p[0], h, t, follow_first_item, mode);
       }
       return;
      }
  } /* end switch */
}


/****************************************************************
 *			ARRAY_LENGTH				*
 ****************************************************************
 * Add an amount k to *offset, and return a list x, such that	*
 * the length of x plus k is the length of l.			*
 * List x is shorter than l.  List l must have tag ARRAY_TAG.	*
 ****************************************************************/

ENTITY array_length(ENTITY l, LONG *offset)
{
  ENTITY* hd  = ENTVAL(l);
  LONG len    = IVAL(hd[0]);
  int bdy_tag = TAG(hd[1]);

  if(bdy_tag == ENTITY_TOP_TAG) {
    ENTITY* q = ENTVAL(hd[1]);
    *offset += (len-1)*ENT_BLOCK_GRAB_SIZE;
    *offset += IVAL(ENTVAL(q[len-1])[0]);
    if(*offset < 0) { /* overflow check */
      failure = LIMIT_EX;
      return nil;
    }
    return hd[2];
  }

  else {
    *offset += len;
    if(*offset < 0) { /* overflow check */
      failure = LIMIT_EX;
      return nil;
    }
    if(bdy_tag == BOX_TAG) return nil;
    else return hd[2];
  }
}


/****************************************************************
 *			TOP_LEVEL_SUBLIST			*
 ****************************************************************
 * top_level_sublist returns a sublist or subscript of the 	*
 * array whose body is an ENTITY_TOP_TAG entity with pointer	*
 * bdy_p, whose length field holds len and whose follower is    *
 * follow.							*
 *								*
 * If subscript is false, then the sublist starts at offset i,  *
 * and has length n.  If n is LONG_MAX, get a suffix at offset	*
 * i.								*
 *								*
 * If subscript is true, then a subscript (list member) is	*
 * desired, with index i, numbering from 0.  In this case, n 	*
 * must be 1.							*
 ****************************************************************/

PRIVATE ENTITY
top_level_sublist(ENTITY* bdy_p, LONG len, ENTITY follow, 
		  LONG i, LONG n, LONG *time_bound, Boolean subscript)
{

  /*------------------------------------------------------------*
   * Each of the concatenated arrays except the last has  	*
   * size ENT_BLOCK_GRAB_SIZE.				   	*
   *							   	*
   * Each item in the array is itself an array, with a 		*
   * body of kind INDIRECT_TAG or PLACE_TAG.                    *
   *                                                            *
   *  last_p     points to the array header of the last item.   *
   *  last_len   is the length of the last item.                *
   *  total_len  is the length of all of the items              *
   *		 concatenated together.		   	 	*
   *------------------------------------------------------------*/

  ENTITY* last_p    = ENTVAL(bdy_p[len-1]);
  LONG    last_len  = IVAL(last_p[0]);
  LONG    total_len = (len-1)*ENT_BLOCK_GRAB_SIZE + last_len;
  LONG start, start_i;
  ENTITY *start_p, *start_bdy_p;

  /*------------------------------------------------------------*
   * If i is too large, then we are taking a subscript or	*
   * sublist of follow.						*
   *------------------------------------------------------------*/

  if(i >= total_len) {
    return ast_sublist1(follow, i - total_len, n, time_bound, 0, subscript);
  }

  /*------------------------------------------------------------*
   * The item that contains the desired index (for 		*
   * subscripting) or that contains the start of the sublist	*
   * is bdy_p[start].                                    	*
   *                                                     	*
   * start_p     points to the array header for the start item	*
   * start_bdy_p points to the content of the start item.	*
   * start_i     is the index in the start item where the 	*
   *		    subscript or sublist begin is found.	*
   *------------------------------------------------------------*/

  start       = i/ENT_BLOCK_GRAB_SIZE;
  start_p     = ENTVAL(bdy_p[start]);
  start_bdy_p = ENTVAL(start_p[1]);
  start_i     = i - start*ENT_BLOCK_GRAB_SIZE;
  if(subscript) {
    return start_bdy_p[start_i];
  }
  else {
    LONG start_len = IVAL(start_p[0]);
    LONG start_extract_len;
    ENTITY* new_head;

    /*--------------------------------------------------*
     * Get the part of the start item that begins the	*
     * result.	 The parts of new_head are		*
     *							*
     *  new_head[0]: the length of the sublist of the	*
     *               start item.	   		*
     *  new_head[1]: the array content (pointing into	*
     *		      the array of the start item).	*
     *  new_head[2]: what follows the sublist of the 	*
     *		      start item (set below).		*
     *  new_head[3]: number of places after the end	*
     * 	             of the start item sublist to mark 	*
     *		     during garbage collection.	        *
     *--------------------------------------------------*/

    start_extract_len = (n == LONG_MAX || start_i + n > start_len)
			   ? start_len - start_i
			   : n;
    new_head    = allocate_entity(4);
    new_head[0] = ENTI(start_extract_len);
    new_head[1] = ENTP(TAG(start_p[1]), start_bdy_p + start_i);
    new_head[3] = ENTI(IVAL(start_p[3])
			+ (start_len - (start_i + start_extract_len)));

    /*---------------------------------------------------*
     * Now get what comes after this array from the      *
     * remaining items, and install it into new_head[2]. *
     * The cases are					 *
     *   1. The entire result is contained inside the	 *
     *      start part, and nothing comes after it.	 *
     *   2. The entire top level after the start item	 *
     *      follows the start item sublist.		 *
     *   3. Part of the top level array must follow	 *
     *      the start item sublist.			 *
     *---------------------------------------------------*/

    if(start_extract_len == n) {	/* Case 1 */
      new_head[2] = nil;
    }

    else if(n >= total_len - i) {	/* Case 2 */
      ENTITY* new_top_p = allocate_entity(3);
      new_top_p[0] = ENTI(len - start - 1);
      new_top_p[1] = ENTP(ENTITY_TOP_TAG, bdy_p + start + 1);
      new_top_p[2] = ast_sublist1(follow, 0, n - (total_len - i), 
				  time_bound, 0, 0);
      new_head[2]  = ENTP(ARRAY_TAG, new_top_p);
    }

    else {				/* Case 3 */
      LONG new_top_ent_len = n - start_extract_len;
      LONG new_top_len = (new_top_ent_len - 1)/ENT_BLOCK_GRAB_SIZE + 1;

      /*------------------------------------------------------*
       * All but the last item in the follower is full.  Get  *
       * the last item, which might be partial.	      	      *
       * last_item_p points to the array header of the item   *
       * that needs to be made partial.		       	      *
       * new_last_p points to the (partial) copy header.      *
       *------------------------------------------------------*/

      ENTITY* last_item_p  = ENTVAL(bdy_p[start + new_top_len]);
      ENTITY* new_last_p   = allocate_entity(4);
      LONG    new_last_len = new_top_ent_len
			    - (new_top_len - 1) * ENT_BLOCK_GRAB_SIZE;
      new_last_p[0] = ENTI(new_last_len);
      new_last_p[1] = last_item_p[1];
      new_last_p[2] = nil;
      new_last_p[3] = ENTI(IVAL(last_item_p[3]) +
			    (IVAL(last_item_p[0]) - new_last_len));

      /*------------------------------------------------------*
       * Now get follower for the new_head header.  If        *
       * new_top_len = 1, then the follower is just the array *
       * whose header is new_last_p.  Otherwise, the follower *
       * is a top level array.				      *
       *------------------------------------------------------*/

      if(new_top_len == 1) {
	new_head[2] = ENTP(ARRAY_TAG, new_last_p);
      }
      else {
	ENTITY* new_top_p     = allocate_entity(3);
	ENTITY* new_top_bdy_p = allocate_entity(new_top_len);
	LONG k;
	for(k = new_top_len - 2; k >= 0; k--) {
	  new_top_bdy_p[k] = bdy_p[start + k + 1];
	}
	new_top_bdy_p[new_top_len - 1] = ENTP(ARRAY_TAG, new_last_p);
	new_top_p[0] = ENTI(new_top_len);
	new_top_p[1] = ENTP(ENTITY_TOP_TAG, new_top_bdy_p);
	new_top_p[2] = nil;
	new_head[2]  = ENTP(ARRAY_TAG, new_top_p);
      }
    } /* end case 3 */

    return ENTP(ARRAY_TAG, new_head);

  } /* end else(!subscript) */
}


/****************************************************************
 *			ARRAY_SUBLIST				*
 ****************************************************************
 * array_sublist returns a sublist or subscript of array arr,	*
 * which must have tag ARRAY_TAG.				*
 *								*
 * If subscript is false, then a sublist is desired.  The 	*
 * sublist starts at offset i, and has length n.  If n is	*
 * LONG_MAX, get a suffix at offset i.				*
 *								*
 * If subscript is true, then a subscript (list member) is	*
 * desired, with index i, numbering from 0.  In this case, n 	*
 * must be 1.							*
 ****************************************************************/

ENTITY array_sublist(ENTITY arr, LONG i, LONG n, LONG *time_bound,
		     Boolean subscript)
{
  ENTITY *ap, *bp, bdy;
  LONG len, extract_len;
  int bdy_tag;

  /*-------------------------------------------------------*
   * To take a sublist of an array, we need to build a new *
   * array node, with an updated start and length.	   *
   * We must also take into account the list that follows  *
   * the array.						   *
   *							   *
   * Subscripting into an array is a little simpler.  We   *
   * just extract a member of the array.		   *
   *-------------------------------------------------------*/

  ap      = ENTVAL(arr);
  len     = IVAL(ap[0]);
  bdy     = ap[1];
  bdy_tag = TAG(bdy);

  /*-------------------------------------------------------*
   * Top level arrays (bdy_tag = ENTITY_TOP_TAG) are quite *
   * different from the others.  Handle them separately.   *
   *-------------------------------------------------------*/

  if(bdy_tag == ENTITY_TOP_TAG) {
    return top_level_sublist(ENTVAL(bdy), len, ap[2], i, n, 
			     time_bound, subscript);
  }

  /*-------------------------------------------------------*
   * If there are at most i things in the array, then take *
   * a sublist or subscript of what follows the array,	   *
   * if anything. 					   *
   *-------------------------------------------------------*/

  if(i >= len) {
    if(bdy_tag == BOX_TAG) {
      if(subscript) {
	failure = DOMAIN_EX;
	failure_as_entity = qwrap(DOMAIN_EX,
				  make_str("subscript out of bounds"));
      }
      return nil;
    }
    else {
      TIME_STEP(time_bound);
      return ast_sublist1(ap[2], i - len, n, time_bound, FALSE, subscript);
    }
  }

  /*------------------------------------------------------------*
   * If i < len, then at least part of the sublist or subscript *
   * is from this array itself.  Get that part.  If there is	*
   * more from the follower, get that too.			*
   *------------------------------------------------------------*/

  /*-----------------------------------------------------*
   * Case of subscripting.  Get a member of the array. 	 *
   * The test above will cover an index that is too	 *
   * large, so no test for that needs to be done here.	 *
   *-----------------------------------------------------*/

  if(subscript) {
    switch(bdy_tag) {
      case BOX_TAG:
	return ENT_ADD(bdy, i);

      case INDIRECT_TAG:
	return ENTVAL(bdy)[i];

      case PLACE_TAG:
#	ifdef SMALL_ENTITIES
	  return ENT_ADD(bdy, i);
#	else
	  return ENT_ADD(bdy, i*sizeof(ENTITY));
#	endif

      case STRING_TAG:
	return ENTCH(STRING_BUFF(BIVAL(bdy))[IVAL(ap[3]) + i]);

      default: 
	die(134, bdy_tag);
	return nil;
    }
  }

  else /* !subscript */ {

    /*----------------------------------------------------------*
     * Get that part of the array itself that is desired.  What *
     * follows the array is dealt with below.			*
     *								*
     * First, decrease the desired length to what is actually	*
     * available. 						*
     *----------------------------------------------------------*/

    if(n == LONG_MAX || n + i > len) extract_len = len - i;
    else extract_len = n;

    /*-------------------------------------------------------------*
     * The new array header node will have two cells for a BOX_TAG *
     * array, and four cells for other kinds of array headers.     *
     * We need to bump the start up by i, except in the case of    *
     * a PLACE_TAG or INDIRECT_TAG array in the large-entity       *
     * representation, where we need to bump up by 		   *
     * i*sizeof(ENTITY). 					   *
     *-------------------------------------------------------------*/

    switch(bdy_tag) {

      case BOX_TAG:
	bp = allocate_entity(2);
	bp[0] = ENTI(extract_len);
	bp[1] = ENT_ADD(bdy, i);
	return ENTP(ARRAY_TAG, bp);

      case PLACE_TAG:
      case INDIRECT_TAG:
	bp = allocate_entity(4);
	bp[0] = ENTI(extract_len);
#	ifndef SMALL_ENTITIES
	  bp[1] = ENT_ADD(bdy, i*sizeof(ENTITY));
#       else
	  bp[1] = ENT_ADD(bdy, i);
#	endif
	bp[3] = (bdy_tag == PLACE_TAG)
		  ? ap[3]
		  : ENTI(IVAL(ap[3]) + (len - extract_len - i));
	break;

      case STRING_TAG:
	bp    = allocate_entity(4);
	bp[0] = ENTI(extract_len);
	bp[1] = ap[1];
	bp[3] = ENTI(i + IVAL(ap[3]));
	break;

     default:
	die(134, bdy_tag);
    }

    /*--------------------------------------------------*
     * Now get the sublist of what follows this array.  *
     * It should be installed into bp[2].		*
     *--------------------------------------------------*/

    bp[2] = nil;
    if(n > extract_len && !IS_NIL(ap[2])) {
      ENTITY      result  = ENTP(ARRAY_TAG, bp);
      REG_TYPE    mark    = reg1_param(&result);
      REGPTR_TYPE ptrmark = reg1_ptrparam(&bp);

      LONG rest_len = (n == LONG_MAX) ? LONG_MAX : n - extract_len;
      bp[2] = ast_sublist1(ap[2], 0, rest_len, time_bound, FALSE, FALSE);
      unreg(mark);
      unregptr(ptrmark);
      return result;
    }
    else {
      return ENTP(ARRAY_TAG, bp);
    }

  } /* end else(!subscript) */
}


/********************************************************
 *			AST_ARRAY			*
 ********************************************************
 * Return an array of 'a' empty nonshared boxes.  'a' 	*
 * must be fully evaluated, and must be a nonnegative	*
 * integer.						*
 ********************************************************/

ENTITY ast_array(ENTITY a)
{
  LONG n;
  ENTITY *hd;

  n = get_ival(a, LARGE_ARRAY_EX);
  if(failure >= 0) return nil;

  if(n <= 0) {
    if(n < 0) {
      failure = CONVERSION_EX; 
    }
    return nil;
  }

  hd    = allocate_entity(2);
  hd[0] = a;
  hd[1] = ENTB(new_boxes(n));
  return ENTP(ARRAY_TAG, hd);
}


/********************************************************
 *			SHORT_PLACE_ARRAY		*
 ********************************************************
 * Return an array of n empty shared boxes.  n must be  *
 * positive and no larger than ENT_BLOCK_GRAB_SIZE.     *
 ********************************************************/

PRIVATE ENTITY short_place_array(LONG n)
{
  ENTITY *hd, *p;
  int i;

  hd = allocate_entity(4);
  p  = allocate_entity(toint(n));
  for(i = 0; i < n; i++) p[i] = NOTHING;
  hd[0] = ENTI(n);
  hd[1] = ENTP(PLACE_TAG, p);
  hd[2] = nil;
  return hd[3] = ENTP(ARRAY_TAG, hd);
}


/********************************************************
 *			LONG_PLACE_ARRAY		*
 ********************************************************
 * Return an array of n empty shared boxes.  n can be	*
 * large.						*
 ********************************************************/

PRIVATE ENTITY long_place_array(LONG n)
{
  /*--------------------------------------------------*
   * A small array can be packed into a single block. *
   *--------------------------------------------------*/

  if(n <= ENT_BLOCK_GRAB_SIZE) return short_place_array(n);

  /*-----------------------------------------------*
   * A long array needs to have a top-level index. *
   * If the array is very long, that top level     *
   * index will need to be followed by another.    *
   *-----------------------------------------------*/

  else {
    LONG top_len = (n - 1)/ENT_BLOCK_GRAB_SIZE + 1;
    LONG this_array_n;
    ENTITY follow;

    if(top_len <= ENT_BLOCK_GRAB_SIZE) {
      follow = nil;
      this_array_n = n;
    }
    else {
      top_len      = ENT_BLOCK_GRAB_SIZE;
      this_array_n = ENT_BLOCK_GRAB_SIZE * ENT_BLOCK_GRAB_SIZE;
      follow = long_place_array(n - this_array_n);
    }
 
    {ENTITY* top_p = allocate_entity(top_len);
     ENTITY* hd = allocate_entity(3);
     LONG k, i;

     hd[0] = ENTI(top_len);
     hd[1] = ENTP(ENTITY_TOP_TAG, top_p);
     hd[2] = follow;
     for(i = 0, k = this_array_n;
         i < top_len - 1;
         i++, k -= ENT_BLOCK_GRAB_SIZE) {
       top_p[i] = short_place_array(ENT_BLOCK_GRAB_SIZE);
     }
     top_p[top_len - 1] = short_place_array(k);
     return ENTP(ARRAY_TAG, hd);
    }
  }
}


/********************************************************
 *			AST_PLACE_ARRAY			*
 ********************************************************
 * Return an array of a empty shared boxes.  a must be  *
 * fully evaluated, and must be a nonnegative		*
 * integer.						*
 ********************************************************/

ENTITY ast_place_array(ENTITY a)
{
  LONG n = get_ival(a, LARGE_ARRAY_EX);

  if(failure >= 0) return nil;

  if(n <= 0) {
    if(n < 0) failure = CONVERSION_EX;
    return nil;
  }

  return long_place_array(n);
}


/****************************************************************
 *			AST_PACK_STDF				*
 ****************************************************************
 * Return the result of packing list l.  (What is returned is	*
 * actually a lazy primitive that will perform the pack when	*
 * evaluated.)							*
 ****************************************************************/

ENTITY ast_pack_stdf(ENTITY l)
{
  ENTITY len = make_lazy_prim(LENGTH_TMO, l, zero);
  return  make_lazy_prim(PACK_TMO, l, len);
}


/****************************************************************
 *			ONLY_BYTES				*
 ****************************************************************
 * Return 							*
 *								*
 *    2 if list l has only small int members that are 		*
 *      representable in one unsigned byte, 			*
 *								*
 *    1 if list l has only small int members that are		*
 *      representable in an unsigned short int (and at 		*
 *	least one that is too large for a byte),		*
 *								*
 *    0 if the list has other kinds of members.  		*
 *								*
 * only_bytes evaluates the list structure if necessary, 	*
 * with LONG_MAX time.  Therefore, the list should already	*
 * be evaluated, to prevent spending a long time in this	*
 * function.						  	*
 *								*
 * If any unevaluated members are encountered, only_bytes	*
 * does not evaluate them.  It presumes that they are not	*
 * small integers, and returns 0.				*
 ****************************************************************/

PRIVATE int only_bytes(ENTITY l)
{
  LONG v, l_time = LONG_MAX;
  int result;
  ENTITY a, h, t;
  REG_TYPE mark = reg3(&a, &h, &t);

  result = 2;    /* Decreased to 1 when necessary. */
  a = l;
  for(;;) {
    switch(TAG(a)) {

      case NOREF_TAG:
      case FILE_TAG:
      case STRING_TAG:
      case CSTR_TAG:
	goto out;

      case INDIRECT_TAG:
      case GLOBAL_INDIRECT_TAG:
        a = *ENTVAL(a);
        break;

      case APPEND_TAG:

        /*----------------------------------------------------------*
         * Handle an append by checking the left subtree, and then  *
         * tail-recuring on the right subtree.			    *
	 *							    *
	 * Note: It is important to calculate ENTVAL(a) twice,      *
	 * Since only_bytes can cause a garbage collection, which   *
	 * might relocate ENTVAL(a).				    *
         *----------------------------------------------------------*/

        {int r = only_bytes(ENTVAL(a)[0]); 
         if(r == 0 ) {result = 0; goto out;}
         else if(r == 1) result = 1;
         a = ENTVAL(a)[1];
	 break;
        }

      case ARRAY_TAG:

        {ENTITY* aq = ENTVAL(a);
         int tag = TAG(aq[1]);

	 /*-------------------------------------------------------*
	  * A string has only bytes just when its follower does.  *
          * and it cannot affect the value (1 or 2).		  *
	  * Handle the follower by looping on it.		  *
	  *-------------------------------------------------------*/

	 if(tag == STRING_TAG) a = aq[2];

	 /*-------------------------------------------------------*
	  * A box array never has only bytes.			  *
	  *-------------------------------------------------------*/

	 else if(tag == BOX_TAG || tag == PLACE_TAG) {result = 0; goto out;}

	 /*-------------------------------------------------------*
	  * An ordinary array has only bytes just when all of its *
	  * main members do, and its follower does.		  *
	  * Handle the follower by looping on it.		  *
	  *-------------------------------------------------------*/

         else if(tag == INDIRECT_TAG) {
	   ENTITY* pp = ENTVAL(aq[1]);
	   int len = toint(IVAL(aq[0]));
	   int i;
	   for(i = 0; i < len; i++) {
	     h = pp[i];
	     while(TAG(h) == INDIRECT_TAG) h = *ENTVAL(h);
	     if(TAG(h) != NOREF_TAG) {result = 0; goto out;}
             v = NRVAL(h);
             if(v < 0 || v > USHORT_MAX) {result = 0; goto out;}
             else if(v > UBYTE_MAX) result = 1;
           }
	   a = aq[2];
	 }

	 /*-------------------------------------------------------------*
	  * A large array has only bytes just when each of its 		*
	  * component arrays does, and its follower does.		*
	  * Need to be careful about pointers because it is possible	*
	  * for them to move due to garbage collection.			*
	  *-------------------------------------------------------------*/

	 else /* tag == ENTITY_TOP_TAG */ {
	   int i;
	   int len = IVAL(aq[0]);
	   for(i = 0; i < len; i++) {
	     int r = only_bytes(ENTVAL(ENTVAL(a)[1])[i]);
	     if(r == 0) {result = 0; goto out;}
	     else if(r == 1) result = 1;
	   }
	   a = ENTVAL(a)[2];
	 }
	 break;
        }

      case TREE_TAG:
        if(ENT_EQ(*ENTVAL(a),NOTHING)) goto evaluate; /* default, below. */
	/* Else fall through to general list case. */

      case PAIR_TAG:
      case TRIPLE_TAG:
      case QUAD_TAG:

        /*----------------------------------------------------------*
         * General lists: get the head and tail, check the head and *
         * tail-recur on the tail.				    *
         *----------------------------------------------------------*/

        ast_split(a, &h, &t);
        if(TAG(h) != NOREF_TAG) {
	  h = remove_indirection(h);
	  if(TAG(h) != NOREF_TAG) {result = 0; goto out;}
	}
        v = NRVAL(h);
        if(v < 0 || v > USHORT_MAX) {result = 0; goto out;}
        else if(v > UBYTE_MAX) result = 1;
        a = t;
	break;

      default:
      evaluate:
        IN_PLACE_EVAL(a, &l_time);
	break;

    } /* end switch */
  } /* end for(;;) */

 out:
  unreg(mark);
  return result;
}


/****************************************************************
 *			PACK_STR				*
 ****************************************************************
 * pack_str(l,n,t) packs the first n members of l into bytes,  *
 * returning the result. It sets t to any part that is not 	*
 * copied.		                                        *
 *								*
 * It is required that each member of l is small enough to	*
 * store in one unsigned byte.					*
 ****************************************************************/

PRIVATE ENTITY pack_str(ENTITY l, LONG n, ENTITY *t)
{
  CHUNKPTR chunk;
  char *s;
  LONG chars_copied;

  /*----------------------------------------------*
   * Copy the string into a byte array of size n. *
   *----------------------------------------------*/

  chunk = allocate_binary(n);
  s     = STRING_BUFF(chunk);
  copy_str1(s, l, n, t, &chars_copied, FALSE);

  /*----------------------------------------------*
   * If fewer than n bytes where copied, then	  *
   * copy into a smaller buffer.		  *
   *----------------------------------------------*/

  if(chars_copied != n) {
    CHUNKPTR newchunk = allocate_binary(chars_copied);
    memcpy(STRING_BUFF(newchunk), s, chars_copied);
    free_chunk(chunk);
    return ENTP(STRING_TAG, newchunk);
  }
  else return ENTP(STRING_TAG, chunk);
}


/****************************************************************
 *			SHORT_PACK_NORMAL			*
 ****************************************************************
 * short_pack_normal behaves the same as pack_normal, below,	*
 * but requires that n <= ENT_BLOCK_GRAB_SIZE.			*
 ****************************************************************/

PRIVATE ENTITY short_pack_normal(ENTITY a, LONG n, ENTITY *t)
{
  ENTITY *hd, *p;
  LONG m;
  REGPTR_TYPE ptrmark;

  hd = allocate_entity(4);
  p  = allocate_entity(toint(n));
  ptrmark = reg1_ptrparam(&hd);
  reg1_ptrparam(&p);
  copy_to_rack(p, a, n, t, &m);
  hd[0] = ENTI(m);
  hd[1] = ENTP(INDIRECT_TAG, p);
  hd[2] = nil;
  hd[3] = zero;

  unregptr(ptrmark);
  return ENTP(ARRAY_TAG, hd);
}


/****************************************************************
 *			PACK_NORMAL				*
 ****************************************************************
 * pack_normal(a,n,t) packs the first n members of a into an 	*
 * array of entities, returning the result, and sets t to the   *
 * part of a starting at the (n+1)st member. 			*
 *								*
 * This function is used when list a contains things that 	*
 * cannot be stored in a single byte.				*
 ****************************************************************/

PRIVATE ENTITY pack_normal(ENTITY a, LONG n, ENTITY *t)
{
  /*-------------------------------------------------*
   * It is not worth while to pack very short lists. *
   *-------------------------------------------------*/

  if(n < 3) return a;

  /*--------------------------------------------------*
   * A small array can be packed into a single block. *
   *--------------------------------------------------*/

  if(n <= ENT_BLOCK_GRAB_SIZE) return short_pack_normal(a, n, t);

  /*-----------------------------------------------*
   * A long array needs to have a top-level index, *
   * and needs a follower if it is very long.      *
   *-----------------------------------------------*/

  else {
    LONG top_len = (n - 1)/ENT_BLOCK_GRAB_SIZE + 1;
    LONG this_array_n;

    if(top_len <= ENT_BLOCK_GRAB_SIZE) this_array_n = n;
    else {
      top_len      = ENT_BLOCK_GRAB_SIZE;
      this_array_n = ENT_BLOCK_GRAB_SIZE * ENT_BLOCK_GRAB_SIZE;
    }
     
    {ENTITY* top_p = allocate_entity(top_len);
     ENTITY* hd = allocate_entity(3);
     LONG k, i;
     REGPTR_TYPE ptrmark = reg1_ptrparam(&top_p);
     reg1_ptrparam(&hd);

     *t = a;
     for(i = 0, k = this_array_n;
         i < top_len - 1 && !is_lazy(*t);
         i++, k -= ENT_BLOCK_GRAB_SIZE) {
       top_p[i] = short_pack_normal(*t, ENT_BLOCK_GRAB_SIZE, t);
     }
     if(!is_lazy(*t)) {
       top_p[i] = short_pack_normal(*t, k, t);
       i++;
     }
     hd[0] = ENTI(i);
     hd[1] = ENTP(ENTITY_TOP_TAG, top_p);
     if(this_array_n < n && !is_lazy(*t)) {
       hd[2] = pack_normal(*t, n - this_array_n, t);
     }
     else hd[2] = nil;

     unregptr(ptrmark);
     return ENTP(ARRAY_TAG, hd);
    }
  }
}


/****************************************************************
 *			AST_PACK				*
 ****************************************************************
 * ast_pack(l,len,l_time) returns a packed version of list l.  	*
 * len must be the length of l, but can be a lazy entity.	*
 * This function is timed, so might time-out and return a lazy	*
 * value.							*
 ****************************************************************/

ENTITY ast_pack(ENTITY l, ENTITY len, LONG *l_time)
{
  LONG n;
  int tag, has_only_bytes;
  ENTITY t, result, ll;
  REG_TYPE mark;

  tag = TAG(l);
  ll  = l;
  while(tag == INDIRECT_TAG) {
    ll  = *ENTVAL(ll);
    tag = TAG(ll);
  }

  /*----------------------------------------------------*
   * nil, STRING_TAG and ARRAY_TAG entities are 	*
   * considered packed.  Return them without change.    *
   *----------------------------------------------------*/

  if(tag == NOREF_TAG || tag == STRING_TAG|| tag == ARRAY_TAG) {
    return l;
  }

  /*----------------------------------------------------*
   * A file is considered packed if it is not volatile. *
   *----------------------------------------------------*/

  if(tag == FILE_TAG) {
    if(!IS_VOLATILE_FM(FILEENT_VAL(ll)->mode)) return l;
  }

  mark = reg1(&t);
  reg1_param(&l);

# ifdef DEBUG
    if(trace_extra) {
      trace_i(249);
      trace_print_entity(l);
      trace_i(250);
      trace_print_entity(len);
      tracenl();
    }
# endif

  /*------------------------------------------------------------------*
   * Get the length.  Tell await& expressions to store their results. *
   *------------------------------------------------------------------*/

  should_not_recompute++;
  SET_EVAL(t, len, l_time);
  should_not_recompute--;
  if(failure >= 0) {
    result = make_lazy_prim(PACK_TMO, l, t);
    goto out;
  }
  n = get_ival(t,LIMIT_EX);

  /*------------------------------------------------------------------*
   * Put into an array.  If there are only bytes in l, then we can    *
   * use an array of bytes.  Otherwise, we need an array of entities. *
   *------------------------------------------------------------------*/

  has_only_bytes = only_bytes(l);

# ifdef DEBUG
    if(trace_extra) {
      trace_i(251, n, has_only_bytes);
    }
# endif

  /*------------------------------------------------------------*
   * Currently, shorts are not handled in a special way.  That	*
   * should be changed.						*
   *------------------------------------------------------------*/

  result = (has_only_bytes==2) ? pack_str(l,n,&t) : pack_normal(l,n,&t);

  /*----------------------------------------------------*
   * If anything was not copied, append it to the list. *
   *----------------------------------------------------*/

  {ENTITY rest = t;
   while(TAG(rest) == INDIRECT_TAG) rest = *ENTVAL(rest);
   if(!IS_NIL(rest)) {
     result = quick_append(result, t);
   }
  }

 out:
  unreg(mark);

# ifdef DEBUG
    if(trace_extra) {
      trace_i(252, TAG(result));
      trace_print_entity(result);
      tracenl();
    }
# endif

  return result;
}


/****************************************************************
 *			COPY_TO_RACK				*
 ****************************************************************
 * Copy the first (up to) n members of list aa to location x,	*
 * and return the location in array x just after the copy.	*
 * Set t to the  part of list aa just after the last member	*
 * copied, and  set m = the actual number of members copied	*
 * (which might be less than n, if there were not n members in	*
 * list aa.)							*
 *								*
 * If list aa has a lazy suffix, copy_to_rack will not go into  *
 * that suffix.							*
 ****************************************************************/

PRIVATE ENTITY* copy_to_rack(ENTITY *x, ENTITY aa, LONG n, ENTITY *t, LONG *m)
{
  ENTITY *p, a;
  LONG k,  l_time = LONG_MAX;
  REG_TYPE mark;
  REGPTR_TYPE ptrmark;

  a = aa;
  p = x;
  mark    = reg1_param(&a);
  ptrmark = reg1_ptrparam(&p);

  k = n;
  while(k > 0) {
    switch(TAG(a)) {
      case NOREF_TAG:
        goto exitloop;

      case INDIRECT_TAG:
      case GLOBAL_INDIRECT_TAG:
        a   = *ENTVAL(a);
        break;

      case TREE_TAG:
        if(ENT_EQ(*ENTVAL(a),NOTHING)) goto eval_a;
        /* Else fall through to default case. */

      case PAIR_TAG:
      case TRIPLE_TAG:
      case QUAD_TAG:
      case APPEND_TAG:
      case ARRAY_TAG:
      case STRING_TAG:
      case CSTR_TAG:
      case FILE_TAG:
        ast_split(a,p,&a);
        k--;
        p++;

      default:
      eval_a:
	IN_PLACE_EVAL(a, &l_time);
	break;
    }
  }

exitloop:
  *t = a;
  *m = n - k;
  unreg(mark);
  unregptr(ptrmark);
  return p;
}


/****************************************************************
 *			COPY_STR				*
 *			COPY_STR1				*
 ****************************************************************
 * Copy list aa into array s of bytes, and return s.		*
 * aa must be a list of characters, and should be fully 	*
 * evaluated.  If it is not fully evaluated, it will be         *
 * evaluated, but with a large time bound.			*
 * s will be null-terminated.					*
 *								*
 * At most n characters are copied, not including the null at 	*
 * the end of s. It must be the case that array s has at least  *
 * n+1 bytes.							*
 *								*
 * If any part of list aa is not copied to s, then copy_str	*
 * sets *tl to the suffix of aa that was not copied.  		*
 *								*
 * Copy_str1 is the same as copy_str, but it sets *chars_copied *
 * to the actual number of characters copied. Also, it only     *
 * null-terminates s if term is true.				*
 ****************************************************************/

char* copy_str1(char *s, ENTITY aa, SIZE_T n, ENTITY *tl, 
		LONG* chars_copied, Boolean term)
{
  char *t;
  ENTITY hd, a;
  LONG ccpy = 0, l_time = LONG_MAX;
  REG_TYPE mark = reg2(&hd, &a);

  a   = aa;
  t   = s;
  *tl = nil;
  while (n > 0) {
    int a_tag = TAG(a);
    switch(a_tag) {

      case NOREF_TAG:
        goto exitloop;  /* This is the nil at the end of the string. */

      case INDIRECT_TAG:
      case GLOBAL_INDIRECT_TAG:
        a   = *ENTVAL(a);
        break;

      /*--------------------------------------------*
       * Handle STRING_TAG and CSTR_TAG entities by *
       * copying from the byte arrays.	            *
       *--------------------------------------------*/

      case STRING_TAG:
      case CSTR_TAG:
	{char* a_buff;
	 LONG a_len;
	 LONG ncpy;

         if(a_tag == STRING_TAG) {
           CHUNKPTR chunk = BIVAL(a);
           a_buff = STRING_BUFF(chunk);
           a_len  = STRING_SIZE(chunk);
	 }
	 else {
	   a_buff = CSTRVAL(a);
	   a_len  = strlen(a_buff);
	 }

         ncpy = min(a_len, (LONG)n);
	 ccpy += ncpy;
         while(ncpy > 0) {
	   *(t++) = *(a_buff++);
	   ncpy--;
         }
         if(term) *t = '\0';

         if(a_len > (LONG)n) {
	   *tl = make_strn(a_buff, a_len - n);
         }

	 *chars_copied = ccpy;
	 unreg(mark);
         return s;
        }

      case TREE_TAG:
        if(ENT_EQ(*ENTVAL(a),NOTHING)) goto eval_a;
        /* Else fall through to default case. */

      /*----------------------------------------------------------*
       * Handle the general case by getting the head and tail,    *
       * copying the head, and tail-recurring on the tail.	  *
       *----------------------------------------------------------*/

      case PAIR_TAG:
      case TRIPLE_TAG:
      case QUAD_TAG:
      case APPEND_TAG:
      case ARRAY_TAG:
      case FILE_TAG:
        ast_split(a, &hd, &a);
        *(t++) = CHVAL(remove_indirection(hd));
        ccpy++;
        n--;
	break;

      default:
      eval_a:
	IN_PLACE_EVAL(a, &l_time);
	break;
    }
  } /* end while(n > 0) */

exitloop:

  /*--------------------------------*
   * Null terminate the string.     *
   *--------------------------------*/

  if(term) *t = '\0';

  /*------------------------------------------------------------*
   * If the number of characters to copy maxed out, set *tl to  *
   * that part of aa (which will be in a here) that was not     *
   * copied.							*
   *								*
   * Record number of chars copied. 				*
   *------------------------------------------------------------*/

  if(n == 0) *tl = a;
  *chars_copied = ccpy;

  unreg(mark);
  return s;
}

/*---------------------------------------------------------------*/

char* copy_str(char *s, ENTITY aa, SIZE_T n, ENTITY *tl)
{
  LONG m;
  return copy_str1(s, aa, n, tl, &m, TRUE);
}



/****************************************************************
 *			SCAN_FOR_STDF				*
 ****************************************************************
 * Scan_for_stdf(s, (leadin,what,sense)) scans string s         *
 * for an occurrence of a member or nonmember of bitset what.   *
 * what must be in the form of a dense set, represented by an   *
 * integer.							*
 * -------------------------------------------------------------*
 * If sense is true, scan_for_stdf returns a triple (i,r,p) 	*
 * such that 							*
 *								*
 *   s[i]  is the first member of s that is in set what (where	*
 *	   indexing is from 0),					*
 * 								*
 *   r     is the suffix of s that is obtained by deleting i	*
 *         characters from the start of s.			*
 *								*
 *   p     is s[i-1] if i > 0, and is leadin if i = 0.		*
 *								*
 * If string s contains no member of set what, then the returned*
 * value is (length(s),nil, x) where x is the last member of s  *
 * if s is empty, and x is leadin if s is empty.		*
 *								*
 * -------------------------------------------------------------*
 * If sense is false, scan_for_stdf returns a triple (i,r,p)	*
 * that is similar to the previous case, except that s[i]       *
 * is the first member of s that is NOT in set what.	 	*
 *								*
 * scan_for_stdf just creates a lazy primitive.  		*
 *								*
 * scan_for_help performs the computation by translating the 	*
 * integer what into a bit-vector and then calling 		*
 * scan_for_help1, which does the scan.	 			*
 *								*
 * scan_for_help is called by redo_lazy_prim in 		*
 * evaluate/lazyprim.c.						*
 ****************************************************************/

/****************************************************************
 * scan_for_help1 scans string txt for a member or nonmember 	*
 * of bitset.  sense tells whether it is searching for a member *
 * or a nonmember.  leadin is the character that is presumed to *
 * precede txt.  time limits the time before a time-out is      *
 * done.  							*
 *								*
 * res_cnt, chars_rej, res_suff and res_leadin are 		*
 * out-parameters.  They are set as follows.			*
 *								*
 *    res_cnt:   If res_cnt is positive, then res_cnt-1 is the  *
 *               index i sought in the description of 		*
 *		 scan_for_stdf, of the first member or		*
 *               nonmember of l in s.				*
 *								*
 *		 If res_cnt = 0, then no appropriate character  *
 *		 was found in s.				*
 *								*
 *    chars_rej: The number of characters at the front of s     *
 *               that occur before the first member or nonmember*
 *               of bitset (depending on sense), or until a     *
 *               time-out occurred.				*
 *								*
 *    res_suff:  The suffix of s that begins with the character *
 *               found, or just after the first chars_rej chars *
 * 		 if no character was found.  			*
 *								*
 *    res_leadin: The character that occurs just before the     *
 *                matched character, or the last character of   *
 *                txt if no match is found.			*
 ****************************************************************/

PRIVATE void
scan_for_help1(ENTITY txt, ENTITY the_set, LONG sense, char leadin,
	       LONG *l_time, LONG *res_cnt, LONG *chars_rej,
	       ENTITY *res_suff, char *res_leadin)
{
  ENTITY p, h;
  BIGINT_BUFFER bitset;
  int tag;
  LONG n = 0;		/* Counts characters skipped. */
  int time_step_count = TIME_STEP_COUNT_INIT;
  REG_TYPE mark = reg2(&p, &h);
  reg2_param(&txt, &the_set);

  p = txt;
  for(;;) {

    /*-------------------------------------------------*
     * Make sure the next character of p is available. *
     *-------------------------------------------------*/

    IN_PLACE_EVAL_FAILTO(p, l_time, fail);

    /*---------------------------------------*
     * Every so often we decrement the time. *
     *---------------------------------------*/

    MAYBE_TIME_STEP(l_time);
    if(failure >= 0) goto fail;

    tag = TAG(p);
    switch(tag) {

      /*----------------------------------------------------------------*
       * If there are no more characters in p, then we have reached	*
       * the end of txt.  We should return with res_cnt = 0, since no   *
       * appropriate character of txt was found.  n has been counting   *
       * the number of characters skipped, so it should be returned as  *
       * the value of chars_rej.  There is no more of txt, so res_suff  *
       * is nil.  leadin has been keeping track of the previous 	*
       * character.							*
       *----------------------------------------------------------------*/

      case NOREF_TAG:
	*res_cnt    = 0;
	*chars_rej  = n;
	*res_suff   = nil;
	*res_leadin = leadin;
	unreg(mark);
	return;

      /*------------------------------------------------------------*
       * We want to scan packed strings with reasonable efficiency. *
       *------------------------------------------------------------*/

      case STRING_TAG:
      case CSTR_TAG:
      case ARRAY_TAG:
	{unsigned char *ss;
	 register int i, k;
	 int bitset_tag;
	 ENTITY follower;
	 intcell the_set_space[INTCELLS_IN_LONG];

	 /*--------------------------------------------------*
	  * Get the length k and the address ss of string p. *
          * Also get what follows the string part of p.      *
	  *--------------------------------------------------*/

         if(tag == STRING_TAG) {
	   CHUNKPTR chunk = BIVAL(p);
	   ss       = STRING_BUFF(chunk);
	   k        = STRING_SIZE(chunk);
	   follower = nil;
	 }
	 else if(tag == CSTR_TAG) {
	   ss       = CSTRVAL(p);
	   k        = strlen(ss);
	   follower = nil;
	 }
         else /* tag == ARRAY_TAG */ {

	   /*---------------------------------------------------*
	    * We only try to handle ARRAY_TAG/STRING_TAG	*
	    * entities here.  Other arrays are handled by	*
	    * the general mechanism below.			*
	    *---------------------------------------------------*/

	   ENTITY* hd = ENTVAL(p);
	   if(TAG(hd[1]) != STRING_TAG) goto general;
	   else {
	     CHUNKPTR chunk = BIVAL(hd[1]);
	     ss       = STRING_BUFF(chunk) + IVAL(hd[3]);
	     k        = IVAL(hd[0]);
	     follower = hd[2];
	   }
         }

         /*---------------------------------------------------*
          * Get the set being searched for as a bit array.    *
          *---------------------------------------------------*/

         const_int_to_array(the_set, &bitset, &bitset_tag, the_set_space);

	 /*------------------------------------------------*
	  * Scan ss, looking for an appropriate character. *
	  *------------------------------------------------*/

	 for(i = 0; i < k; i++) {
	   register int chval = *ss;
	   register int select_index = chval >> LOG_INTCELL_BITS;
	   intcell found;
	   if(select_index >= bitset.val.size) {
	     found = FALSE;
	   }
	   else {
	     register intcell
	       select_word = bitset.val.buff[select_index];
	     register intcell
	       mask = (1 << (chval & (INTCELL_BITS - 1)));
	     found = select_word & mask;
	   }

	   /*-------------------------------------------------------------*
	    * If chval is the sought character, then set results as 	  *
	    * follows.						    	  *
	    *    res_cnt: n (chars scanned before starting this string)   *
	    *             + i (chars before chval in this string)	  *
	    *             + 1 (to get one larger than the number skipped.)*
	    *    res_suff: string ss, of length k-i.    		  *
	    *    chars_rej: irrelevant.					  *
	    *    res_leadin: leadin.					  *
	    *-------------------------------------------------------------*/

	   if((found && sense) || (!found && !sense)) {
	     *res_cnt    = n+i+1;
	     *res_leadin = leadin;
	     *res_suff   = ast_sublist1(p, i, LONG_MAX, l_time, TRUE, FALSE);
	     unreg(mark);
	     return;
	   }

	   /*---------------------------------------------------------*
	    * If chval is not the sought char, then try the next one. *
	    * Update leadin, the previous character.		      *
	    *---------------------------------------------------------*/

	   leadin = (char) chval;
	   ss++;
	 }

	 /*-------------------------------------------------------------*
	  * If the loop exits, then we have failed to find the sought 	*
	  * characater.  All k characters in p were rejected, plus the  *
	  * n characters from previous iterations of the loop.		*
          *								*
 	  * Tail recur on the follower.					*
	  *-------------------------------------------------------------*/

	 n += k;
	 p  = follower;
	 break;
	}

      /*----------------------------------------------------------*
       * For any other kind of entity, we just use head and tail. *
       *----------------------------------------------------------*/

      default:
      general:
	{int i;
	 intcell found;
	 intcell the_set_space[INTCELLS_IN_LONG];

	 h = ast_head(p);
	 IN_PLACE_EVAL_FAILTO(h, l_time, fail);
	 i = (int) VAL(remove_indirection(h));

         /*---------------------------------------------------*
          * Get the set being searched for as a bit array.    *
          *---------------------------------------------------*/

         const_int_to_array(the_set, &bitset, &tag, the_set_space);

	 /*---------------------------------------------*
	  * i is the character at the head of the list. *
	  *---------------------------------------------*/

	 {register int select_index = i >> LOG_INTCELL_BITS;
	  if(select_index >= bitset.val.size) {
	    found = FALSE;
	  }
	  else {
	    register intcell
	       select_word = bitset.val.buff[select_index];
	    register intcell
	       mask = (1 << (i & (INTCELL_BITS - 1)));
	    found = select_word & mask;
	  }
	 }

	 /*-------------------------------------------------------------*
	  * If i is the sought character, then set res_cnt to n+1 (one 	*
	  * larger then the number of prior characters) and res_suff to	*
	  * this list (p) that starts with i.				*
	  *-------------------------------------------------------------*/

	 if((found && sense) || (!found && !sense)) {
	   *res_cnt    = n+1;
	   *res_suff   = p;
	   *res_leadin = leadin;
	   unreg(mark);
	   return;
	 }

	 /*-------------------------------------------------------------*
	  * If i is not the sought character, then skip it.  Bump up n  *
	  * to count skipped characters, set p to the list that follows *
	  * i, and go back to the top of the main loop.	 i becomes the  *
          * previous character, leadin.					*
	  *-------------------------------------------------------------*/

	 else {
	   n++;
 	   leadin = (char) i;
	   p      = ast_tail(p);
	 }
       }
    } /* end switch */

  } /* end for */

  /*------------------------------------------------------------------*
   * At a failure or timeout, note that we have rejected n characters *
   * and that the remaining part to read is p.			      *
   *------------------------------------------------------------------*/

 fail:
  *res_cnt    = 0;
  *chars_rej  = n;
  *res_suff   = p;
  *res_leadin = leadin;
  unreg(mark);
  return;
}

/*--------------------------------------------------------------*/

ENTITY scan_for_help(ENTITY txt, ENTITY the_set, ENTITY offset, ENTITY sense,
		     ENTITY leadin, LONG *l_time)
{
  char leadin_char;
  ENTITY p, result, res_suff;
  LONG n, res_cnt, chars_rej;
  REG_TYPE mark = reg2(&p, &res_suff);

  reg2_param(&txt, &offset);
  reg2_param(&sense, &leadin);
  reg1_param(&the_set);

  /*-----------------------------------------------------------------*
   * We need the sense and the set of characters to seek to be       *
   * fully evaluated.  Since sense is Boolean, eval will necessarily *
   * fully evaluate it.						     *
   *-----------------------------------------------------------------*/

  IN_PLACE_FULL_EVAL_FAILTO(the_set, l_time, fail1);
  IN_PLACE_EVAL_FAILTO(sense, l_time, fail1);
  IN_PLACE_EVAL_FAILTO(leadin, l_time, fail1);
  goto got_all;

  /*------------------------------------------------------------*
   * If evaluation failed or timed out, build a lazy primitive  *
   * again.							*
   *------------------------------------------------------------*/

 fail1:
  p      = ast_quad(offset, leadin, the_set, sense);
  result = make_lazy_prim(SCAN_FOR_TMO, txt, p);
  unreg(mark);
  return result;

 got_all:

  /*----------------------------------------------------*
   * Get the offset from prior attempts to do the scan. *
   *----------------------------------------------------*/

  n = get_ival(offset, 0);

  /*--------------*
   * Do the scan. *
   *--------------*/

  scan_for_help1(txt, the_set, VAL(remove_indirection(sense)), 
		 CHVAL(remove_indirection(leadin)), l_time,
		 &res_cnt, &chars_rej, &res_suff, &leadin_char);

  /*--------------------------------------------------------------------*
   * If the scan succeeded, then get the result.  If res_cnt is 	*
   * positive, then the number of skipped characters is res_cnt-1.	*
   * If res_cnt is 0, then the number of skipped characters is		*
   * chars_rej.  Note that we must add n to this, to take into account	*
   * prior scans.							*
   *--------------------------------------------------------------------*/

  if(failure < 0) {
    p = ast_make_int(((res_cnt > 0) ? res_cnt-1 : chars_rej) + n);
    result = ast_triple(p, res_suff, ENTU(leadin_char));
    unreg(mark);
    return result;
  }

  /*-------------------------------------------------------*
   * If the scan timed out, then rebuild a lazy primitive. *
   *-------------------------------------------------------*/

  else if(failure == TIME_OUT_EX) {
    p      = ast_quad(ast_make_int(chars_rej + n), ENTU(leadin_char), 
		      the_set, sense);
    result = make_lazy_prim(SCAN_FOR_TMO, res_suff, p);
    unreg(mark);
    return result;
  }

  /*--------------------------------------*
   * If the scan failed, just return nil. *
   *--------------------------------------*/

  else return nil;
}

/*------------------------------------------------------------------*/

ENTITY scan_for_stdf(ENTITY s, ENTITY l_info)
{
  ENTITY second, evaled_second;

  second        = ast_pair(zero, l_info);
  evaled_second = make_lazy_prim(FULL_EVAL_TMO, second, second);
  return make_lazy_prim(SCAN_FOR_TMO, s, evaled_second);
}
