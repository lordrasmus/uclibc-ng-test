/* sigaction must refuse the signals the thread library keeps for itself,
   the real-time signals below SIGRTMIN: a handler of the program's on
   SIGCANCEL would break pthread_cancel, one on SIGSETXID setuid in a
   program with threads.  glibc and musl refuse them with EINVAL, as
   sigaddset does here (tst-sigset).

   Checked before and after the first thread, and with a query only,
   which glibc refuses as well.  */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

static int check(const char *when)
{
	int bad = 0;
#if defined __SIGRTMIN && defined SIGRTMIN
	struct sigaction sa, old;
	int sig;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = SIG_IGN;
	for (sig = __SIGRTMIN; sig < SIGRTMIN; ++sig) {
		errno = 0;
		if (sigaction(sig, &sa, NULL) != -1 || errno != EINVAL) {
			printf("%s: sigaction (%d, handler) did not fail with EINVAL (errno %d)\n",
			       when, sig, errno);
			bad = 1;
		}
		errno = 0;
		if (sigaction(sig, NULL, &old) != -1 || errno != EINVAL) {
			printf("%s: sigaction (%d, NULL) did not fail with EINVAL (errno %d)\n",
			       when, sig, errno);
			bad = 1;
		}
	}
#else
	(void) when;
#endif
	return bad;
}

static void *thread(void *arg)
{
	return arg;
}

int main(void)
{
	pthread_t t;
	int bad;

	bad = check("before the first thread");
	if (pthread_create(&t, NULL, thread, NULL) != 0) {
		puts("pthread_create failed");
		return 1;
	}
	pthread_join(t, NULL);
	bad |= check("after the first thread");
	return bad;
}
