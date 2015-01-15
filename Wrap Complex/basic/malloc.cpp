/*
// Copyright 1996-97. Raindrop Geomagic, Inc.
// All Rights Reserved.
//
// This is unpublished proprietary source code of Raindrop Geomagic, Inc.; 
// the contents of this file may not be disclosed to third parties, copied 
// or duplicated in any form, in whole or in part, without the prior written 
// permission of Raindrop Geomagic, Inc.  
//
// This copyright notices may not be removed from this file at any time.
// 
// RESTRICTED RIGHTS LEGEND:
// Use, duplication or disclosure by the Government is subject to restrictions
// as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
// and Computer Software clause at DFARS 252.227-7013, and/or in similar or
// successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished-
// rights reserved under the Copyright Laws of the United States.
// 
//                 Raindrop Geomagic, Inc
//                 206 N. Randolph Street, Suite 520
//                 Champaign IL 61820-3898
//                 Phone: 217-239-2551
//                 Fax:   217-239-2556
//                 Email: inquiry@geomagic.com
// 
*/

/* basic/malloc.c --- My very own routines to encapsulate malloc. */

/*---------------------------------------------------------------------------*/

#include "basic.h"

#ifdef _WINDOWS
#include <sys\types.h>
#else
#include <sys/types.h>
#endif

#if !(defined (NeXT) || defined (__convex__) || defined (__alpha) || defined(_WINDOWS))
# include <malloc.h>
/*# define USE_mallinfo*/   /* Uncomment this when you don't have mallinfo(). */
#endif

/*---------------------------------------------------------------------------*/

static void dummy_malloc_fail_handler (void) { }
void (*malloc_fail_handler) (void) = dummy_malloc_fail_handler;

void 
set_malloc_fail_handler (void (*malloc_fail_handler_fun) (void))
{
  malloc_fail_handler = malloc_fail_handler_fun;
}

/*---------------------------------------------------------------------------*/

#define REPORT_THRESHOLD  1000000

#define Proto  if (debug_level > 2) printf /* Usage: Proto (printf_arguments) */

static int debug_level = 0;
static unsigned long total = 0;

typedef struct malist_record
{
  void *address;
  int bytes; 
  const char *f;
  int l;
  int mark;
  struct malist_record *next;
} Malist;

static Malist *ml = NULL;  /* ml keeps track of allocated memory */
static int ml_len = 0;

/* routines to manage ml */
static void malist_push   (void *address, int bytes, const char *f, int l);
static void malist_delete (void *address);

#ifdef _WINDOWS
HANDLE g_hHeap = NULL;
#endif

/*---------------------------------------------------------------------------*/

void* basic_malloc (int n,          /* number of bytes, n > 0 */
                    const char *f,  /* for __FILE__ macro */
                    int l)          /* for __LINE__ macro */
     /* This is the encapsulated malloc function. It calls malloc() (see man
        pages) and issues a basic_errro() when allocation fails.  NOTE: This
        should only be used in connection with MALLOC marco in basic.h. */
     /* NOTE: Currently, basic_malloc() chunks only allow for 'int n' bytes
        (and not 'unsigned long n') */
{
  Basic_byte *memory;
  Proto (">>> basic_malloc (n=%d, f=\"%s\", l=%d)\n", n, f, l);
#ifdef _WINDOWS
  if (g_hHeap == NULL) g_hHeap = GetProcessHeap();
  if (n > 0) 
    memory = (Basic_byte *) 
      HeapAlloc(g_hHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS, 
		(DWORD) n);
#else
  memory = (Basic_byte *) malloc ((unsigned) n);
#endif
  if ((n < 1) or (not memory))
  {
    basic_error ("basic_malloc: failed, n=%d, f=\"%s\", l=%d", n, f, l);
    malloc_fail_handler ();
  }
  Proto (">>> basic_malloc() returns 0x%lx ... 0x%lx\n",
         Addr (memory), Addr (&(memory[n])));
  total += n;
  if (debug_level > 0)
    malist_push (memory, n, f, l);
  return (memory);
}

/*---------------------------------------------------------------------------*/

void* basic_realloc (void *ptr,      /* pointer to memory to be re-allocated */
                     int n,          /* number of bytes, n > 0 */
                     const char *f,  /* for __FILE__ macro */
                     int l)          /* for __LINE__ macro */
     /* This is the encapsulated realloc function. It calls realloc() (see man
        pages) and issues a basic_errro() when allocation fails.  NOTE: This
        should only be used in connection with REALLOC marco in basic.h. */
{
  Basic_byte *memory;
  Proto (">>> basic_realloc (ptr=0x%lx, n=%d, f=\"%s\", l=%d)\n",
         Addr (ptr), n, f, l);
  if (not ptr)
    basic_error ("basic_realloc: ptr=0");
#ifdef _WINDOWS
  if(g_hHeap == NULL) g_hHeap = GetProcessHeap();
  if (n > 0) 
    memory = (Basic_byte *) 
      HeapReAlloc(g_hHeap, HEAP_ZERO_MEMORY | HEAP_GENERATE_EXCEPTIONS,
		  (LPVOID) ptr, (DWORD) n);
#else
  memory = (Basic_byte *) realloc (ptr, (unsigned) n);
#endif
  if ((n < 1) or (not memory))
  {
    basic_error ("basic_realloc: failed, ptr=0x%lxn=%d, f=\"%s\", l=%d",
                 Addr (ptr), n, f, l);
    malloc_fail_handler ();
  }
  Proto (">>> basic_realloc() returns 0x%lx ... 0x%lx\n",
         Addr (memory), Addr (&(memory[n])));
  total += n;
  if (debug_level > 0)
    {
      malist_delete (ptr);
      malist_push (memory, n, f, l);
    }
  return (memory);
}

/*---------------------------------------------------------------------------*/

void basic_free (void *ptr,      /* pointer to memory to free */
                 const char *f,  /* for __FILE__ macro */
                 int l)          /* for __LINE__ macro */
     /* This is the encapsulated free function. It calls free()
        (see man pages) and should only be used in FREE macro
        of basic.h, since FREE (ptr) sets ptr to NULL, too */
{
  if (ptr)
    {
      Proto (">>> basic_free (ptr=0x%lx, f=\"%s\", l=%d)\n", Addr (ptr), f, l);
      
#ifdef _WINDOWS
	  HeapFree(g_hHeap,0,(LPVOID)ptr);
#else
	  free (ptr);
#endif
      if (debug_level > 0)
        malist_delete (ptr);
    }
}

/*---------------------------------------------------------------------------*/

char* basic_strdup (const char *s1,  /* string to be duplicated */
                    const char *f,   /* for __FILE__ macro */
                    int l)           /* for __LINE__ macro */
     /* This function returns a pointer to a duplicate of string s1.
        The memory therefore is obtained by basic_malloc().  One should
        always call this function via the STRDUP macro of basic.h.  The
        resulting sring should be deallocated using FREE (ie, basic_free).
        NOTE: It "hides" the allocated string via basic_mark (..., -2).
        Cf, standard strdup(). */
{
  if (s1)
    {
      char *r = (char *) basic_malloc ((int) (sizeof (char) * (strlen (s1) + 1)), f, l);
      //sprintf (r, "%s", s1);
	  sprintf_s (r, strlen(s1) + 1, "%s", s1);
      basic_malloc_mark (r, -2);
      return (r);
    }
  else
    return (NULL);
}

/*---------------------------------------------------------------------------*/

Basic_malloc_info* basic_malloc_info (void)
     /* Returns some malloc info. */
     /* NOTE: The returned address, which points to the info structure,
              is a constant.  Do not FREE() it and consider the fields
              of the structure as read-only. */
{
  static Basic_malloc_info info;
  info.total = total;
  info.in_use = basic_malloc_marked_bytes (0, MAXINT);
  info.hidden = basic_malloc_marked_bytes (-MAXINT, -1);
/*#ifdef USE_mallinfo
  {
    static struct mallinfo mi;
    mi = mallinfo ();
    info.arena = mi.arena;
    info.used_sml = mi.usmblks;
    info.used_ord = mi.uordblks;
    info.used = info.used_sml + info.used_ord;
  }
 #else
*/
  info.arena = info.used = info.used_sml = info.used_ord = (unsigned long) -1;  /* undefined */
/*#endif*/
  return (&(info));
}

/*---------------------------------------------------------------------------*/

void basic_malloc_debug (int level)
     /* To set the basic_malloc debugging level.
        Level i includes all levels l with l < i.
        The different levels have the following meaning:
        0 ... minimal or no debugging (default);
        1 ... keep track of allocated meomory;
        2 ... report size of malloc heap every REPORT_THRESHOLD bytes;
        3 ... protocol mode (see #define Proto);
        4 ... mallopt (M_DEBUG, 1); works on SGI's (cc -xansi) only! */
{
  debug_level = level;
#if  0 && defined (sgi) &&  defined (_SGI_SOURCE)
  /* defined (sgi) && !defined (_SGI_SOURCE) probably means cc -ansi */
  mallopt (M_DEBUG, If ((level > 3), 1, 0));
#else
  if (debug_level > 1)
    fprintf (stderr, "WARNIG: basic_malloc_debug(%d) ignored!\n", debug_level);
#endif
}

/*---------------------------------------------------------------------------*/

void basic_malloc_mark (void *ptr, int mark)
     /* Marks memory object specified by ptr, provided it was allocated
        by this module, and the debugging level is larger than 0.
        The integer marks have different meaning according to their sign:
        == 0 ... unmarked;
        <  0 ... hidden (-1 ... via HIDE(), -2 ... via STRDUP());
        >= 0 ... in use */
{
  if (ml)
    { /* Note: ml is managed by malist_push(), etc. */
      Malist *p = ml;
      while (p->address)
        {
          if (p->address == ptr)
            {
              p->mark = mark;
              return;
            }
          p = p->next;
        }
    }
}

/*---------------------------------------------------------------------------*/

unsigned long basic_malloc_marked_bytes (int a, int b)
     /* Returns total size of memory objects marked with m, st, a <= m <= b */
{
  int r = 0;
  if (ml)
    {
      Malist *p = ml;
      while (p->address)
        {
          if ((a <= p->mark) and (p->mark <= b))
            r += p->bytes;
          p = p->next;
        }
    }
  return (r);
}

/*---------------------------------------------------------------------------*/

void basic_malloc_list_print (FILE *file)
     /* Prints the internal list that keeps track of allocated memory
        objects to given file; if *file == NULL, call is ignored. */
{
  if (file and (debug_level > 0) and ml)
    {
      Malist *p = ml;
      fprintf (file, "(basic_malloc list: %s", If (ml_len, "", "empty"));
      while (p->address)
        {
          fprintf (file, "\n 0x%lx-> %8d bytes, f=%12s, l=%3d",
                  Addr (p->address), p->bytes, p->f, p->l);
          if (p->mark < 0)
            fprintf (file, ", mark=%d (hidden%s%s)",
                    p->mark,
                    If ((p->mark == -1), ":HIDE", ""),
                    If ((p->mark == -2), ":STRDUP", ""));
          else if (p->mark > 0)
            fprintf (file, ", mark=%d", p->mark);
          p = p->next;
        }
      fprintf (file, ")\n");
    }
}

/*---------------------------------------------------------------------------*/

void basic_malloc_info_print (FILE *file)
     /* Prints some information about the malloc heap to given file;
        if *file == NULL, call is ignored. */
{
  if (file)
    {
      Basic_malloc_info *di = basic_malloc_info ();
      if (di->arena > 0)
        fprintf (file,
                "(malloc arena: %ldk, in use: %ldk, sml: %ldk, ord: %ldk)\n",
                basic_kbytes (di->arena), basic_kbytes (di->used),
                basic_kbytes (di->used_sml), basic_kbytes (di->used_ord));
      if (debug_level > 0)
        {
          fprintf (file, "(basic_malloc total: %ldk", basic_kbytes (di->total));
          if (di->in_use > 0)
            fprintf (file, ", in use: %ldk", basic_kbytes (di->in_use));
          if (di->hidden > 0)
            fprintf (file, ", hidden: %ldk", basic_kbytes (di->hidden));
          fprintf (file, ")\n");
          if (di->in_use > 0) 
            basic_malloc_list_print (file);
        }
    }
}

/*---------------------------------------------------------------------------*/

static void malist_push (void *address, int bytes, const char *f, int l)
     /* Pushes malloc info on internal list ml to keep track of allocated
        memory. */
{
  Malist *newlist;
  Assert (address and (bytes > 0));
  if (ml == NULL)
    { /* initialize  with stopper! */
      ml = (Malist *) malloc (sizeof (Malist));
      BZERO (ml, Malist, 1);
    }
  newlist = (Malist *) malloc (sizeof (Malist));
  ml_len ++;
  newlist->address = address;
  newlist->bytes = bytes;
  newlist->f = f;
  newlist->l = l;
  newlist->mark = 0;
  newlist->next = ml;
  ml = newlist;
  if (debug_level > 1)
    { 
      static unsigned long last_report = 0;
      if (total - last_report > REPORT_THRESHOLD)
        {
#ifdef USE_mallinfo
          struct mallinfo mi;
          mi = mallinfo ();
/*          printf ("(malloc arena: %ldk, basic_malloc total: %ldk)\n",
                 basic_kbytes (mi.arena), basic_kbytes (total));*/
#else
          /*printf ("(basic_malloc total: %ldk)\n", basic_kbytes (total));*/
#endif
          last_report = total;
        }
    }
}

/*---------------------------------------------------------------------------*/

static void malist_delete (void *address)
     /* Deletes given address from ml, if it finds it there. */
{
  if (address and ml)
    {
      Malist *p = ml;
      while (p->address)
        {
          if (p->address == address)
            { /* delete entry by copying the p->next record to p (stopper!) */
              Malist *succ = p->next;
              ml_len --;
              BCOPY (succ, p, sizeof (Malist));
              FREE(succ);
              return;
            }
          p = p->next;
        }
    }
}
