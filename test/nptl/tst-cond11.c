/* Copyright (C) 2003, 2004 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 2003.

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
   <http://www.gnu.org/licenses/>.  */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>


/* A bogus clock value that tells run_test to use pthread_cond_timedwait
   rather than pthread_cond_clockwait.  */
#define CLOCK_USE_ATTR_CLOCK (-1)

#if defined _POSIX_CLOCK_SELECTION && _POSIX_CLOCK_SELECTION >= 0
static int
run_test (clockid_t cl, clockid_t wait_clock)
{
  pthread_condattr_t condattr;
  pthread_cond_t cond;
  pthread_mutexattr_t mutattr;
  pthread_mutex_t mut;

  printf ("clock = %d, wait clock = %d\n", (int) cl, (int) wait_clock);
  int use_attr = wait_clock == CLOCK_USE_ATTR_CLOCK;
  if (use_attr)
    wait_clock = cl;

  if (pthread_condattr_init (&condattr) != 0)
    {
      puts ("condattr_init failed");
      return 1;
    }

  if (pthread_condattr_setclock (&condattr, cl) != 0)
    {
      puts ("condattr_setclock failed");
      return 1;
    }

  clockid_t cl2;
  if (pthread_condattr_getclock (&condattr, &cl2) != 0)
    {
      puts ("condattr_getclock failed");
      return 1;
    }
  if (cl != cl2)
    {
      printf ("condattr_getclock returned wrong value: %d, expected %d\n",
	      (int) cl2, (int) cl);
      return 1;
    }

  if (pthread_cond_init (&cond, &condattr) != 0)
    {
      puts ("cond_init failed");
      return 1;
    }

  if (pthread_condattr_destroy (&condattr) != 0)
    {
      puts ("condattr_destroy failed");
      return 1;
    }

  if (pthread_mutexattr_init (&mutattr) != 0)
    {
      puts ("mutexattr_init failed");
      return 1;
    }

  if (pthread_mutexattr_settype (&mutattr, PTHREAD_MUTEX_ERRORCHECK) != 0)
    {
      puts ("mutexattr_settype failed");
      return 1;
    }

  if (pthread_mutex_init (&mut, &mutattr) != 0)
    {
      puts ("mutex_init failed");
      return 1;
    }

  if (pthread_mutexattr_destroy (&mutattr) != 0)
    {
      puts ("mutexattr_destroy failed");
      return 1;
    }

  if (pthread_mutex_lock (&mut) != 0)
    {
      puts ("mutex_lock failed");
      return 1;
    }

  if (pthread_mutex_lock (&mut) != EDEADLK)
    {
      puts ("2nd mutex_lock did not return EDEADLK");
      return 1;
    }

  struct timespec ts;
  if (clock_gettime (wait_clock, &ts) != 0)
    {
      puts ("clock_gettime failed");
      return 1;
    }

  /* Wait one second.  */
  ++ts.tv_sec;

  int e = use_attr
	  ? pthread_cond_timedwait (&cond, &mut, &ts)
	  : pthread_cond_clockwait (&cond, &mut, wait_clock, &ts);
  if (e == 0)
    {
      puts ("cond_timedwait succeeded");
      return 1;
    }
  else if (e != ETIMEDOUT)
    {
      puts ("cond_timedwait did not return ETIMEDOUT");
      return 1;
    }

  /* The wait must not end before the timeout on its own clock.  */
  struct timespec now;
  if (clock_gettime (wait_clock, &now) != 0)
    {
      puts ("clock_gettime failed");
      return 1;
    }
  if (now.tv_sec < ts.tv_sec
      || (now.tv_sec == ts.tv_sec && now.tv_nsec < ts.tv_nsec))
    {
      printf ("timed out at %ld.%09ld, before %ld.%09ld\n",
	      (long) now.tv_sec, now.tv_nsec, (long) ts.tv_sec, ts.tv_nsec);
      return 1;
    }

  if (pthread_mutex_unlock (&mut) != 0)
    {
      puts ("mutex_unlock failed");
      return 1;
    }

  if (pthread_mutex_destroy (&mut) != 0)
    {
      puts ("mutex_destroy failed");
      return 1;
    }

  if (pthread_cond_destroy (&cond) != 0)
    {
      puts ("cond_destroy failed");
      return 1;
    }

  return 0;
}
#endif


static int
do_test (void)
{
#if !defined _POSIX_CLOCK_SELECTION || _POSIX_CLOCK_SELECTION == -1

  puts ("_POSIX_CLOCK_SELECTION not supported, test skipped");
  return 0;

#else

  int res = run_test (CLOCK_REALTIME, CLOCK_USE_ATTR_CLOCK);

  /* Only CLOCK_REALTIME and CLOCK_MONOTONIC are valid wait clocks.  */
  pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
  pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
  struct timespec ts = { 0, 0 };
  pthread_mutex_lock (&mut);
  if (pthread_cond_clockwait (&cond, &mut, CLOCK_PROCESS_CPUTIME_ID, &ts)
      != EINVAL)
    {
      puts ("cond_clockwait accepted CLOCK_PROCESS_CPUTIME_ID");
      res = 1;
    }
  pthread_mutex_unlock (&mut);

# if defined _POSIX_MONOTONIC_CLOCK && _POSIX_MONOTONIC_CLOCK >= 0
#  if _POSIX_MONOTONIC_CLOCK == 0
  int e = sysconf (_SC_MONOTONIC_CLOCK);
  if (e < 0)
    puts ("CLOCK_MONOTONIC not supported");
  else if (e == 0)
    {
      puts ("sysconf (_SC_MONOTONIC_CLOCK) must not return 0");
      res = 1;
    }
  else
#  endif
    {
      res |= run_test (CLOCK_MONOTONIC, CLOCK_USE_ATTR_CLOCK);
      res |= run_test (CLOCK_REALTIME, CLOCK_MONOTONIC);
      res |= run_test (CLOCK_MONOTONIC, CLOCK_MONOTONIC);
      res |= run_test (CLOCK_MONOTONIC, CLOCK_REALTIME);
    }
# else
  puts ("_POSIX_MONOTONIC_CLOCK not defined");
# endif

  return res;
#endif
}

#define TIMEOUT 10
#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
