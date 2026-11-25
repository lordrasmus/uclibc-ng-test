/* TLS that does not fit the reserve in bss: the block then comes from
   sbrk, or from mmap without an MMU, before the thread pointer exists.
   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.

   What this checks is what that path has to get right: the
   initialisation image arrives whole, .tbss is zero to the last byte,
   an increased alignment survives, and a second thread gets its own
   copy.  UCLIBC_STATIC_TLS_RESERVE_SIZE is 128 by default and at most
   65536, so 68K of TLS is past it either way.  */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define TDATA_SIZE 4096
#define TBSS_SIZE  (64 * 1024)

/* Initialised, so it lands in .tdata and has to be copied.  */
__thread unsigned char tdata[TDATA_SIZE] = { [0] = 1, [TDATA_SIZE - 1] = 2 };
/* Uninitialised, so it lands in .tbss and has to be zeroed.  */
__thread unsigned char tbss[TBSS_SIZE];
/* And one with an alignment of its own.  */
__thread unsigned char aligned[16] __attribute__ ((aligned (4096))) = { 3 };

static int check (const char *who)
{
  size_t i;
  int bad = 0;

  if (tdata[0] != 1 || tdata[TDATA_SIZE - 1] != 2)
    {
      printf ("%s: .tdata image is wrong: %u ... %u\n", who,
	      tdata[0], tdata[TDATA_SIZE - 1]);
      bad = 1;
    }
  for (i = 1; i < TDATA_SIZE - 1; i++)
    if (tdata[i] != 0)
      {
	printf ("%s: .tdata[%zu] = %u, not 0\n", who, i, tdata[i]);
	bad = 1;
	break;
      }
  for (i = 0; i < TBSS_SIZE; i++)
    if (tbss[i] != 0)
      {
	printf ("%s: .tbss[%zu] = %u, not 0\n", who, i, tbss[i]);
	bad = 1;
	break;
      }
  if (aligned[0] != 3)
    {
      printf ("%s: the aligned variable lost its value\n", who);
      bad = 1;
    }
  if (((uintptr_t) aligned & 4095) != 0)
    {
      printf ("%s: %p is not 4096-aligned\n", who, (void *) aligned);
      bad = 1;
    }
  return bad;
}

static int from_thread;

static void *run (void *arg)
{
  (void) arg;
  from_thread = check ("thread");
  /* Its own copy: writing here must not reach the main thread's.  */
  memset (tbss, 0x5a, TBSS_SIZE);
  tdata[0] = 9;
  return NULL;
}

int main (void)
{
  pthread_t t;
  int bad = check ("main");

  if (pthread_create (&t, NULL, run, NULL) != 0)
    {
      puts ("pthread_create failed");
      return 1;
    }
  pthread_join (t, NULL);
  bad |= from_thread;

  if (tdata[0] != 1 || tbss[0] != 0 || tbss[TBSS_SIZE - 1] != 0)
    {
      puts ("the thread wrote into the main thread's TLS");
      bad = 1;
    }
  return bad;
}
