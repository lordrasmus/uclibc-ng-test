/* A thread gets at least the stack it asked for, whatever the
   alignment of the program's TLS.
   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.

   The descriptor and the static TLS block sit at the top of the thread's
   mapping, and the descriptor's address is rounded down to the TLS
   alignment.  Both come out of the mapping, not out of the stack the
   caller asked for.  */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

/* Enough to make the rounding visible, and small enough that every
   target can lay out a block with it -- 64K would be a lot to ask of a
   noMMU heap.  */
#define TLS_ALIGN 4096
__thread char aligned_tls[16] __attribute__ ((aligned (TLS_ALIGN))) = { 1 };

static size_t want;
static int bad;

static void *measure (void *arg)
{
  pthread_attr_t at;
  void *low;
  size_t sz, guard = 0;
  char here;

  (void) arg;
  if (pthread_getattr_np (pthread_self (), &at) != 0
      || pthread_attr_getstack (&at, &low, &sz) != 0)
    {
      puts ("pthread_getattr_np failed");
      bad = 1;
      return NULL;
    }
  pthread_attr_getguardsize (&at, &guard);

  /* getstack reports the whole mapping minus the guard; what the thread
     may use is what lies below its own frame.  */
  {
    long usable = &here - (char *) low;

    if (usable < (long) want)
      {
	printf ("asked for %zu bytes of stack, %ld usable\n", want, usable);
	bad = 1;
      }
  }
  /* And it has to be there, not just counted.  */
  {
    volatile char probe[4096];

    memset ((void *) probe, 0x5a, sizeof probe);
  }
  return NULL;
}

static void one (size_t size)
{
  pthread_attr_t a;
  pthread_t t;
  int i;

  want = size;
  for (i = 0; i < 4; i++)	/* mmap does not always land the same way */
    {
      pthread_attr_init (&a);
      if (pthread_attr_setstacksize (&a, size) != 0)
	{
	  pthread_attr_destroy (&a);
	  return;		/* below PTHREAD_STACK_MIN here */
	}
      pthread_attr_setguardsize (&a, 0);
      if (pthread_create (&t, &a, measure, NULL) != 0)
	{
	  printf ("pthread_create failed for %zu\n", size);
	  bad = 1;
	  pthread_attr_destroy (&a);
	  return;
	}
      pthread_join (t, NULL);
      pthread_attr_destroy (&a);
    }
}

int main (void)
{
  if (aligned_tls[0] != 1)
    {
      puts ("the TLS initialisation image did not arrive");
      return 1;
    }
  if (((uintptr_t) aligned_tls & (TLS_ALIGN - 1)) != 0)
    {
      /* Nothing to measure if the alignment did not survive; that is a
	 question for tls_align, not for this test.  */
      printf ("SKIP: TLS at %p is not %d-aligned\n",
	      (void *) aligned_tls, TLS_ALIGN);
      return 0;
    }

  one (65536);
  one (131072);
  one (262144);
  one (PTHREAD_STACK_MIN);

  return bad;
}
