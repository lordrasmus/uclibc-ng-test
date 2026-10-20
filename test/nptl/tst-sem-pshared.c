/* A process-shared semaphore used as a mutex by several processes.

   NPROC processes share one page (mmap MAP_SHARED) holding a sem_t
   initialised with pshared = 1 and value 1, and a plain counter.  Each
   process does ITER times: sem_timedwait, counter++, sem_post.  If the
   semaphore is atomic between processes, the counter ends at
   NPROC * ITER and the semaphore at 1.  A lost update shows up as a
   short counter or a value other than 1; a lost wakeup as a
   sem_timedwait that times out although the semaphore is free.

   Written for sparc32 without compare-and-swap, where the generic
   semaphores took their atomics from locks private to each process,
   and where a misplaced compiler barrier let nwaiters lose updates.  */

#include <errno.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ROUNDS 3
#define ITER 10000
#define NPROC 4

struct shared
{
  sem_t sem;
  volatile long counter;
};

static int
worker (struct shared *sh)
{
  for (long i = 0; i < ITER; i++)
    {
      struct timespec ts;
      clock_gettime (CLOCK_REALTIME, &ts);
      ts.tv_sec += 20;
      if (sem_timedwait (&sh->sem, &ts) != 0)
	{
	  printf ("pid %d: sem_timedwait failed at %ld: %s\n", getpid (), i,
		  errno == ETIMEDOUT ? "timed out, a post was lost"
		  : "error");
	  return 3;
	}
      long c = sh->counter;
      for (volatile int k = 0; k < 20; k++)
	;			/* widen the window a little */
      sh->counter = c + 1;
      if (sem_post (&sh->sem) != 0)
	{
	  printf ("pid %d: sem_post failed at %ld: errno %d\n", getpid (), i,
		  errno);
	  return 4;
	}
    }
  return 0;
}

int
main (void)
{
  int bad = 0;

#ifdef _CS_UCLIBC_ATOMICS
  char kind[32];
  if (confstr (_CS_UCLIBC_ATOMICS, kind, sizeof kind) != 0)
    printf ("atomics: %s\n", kind);
#endif

  struct shared *sh = mmap (NULL, sizeof *sh, PROT_READ | PROT_WRITE,
			    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (sh == MAP_FAILED)
    {
      perror ("mmap");
      return 1;
    }

  for (int r = 1; r <= ROUNDS; r++)
    {
      sh->counter = 0;
      if (sem_init (&sh->sem, 1, 1) != 0)
	{
	  if (errno == ENOSYS)
	    {
	      puts ("SKIP: no process-shared semaphores");
	      return 0;
	    }
	  perror ("sem_init");
	  return 1;
	}
      for (int p = 0; p < NPROC; p++)
	{
	  pid_t pid = fork ();
	  if (pid < 0)
	    {
	      perror ("fork");
	      return 1;
	    }
	  if (pid == 0)
	    _exit (worker (sh));
	}
      int failed = 0;
      for (int p = 0; p < NPROC; p++)
	{
	  int st;
	  if (wait (&st) < 0 || !WIFEXITED (st) || WEXITSTATUS (st) != 0)
	    failed = 1;
	}
      int val = -1;
      sem_getvalue (&sh->sem, &val);
      long want = (long) NPROC * ITER;
      if (failed || sh->counter != want || val != 1)
	{
	  printf ("round %d: counter %ld of %ld, value %d%s\n", r,
		  sh->counter, want, val, failed ? ", a process failed" : "");
	  bad = 1;
	}
      sem_destroy (&sh->sem);
    }
  return bad;
}
