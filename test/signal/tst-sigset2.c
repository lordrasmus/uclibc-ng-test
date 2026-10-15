/* sigset() SIG_HOLD and sighold() tests.
   Copyright (C) 2009-2026 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <https://www.gnu.org/licenses/>.  */

/* From glibc's signal/tst-sigset2.c, with perror() for error(), which is
   optional in uClibc-ng.  */

#include <features.h>

#if defined __UCLIBC__ && !defined __UCLIBC_HAS_OBSOLETE_BSD_SIGNAL__

/* signal.h declares sighold() and sigset() only with that option.  */
#include <stdio.h>
int
main (void)
{
  puts ("needs UCLIBC_HAS_OBSOLETE_BSD_SIGNAL");
  return 23;			/* SKIP */
}

#else

#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define TEST_SIG SIGINT

static void
die (const char *what)
{
  perror (what);
  exit (1);
}

/* Print mask of blocked signals for this process */
static void
printSigMask (const char *msg)
{
  sigset_t currMask;
  int sig;
  int cnt;

  if (msg != NULL)
    printf ("%s", msg);

  if (sigprocmask (SIG_BLOCK, NULL, &currMask) == -1)
    die ("sigprocmask");

  cnt = 0;
  for (sig = 1; sig < NSIG; sig++)
    {
      if (sigismember (&currMask, sig))
	{
	  cnt++;
	  printf ("\t\t%d (%s)\n", sig, strsignal (sig));
	}
    }

  if (cnt == 0)
    printf ("\t\t<empty signal set>\n");
} /* printSigMask */

static void
handler (int sig)
{
  printf ("Caught signal %d\n", sig);
  printSigMask ("Signal mask in handler\n");
  printf ("Handler returning\n");
  _exit (1);
} /* handler */

static void
printDisposition (sighandler_t disp)
{
  if (disp == SIG_HOLD)
    printf ("SIG_HOLD");
  else if (disp == SIG_DFL)
    printf ("SIG_DFL");
  else if (disp == SIG_IGN)
    printf ("SIG_IGN");
  else
    printf ("handled at %" PRIxPTR, (uintptr_t) disp);
} /* printDisposition */

static int
returnTest1 (void)
{
  sighandler_t prev;

  printf ("===== TEST 1 =====\n");
  printf ("Blocking signal with sighold()\n");
  if (sighold (TEST_SIG) == -1)
    die ("sighold");
  printSigMask ("Signal mask after sighold()\n");

  printf ("About to use sigset() to establish handler\n");
  prev = sigset (TEST_SIG, handler);
  if (prev == SIG_ERR)
    die ("sigset");

  printf ("Previous disposition: ");
  printDisposition (prev);
  printf (" (should be SIG_HOLD)\n");
  if (prev != SIG_HOLD)
    {
      printf("TEST FAILED!!!\n");
      return 1;
    }
  return 0;
} /* returnTest1 */

static int
returnTest2 (void)
{
  sighandler_t prev;

  printf ("\n===== TEST 2 =====\n");

  printf ("About to use sigset() to set SIG_HOLD\n");
  prev = sigset (TEST_SIG, SIG_HOLD);
  if (prev == SIG_ERR)
    die ("sigset");

  printf ("Previous disposition: ");
  printDisposition (prev);
  printf (" (should be SIG_DFL)\n");
  if (prev != SIG_DFL)
    {
      printf("TEST FAILED!!!\n");
      return 1;
    }
  return 0;
} /* returnTest2 */

static int
returnTest3 (void)
{
  sighandler_t prev;

  printf ("\n===== TEST 3 =====\n");

  printf ("About to use sigset() to set SIG_HOLD\n");
  prev = sigset (TEST_SIG, SIG_HOLD);
  if (prev == SIG_ERR)
    die ("sigset");

  printf ("About to use sigset() to set SIG_HOLD (again)\n");
  prev = sigset (TEST_SIG, SIG_HOLD);
  if (prev == SIG_ERR)
    die ("sigset");

  printf ("Previous disposition: ");
  printDisposition (prev);
  printf (" (should be SIG_HOLD)\n");
  if (prev != SIG_HOLD)
    {
      printf("TEST FAILED!!!\n");
      return 1;
    }
  return 0;
} /* returnTest3 */

static int
run_child (int (*test) (void))
{
#ifndef __ARCH_USE_MMU__
  /* No fork.  The tests raise no signal, so resetting the disposition
     and the mask gives each one the state a fresh child would have.  */
  if (signal (TEST_SIG, SIG_DFL) == SIG_ERR)
    die ("signal");
  sigset_t s;
  sigemptyset (&s);
  sigaddset (&s, TEST_SIG);
  if (sigprocmask (SIG_UNBLOCK, &s, NULL) == -1)
    die ("sigprocmask");
  return test ();
#else
  pid_t childPid = fork ();
  if (childPid == -1)
    die ("fork");

  if (childPid == 0)
    exit (test ());

  int status;
  if (TEMP_FAILURE_RETRY (waitpid (childPid, &status, 0)) != childPid)
    die ("waitpid");
  return !WIFEXITED (status) || WEXITSTATUS (status) != 0;
#endif
}

int
main (int argc, char *argv[])
{
  if (signal (TEST_SIG, SIG_DFL) == SIG_ERR)
    die ("signal");

  int result = run_child (returnTest1);
  result |= run_child (returnTest2);
  result |= run_child (returnTest3);

  return result;
} /* main */

#endif
