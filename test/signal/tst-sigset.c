/* Test sig*set functions.  */

#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define TEST_FUNCTION do_test ()
static int
do_test (void)
{
  int result = 0;
  int sig = -1;

#define TRY(call)							      \
  if (call)								      \
    {									      \
      printf ("%s (sig = %d): %s\n", #call, sig, strerror(errno));			      \
      result = 1;							      \
    }									      \
  else


  sigset_t set;
  TRY (sigemptyset (&set) != 0);

#ifdef SIGRTMAX
  int max_sig = SIGRTMAX;
#else
  int max_sig = NSIG - 1;
#endif

  for (sig = 1; sig <= max_sig; ++sig)
    {
#if defined __SIGRTMIN && defined SIGRTMIN
      /* The real-time signals below SIGRTMIN are the library's own.  Like
	 glibc and musl, sigaddset and sigdelset must refuse them, or an
	 application could block the signal pthread_cancel relies on.  */
      if (sig >= __SIGRTMIN && sig < SIGRTMIN)
	{
	  errno = 0;
	  TRY (sigaddset (&set, sig) != -1 || errno != EINVAL);
	  errno = 0;
	  TRY (sigdelset (&set, sig) != -1 || errno != EINVAL);
	  TRY (sigismember (&set, sig) != 0);
	  continue;
	}
#endif
      TRY (sigismember (&set, sig) != 0);
      TRY (sigaddset (&set, sig) != 0);
      TRY (sigismember (&set, sig) == 0);
      TRY (sigdelset (&set, sig) != 0);
      TRY (sigismember (&set, sig) != 0);
    }

  return result;
}

#include "../test-skeleton.c"
