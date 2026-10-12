/* getrusage() and wait4() fill a struct rusage the caller can rely on.

   Neither function was covered by this suite at all, which is how two
   defects survived in it.  Both only show with a 64-bit time_t on a 32-bit
   target, where struct timeval is 16 bytes in libc and 8 in the kernel:

     - getrusage() passed the libc structure straight to the syscall, so the
       kernel wrote its 72-byte layout into an 88-byte structure.  Everything
       from ru_stime on was shifted, and the last four longs were never
       written at all.  The canary below is what catches that, and it needs
       no timing: a field the kernel must have written cannot still hold it.

     - wait4() built ru_stime.tv_usec from the seconds word.  The canary
       cannot see that, so the second half compares wait4's rusage against
       getrusage(RUSAGE_CHILDREN), which POSIX says covers exactly the
       children already waited for -- with a single child the two must
       agree.

   Checks that depend on how much time the child happened to use are kept
   soft on purpose: this runs under qemu on more than fifty targets, where
   accounting is coarse enough that a short child can legitimately report
   zero.  */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* Position-dependent, and far outside anything a real field can hold: times
   are small, the counters are small and non-negative, and the fields Linux
   does not maintain are written as zero.  Position-dependent matters because
   a shifted write can otherwise move one field's canary into the next and
   still look overwritten.  */
#define CANARY(i) ((long) (0x5a5a0000L | (i)))

static long *
field (struct rusage *ru, unsigned i)
{
  long *l[] = {
    &ru->ru_maxrss, &ru->ru_ixrss, &ru->ru_idrss, &ru->ru_isrss,
    &ru->ru_minflt, &ru->ru_majflt, &ru->ru_nswap, &ru->ru_inblock,
    &ru->ru_oublock, &ru->ru_msgsnd, &ru->ru_msgrcv, &ru->ru_nsignals,
    &ru->ru_nvcsw, &ru->ru_nivcsw,
  };
  return l[i];
}
#define NFIELDS 14

static const char *
field_name (unsigned i)
{
  static const char *n[NFIELDS] = {
    "ru_maxrss", "ru_ixrss", "ru_idrss", "ru_isrss", "ru_minflt",
    "ru_majflt", "ru_nswap", "ru_inblock", "ru_oublock", "ru_msgsnd",
    "ru_msgrcv", "ru_nsignals", "ru_nvcsw", "ru_nivcsw",
  };
  return n[i];
}

static int
check_timeval (const char *what, const struct timeval *tv)
{
  if (tv->tv_usec < 0 || tv->tv_usec > 999999)
    {
      printf ("%s.tv_usec = %lld, must be in [0, 999999]\n",
	      what, (long long) tv->tv_usec);
      return 1;
    }
  if (tv->tv_sec < 0)
    {
      printf ("%s.tv_sec = %lld, must not be negative\n",
	      what, (long long) tv->tv_sec);
      return 1;
    }
  return 0;
}

/* Enough work to register as user and as system time, but short enough that
   the whole suite does not notice.  */
static void
burn (void)
{
  volatile unsigned long x = 0;
  int i;

  for (i = 0; i < 200000; i++)
    x += i;
  for (i = 0; i < 2000; i++)
    getppid ();
}

static int
do_test (void)
{
  struct rusage ru, ru_after, ru4, ruc;
  unsigned i;
  int result = 0;
  int status;
  pid_t pid;

  /* 1. Every field the kernel reports must have been written.  */
  for (i = 0; i < NFIELDS; i++)
    *field (&ru, i) = CANARY (i);
  memset (&ru.ru_utime, 0xa5, sizeof ru.ru_utime);
  memset (&ru.ru_stime, 0xa5, sizeof ru.ru_stime);

  if (getrusage (RUSAGE_SELF, &ru) != 0)
    {
      printf ("getrusage(RUSAGE_SELF) failed: %m\n");
      return 1;
    }

  for (i = 0; i < NFIELDS; i++)
    if (*field (&ru, i) == CANARY (i))
      {
	printf ("getrusage left %s untouched (still 0x%lx)\n",
		field_name (i), (unsigned long) CANARY (i));
	result = 1;
      }

  result |= check_timeval ("RUSAGE_SELF ru_utime", &ru.ru_utime);
  result |= check_timeval ("RUSAGE_SELF ru_stime", &ru.ru_stime);

  /* A shifted write passes the range check above -- tv_usec stays small and
     tv_sec merely becomes large -- so bound the total as well.  The suite
     kills a test after a few seconds, so anything near a minute of CPU time
     is a misread field rather than a slow machine.  */
  if (ru.ru_utime.tv_sec + ru.ru_stime.tv_sec > 60)
    {
      printf ("RUSAGE_SELF claims %lld s user + %lld s system CPU time, which"
	      " this test cannot have used\n",
	      (long long) ru.ru_utime.tv_sec, (long long) ru.ru_stime.tv_sec);
      result = 1;
    }

  if (ru.ru_maxrss <= 0)
    {
      printf ("ru_maxrss = %ld, expected a positive resident set size\n",
	      ru.ru_maxrss);
      result = 1;
    }

  /* 2. Time must not run backwards.  Soft: coarse accounting may leave both
     readings equal, which says nothing either way.  */
  burn ();
  if (getrusage (RUSAGE_SELF, &ru_after) != 0)
    {
      printf ("second getrusage(RUSAGE_SELF) failed: %m\n");
      return 1;
    }
  if (ru_after.ru_utime.tv_sec < ru.ru_utime.tv_sec
      || (ru_after.ru_utime.tv_sec == ru.ru_utime.tv_sec
	  && ru_after.ru_utime.tv_usec < ru.ru_utime.tv_usec))
    {
      printf ("ru_utime went backwards: %lld.%06lld -> %lld.%06lld\n",
	      (long long) ru.ru_utime.tv_sec, (long long) ru.ru_utime.tv_usec,
	      (long long) ru_after.ru_utime.tv_sec, (long long) ru_after.ru_utime.tv_usec);
      result = 1;
    }

  /* 3. wait4() and getrusage(RUSAGE_CHILDREN) describe the same child.  */
  pid = fork ();
  if (pid < 0)
    {
      printf ("fork failed: %m\n");
      return 1;
    }
  if (pid == 0)
    {
      burn ();
      _exit (0);
    }

  memset (&ru4, 0xa5, sizeof ru4);
  if (wait4 (pid, &status, 0, &ru4) != pid)
    {
      printf ("wait4 failed: %m\n");
      return 1;
    }
  if (!WIFEXITED (status) || WEXITSTATUS (status) != 0)
    {
      printf ("child did not exit cleanly: status 0x%x\n", status);
      result = 1;
    }

  result |= check_timeval ("wait4 ru_utime", &ru4.ru_utime);
  result |= check_timeval ("wait4 ru_stime", &ru4.ru_stime);

  if (getrusage (RUSAGE_CHILDREN, &ruc) != 0)
    {
      printf ("getrusage(RUSAGE_CHILDREN) failed: %m\n");
      return 1;
    }

  if (ru4.ru_utime.tv_sec != ruc.ru_utime.tv_sec
      || ru4.ru_utime.tv_usec != ruc.ru_utime.tv_usec
      || ru4.ru_stime.tv_sec != ruc.ru_stime.tv_sec
      || ru4.ru_stime.tv_usec != ruc.ru_stime.tv_usec)
    {
      printf ("wait4 and RUSAGE_CHILDREN disagree about the one child:\n"
	      "  wait4    utime %lld.%06lld  stime %lld.%06lld\n"
	      "  CHILDREN utime %lld.%06lld  stime %lld.%06lld\n",
	      (long long) ru4.ru_utime.tv_sec, (long long) ru4.ru_utime.tv_usec,
	      (long long) ru4.ru_stime.tv_sec, (long long) ru4.ru_stime.tv_usec,
	      (long long) ruc.ru_utime.tv_sec, (long long) ruc.ru_utime.tv_usec,
	      (long long) ruc.ru_stime.tv_sec, (long long) ruc.ru_stime.tv_usec);
      result = 1;
    }

  return result;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
