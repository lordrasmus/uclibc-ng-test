/* What sighold, sigrelse, sigignore and sigset do, not just what they
   return.  tst-sigsimple covers their errors for bad signal numbers and
   tst-sigset2 the dispositions sigset reports.  */

#include <features.h>

#if defined __UCLIBC__ && !defined __UCLIBC_HAS_OBSOLETE_BSD_SIGNAL__

#include <stdio.h>
int
main (void)
{
  puts ("needs UCLIBC_HAS_OBSOLETE_BSD_SIGNAL");
  return 23;			/* SKIP */
}

#else

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

static volatile sig_atomic_t caught;
static int result;

#define CHECK(cond)							\
  do									\
    if (!(cond))							\
      {									\
	printf ("%s:%d: %s failed\n", __FILE__, __LINE__, #cond);	\
	result = 1;							\
      }									\
  while (0)

static volatile sig_atomic_t blocked_in_handler;

static int
blocked (int sig)
{
  sigset_t s;
  sigprocmask (SIG_BLOCK, NULL, &s);
  return sigismember (&s, sig);
}

static void
handler (int sig)
{
  caught++;
  blocked_in_handler = blocked (sig);
}

static int
pending (int sig)
{
  sigset_t s;
  sigpending (&s);
  return sigismember (&s, sig);
}

int
main (void)
{
  struct sigaction sa;

  memset (&sa, 0, sizeof sa);
  sa.sa_handler = handler;
  sigemptyset (&sa.sa_mask);
  CHECK (sigaction (SIGUSR1, &sa, NULL) == 0);

  /* sighold blocks, the signal waits.  */
  CHECK (sighold (SIGUSR1) == 0);
  CHECK (blocked (SIGUSR1));
  CHECK (raise (SIGUSR1) == 0);
  CHECK (caught == 0);
  CHECK (pending (SIGUSR1));

  /* sigrelse unblocks, and the waiting signal arrives.  */
  CHECK (sigrelse (SIGUSR1) == 0);
  CHECK (!blocked (SIGUSR1));
  CHECK (caught == 1);
  CHECK (!pending (SIGUSR1));

  /* sigignore sets SIG_IGN; the signal then does not end the process.  */
  CHECK (sigignore (SIGUSR2) == 0);
  CHECK (sigaction (SIGUSR2, NULL, &sa) == 0);
  CHECK (sa.sa_handler == SIG_IGN);
  CHECK (raise (SIGUSR2) == 0);

  /* sigset (SIG_HOLD) blocks; sigset with a handler unblocks again,
     reports SIG_HOLD as the previous disposition and delivers.  */
  CHECK (sigset (SIGUSR1, SIG_HOLD) == handler);
  CHECK (blocked (SIGUSR1));
  CHECK (raise (SIGUSR1) == 0);
  CHECK (caught == 1);
  blocked_in_handler = 0;
  CHECK (sigset (SIGUSR1, handler) == SIG_HOLD);
  CHECK (!blocked (SIGUSR1));
  CHECK (caught == 2);

  /* That handler ran with its signal blocked, and it is free again.  */
  CHECK (blocked_in_handler);
  CHECK (!blocked (SIGUSR1));

  /* sigset with SIG_IGN and SIG_DFL takes effect.  */
  CHECK (sigset (SIGUSR1, SIG_IGN) == handler);
  CHECK (sigaction (SIGUSR1, NULL, &sa) == 0 && sa.sa_handler == SIG_IGN);
  CHECK (raise (SIGUSR1) == 0);
  CHECK (caught == 2);
  CHECK (sigset (SIGUSR1, SIG_DFL) == SIG_IGN);
  CHECK (sigaction (SIGUSR1, NULL, &sa) == 0 && sa.sa_handler == SIG_DFL);

  /* sigignore throws away a signal that is already pending.  */
  CHECK (sigset (SIGHUP, handler) != SIG_ERR);
  CHECK (sighold (SIGHUP) == 0);
  CHECK (raise (SIGHUP) == 0);
  CHECK (pending (SIGHUP));
  CHECK (sigignore (SIGHUP) == 0);
  CHECK (!pending (SIGHUP));
  CHECK (sigrelse (SIGHUP) == 0);
  CHECK (caught == 2);

  /* SIGKILL can be neither ignored nor caught.  */
  errno = 0;
  CHECK (sigignore (SIGKILL) == -1 && errno == EINVAL);
  errno = 0;
  CHECK (sigset (SIGKILL, handler) == SIG_ERR && errno == EINVAL);

  if (result == 0)
    puts ("ok");
  return result;
}

#endif
