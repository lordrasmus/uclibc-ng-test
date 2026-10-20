/* From libc-test (MIT, see ../libc-test.h), src/regression/pthread_atfork-errno-clobber.c at 7b95dfa.
   Changed: drops root first, so that RLIMIT_NPROC makes fork fail; skipped
   without an MMU, where there is no fork(). */
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "../libc-test.h"

#ifndef __ARCH_USE_MMU__
int main(void)
{
	puts("SKIP: no fork() without an MMU");
	return 0;
}
#else

#define TEST(c, ...) ((c) ? 1 : (t_error(#c" failed: " __VA_ARGS__),0))

static void handler_errno(void)
{
	errno = 0;
}

int main(void)
{
	/* RLIMIT_NPROC does not bind root, and the suite runs as root. */
	if (getuid() == 0 && setuid(65534))
		t_error("setuid(65534) failed\n");
	t_setrlim(RLIMIT_NPROC, 0);
	pthread_atfork(handler_errno, handler_errno, handler_errno);

	pid_t pid;
	if (!TEST((pid = fork()) == -1, "fork succeeded despite rlimit\n")) {
		if (!pid) _exit(0);
		while (waitpid(pid, NULL, 0)<0 && errno==EINTR);
	} else {
		TEST(errno != 0, "fork failed but errno was clobbered\n");
	}

	return t_status;
}
#endif
