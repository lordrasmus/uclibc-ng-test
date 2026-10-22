/* Copyright (C) 2002, 2003 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 2002.

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
#include <semaphore.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include "../test-skeleton.h"

/* A bogus clock value that tells do_test_clock to use sem_timedwait
   rather than sem_clockwait.  */
#define CLOCK_USE_TIMEDWAIT (-1)

static int
do_test_clock (clockid_t clockid)
{
  const clockid_t clockid_for_get =
    clockid == CLOCK_USE_TIMEDWAIT ? CLOCK_REALTIME : clockid;
  sem_t s;
  struct timespec ts, now;

  printf ("clock = %d\n", (int) clockid);

  if (sem_init (&s, 0, 1) == -1)
    {
      puts ("sem_init failed");
      return 1;
    }

  if (TEMP_FAILURE_RETRY (sem_wait (&s)) == -1)
    {
      puts ("sem_wait failed");
      return 1;
    }

  if (clock_gettime (clockid_for_get, &ts) != 0)
    {
      puts ("clock_gettime failed");
      return 1;
    }

  /* We wait for half a second.  */
  ts.tv_nsec += 500000000;
  if (ts.tv_nsec >= 1000000000)
    {
      ++ts.tv_sec;
      ts.tv_nsec -= 1000000000;
    }

  errno = 0;
  if (TEMP_FAILURE_RETRY (clockid == CLOCK_USE_TIMEDWAIT
			  ? sem_timedwait (&s, &ts)
			  : sem_clockwait (&s, clockid, &ts)) != -1)
    {
      puts ("wait succeeded");
      return 1;
    }
  if (errno != ETIMEDOUT)
    {
      printf ("wait returned errno = %d instead of ETIMEDOUT\n", errno);
      return 1;
    }

  /* The wait must not end before the timeout on its own clock.  */
  if (clock_gettime (clockid_for_get, &now) != 0)
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

  return 0;
}

static int
do_test (void)
{
  int res = do_test_clock (CLOCK_USE_TIMEDWAIT);
  res |= do_test_clock (CLOCK_REALTIME);
  res |= do_test_clock (CLOCK_MONOTONIC);

  /* Only CLOCK_REALTIME and CLOCK_MONOTONIC are valid.  */
  sem_t s;
  struct timespec ts = { 0, 0 };
  sem_init (&s, 0, 0);
  errno = 0;
  if (sem_clockwait (&s, CLOCK_PROCESS_CPUTIME_ID, &ts) != -1
      || errno != EINVAL)
    {
      puts ("sem_clockwait accepted CLOCK_PROCESS_CPUTIME_ID");
      res = 1;
    }

  return res;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
