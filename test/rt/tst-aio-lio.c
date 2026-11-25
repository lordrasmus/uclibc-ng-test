/* lio_listio: modes, opcodes, the answer it gives and the notification.
   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.

   Every case runs in a child of its own, with a watchdog: the ones that
   found bugs here hung or burned the CPU rather than failing.  */

#include <aio.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static char fill[65536];

static void fill_pipe(int fd)
{
	fcntl(fd, F_SETFL, O_NONBLOCK);
	while (write(fd, fill, sizeof fill) > 0)
		;
	fcntl(fd, F_SETFL, 0);
}

static void cb_init(struct aiocb *cb, int fd, void *buf, size_t n, int op)
{
	memset(cb, 0, sizeof *cb);
	cb->aio_fildes = fd;
	cb->aio_buf = buf;
	cb->aio_nbytes = n;
	cb->aio_lio_opcode = op;
}

static int l_mode(void)
{
	struct aiocb *list[1] = { 0 };

	errno = 0;
	if (lio_listio(4711, list, 1, 0) != -1 || errno != EINVAL) return 1;
	errno = 0;
	if (lio_listio(LIO_WAIT, list, -1, 0) != -1 || errno != EINVAL) return 2;
	return 0;
}

/* A count whose byte size does not fit is over any limit.  Only
   reachable where size_t is narrow enough; the library must answer
   before it touches the list.  */
static int l_count(void)
{
	struct aiocb *list[1] = { 0 };

	if ((size_t)INT_MAX <= (SIZE_MAX - 64) / sizeof *list)
		return 0;
	errno = 0;
	if (lio_listio(LIO_WAIT, list, INT_MAX, 0) != -1 || errno != EINVAL) {
		printf("  errno = %d\n", errno); return 1;
	}
	return 0;
}

static int l_opcode(void)
{
	int p[2]; struct aiocb c1, *list[1] = { &c1 };

	if (pipe(p)) return 90;
	cb_init(&c1, p[1], fill, 1, 4711);
	errno = 0;
	if (lio_listio(LIO_WAIT, list, 1, 0) != -1 || errno != EIO) {
		printf("  lio_listio errno = %d\n", errno); return 1;
	}
	if (aio_error(&c1) != EINVAL) {
		printf("  aio_error = %d\n", aio_error(&c1)); return 2;
	}
	return 0;
}

static int l_wait(void)
{
	int p[2]; struct aiocb c1, c2, *list[2] = { &c1, &c2 };
	char buf[16];

	if (pipe(p)) return 90;
	if (write(p[1], "0123456789abcdef", 16) != 16) return 90;
	cb_init(&c1, p[0], buf, 8, LIO_READ);
	cb_init(&c2, 0, 0, 0, LIO_NOP);
	if (lio_listio(LIO_WAIT, list, 2, 0)) {
		printf("  lio_listio errno = %d\n", errno); return 1;
	}
	if (aio_error(&c1) != 0 || aio_return(&c1) != 8) return 2;
	return 0;
}

/* LIO_WAIT must sleep, not spin.  */
static int l_wait_cpu(void)
{
	int p[2]; struct aiocb c1, *list[1] = { &c1 };
	struct rusage ru;
	long ms;
	char buf[8];

	if (pipe(p)) return 90;
	switch (fork()) {
	case -1: return 90;
	case 0:
		usleep(1500000);
		write(p[1], "12345678", 8);
		_exit(0);
	}
	cb_init(&c1, p[0], buf, 8, LIO_READ);
	if (lio_listio(LIO_WAIT, list, 1, 0)) return 1;
	getrusage(RUSAGE_SELF, &ru);
	ms = ru.ru_utime.tv_sec * 1000 + ru.ru_utime.tv_usec / 1000
	   + ru.ru_stime.tv_sec * 1000 + ru.ru_stime.tv_usec / 1000;
	if (ms > 500) { printf("  %ld ms of CPU for a 1.5 s wait\n", ms); return 2; }
	return 0;
}

/* One entry fails at submission: the others still go, and the call
   answers EIO at the end.  */
static int l_bad_entry(void)
{
	int p[2]; struct aiocb c1, c2, *list[2] = { &c1, &c2 };
	char buf[8];

	if (pipe(p)) return 90;
	if (write(p[1], "12345678", 8) != 8) return 90;
	cb_init(&c1, -1, fill, 1, LIO_WRITE);
	cb_init(&c2, p[0], buf, 8, LIO_READ);
	errno = 0;
	if (lio_listio(LIO_WAIT, list, 2, 0) != -1 || errno != EIO) {
		printf("  errno = %d\n", errno); return 1;
	}
	if (aio_error(&c2) != 0 || aio_return(&c2) != 8) {
		printf("  the good entry did not run\n"); return 2;
	}
	return 0;
}

/* An interrupted wait leaves the outcome open: EINTR, even when an
   entry of the list already failed.  */
static void alrm(int s) { (void)s; }

static int l_eintr(void)
{
	int p[2]; struct aiocb c1, c2, *list[2] = { &c1, &c2 };
	struct sigaction sa;

	if (pipe(p)) return 90;
	fill_pipe(p[1]);
	memset(&sa, 0, sizeof sa);
	sa.sa_handler = alrm;			/* no SA_RESTART */
	sigaction(SIGALRM, &sa, 0);
	cb_init(&c1, p[1], fill, 1, 4711);	/* fails right away */
	cb_init(&c2, p[1], fill, sizeof fill, LIO_WRITE);
	alarm(1);
	errno = 0;
	if (lio_listio(LIO_WAIT, list, 2, 0) != -1 || errno != EINTR) {
		printf("  errno = %d\n", errno); return 1;
	}
	return 0;
}

static volatile sig_atomic_t got_signal;
static void usr1(int s) { (void)s; got_signal = 1; }

static int l_notify(void)
{
	int p[2]; struct aiocb c1, *list[1] = { &c1 };
	struct sigevent sev;
	char buf[8];
	int k;

	if (pipe(p)) return 90;
	if (write(p[1], "12345678", 8) != 8) return 90;
	signal(SIGUSR1, usr1);
	cb_init(&c1, p[0], buf, 8, LIO_READ);
	memset(&sev, 0, sizeof sev);
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGUSR1;
	if (lio_listio(LIO_NOWAIT, list, 1, &sev)) return 1;
	for (k = 0; k < 50 && !got_signal; k++)
		usleep(100000);
	if (!got_signal) { printf("  no notification\n"); return 2; }
	return 0;
}

/* Nothing was submitted, so nothing will complete: the call says EIO
   and there must be no notification to come later.  */
static int l_all_failed(void)
{
	struct aiocb c1, c2, *list[2] = { &c1, &c2 };
	struct sigevent sev;

	signal(SIGUSR1, usr1);
	cb_init(&c1, -1, fill, 1, LIO_READ);
	cb_init(&c2, -1, fill, 1, LIO_WRITE);
	memset(&sev, 0, sizeof sev);
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGUSR1;
	errno = 0;
	if (lio_listio(LIO_NOWAIT, list, 2, &sev) != -1 || errno != EIO) {
		printf("  errno = %d\n", errno); return 1;
	}
	usleep(300000);
	if (got_signal) { printf("  notified although nothing ran\n"); return 2; }
	return 0;
}

/* A list of nothing but LIO_NOP and NULL succeeds -- and its
   notification is due right away.  */
static volatile sig_atomic_t nop_ran;
static void nop_cb(union sigval v) { (void)v; nop_ran = 1; }

static int l_only_nop(void)
{
	struct aiocb c1, *list[2] = { &c1, 0 };
	struct sigevent sev;
	int k;

	cb_init(&c1, -1, 0, 0, LIO_NOP);
	memset(&sev, 0, sizeof sev);
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = nop_cb;
	if (lio_listio(LIO_NOWAIT, list, 2, &sev)) {
		printf("  lio_listio errno = %d\n", errno); return 1;
	}
	for (k = 0; k < 50 && !nop_ran; k++)
		usleep(100000);
	if (!nop_ran) { printf("  no notification\n"); return 2; }
	return 0;
}

/* The notification function runs the caller's code and must be able to
   take signals.  */
static volatile sig_atomic_t masked = -1, ran;

static void mask_cb(union sigval v)
{
	sigset_t set;

	(void)v;
	sigprocmask(SIG_BLOCK, 0, &set);
	masked = sigismember(&set, SIGUSR1) || sigismember(&set, SIGTERM);
	ran = 1;
}

static int l_notify_mask(void)
{
	int p[2]; struct aiocb c1, *list[1] = { &c1 };
	struct sigevent sev;
	int k;

	if (pipe(p)) return 90;
	cb_init(&c1, p[1], fill, 1, LIO_WRITE);
	memset(&sev, 0, sizeof sev);
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = mask_cb;
	if (lio_listio(LIO_NOWAIT, list, 1, &sev)) return 1;
	for (k = 0; k < 50 && !ran; k++)
		usleep(100000);
	if (!ran) { printf("  no callback\n"); return 2; }
	if (masked) { printf("  signals blocked in the callback\n"); return 3; }
	return 0;
}


/* The *64 names are symbols of their own, and the same functions.
   Not an address comparison: with a shared library the program sees
   its own PLT slot for each name, so the two differ although they end
   in one function.  Taking the addresses is what makes the linker
   resolve the symbols; calling one says it is the same function.  */
static int l_lfs64_names (void)
{
#ifdef __USE_LARGEFILE64
	void *taken[8];
	int p[2]; struct aiocb64 c1;
	char buf[8];

	taken[0] = (void *) aio_read64;
	taken[1] = (void *) aio_write64;
	taken[2] = (void *) aio_fsync64;
	taken[3] = (void *) aio_error64;
	taken[4] = (void *) aio_return64;
	taken[5] = (void *) aio_cancel64;
	taken[6] = (void *) aio_suspend64;
	taken[7] = (void *) lio_listio64;
	{
		size_t i;

		for (i = 0; i < 8; i++)
			if (taken[i] == NULL) {
				printf("  a *64 name resolved to NULL\n");
				return 1;
			}
	}

	if (pipe(p)) return 90;
	if (write(p[1], "12345678", 8) != 8) return 90;
	cb_init(&c1, p[0], buf, 8, LIO_READ);
	if (aio_read64(&c1)) { printf("  aio_read64 failed\n"); return 1; }
	while (aio_error64(&c1) == EINPROGRESS)
		usleep(1000);
	if (aio_error64(&c1) != aio_error(&c1)) {
		printf("  aio_error64 and aio_error disagree\n");
		return 2;
	}
	if (aio_return64(&c1) != 8) {
		printf("  aio_return64 gave the wrong count\n");
		return 3;
	}
#endif
	return 0;
}

static const struct { const char *name; int (*fn)(void); } cases[] = {
	{ "mode and count",	l_mode },
	{ "count too large",	l_count },
	{ "bad opcode",		l_opcode },
	{ "LIO_WAIT",		l_wait },
	{ "LIO_WAIT sleeps",	l_wait_cpu },
	{ "one bad entry",	l_bad_entry },
	{ "interrupted wait",	l_eintr },
	{ "notification",	l_notify },
	{ "all entries failed",	l_all_failed },
	{ "only LIO_NOP",	l_only_nop },
	{ "notify signal mask",	l_notify_mask },
	{ "*64 names",		l_lfs64_names },
};

int main(void)
{
	size_t i;
	int bad = 0;

	memset(fill, 'A', sizeof fill);
	setvbuf(stdout, 0, _IOLBF, 0);
	for (i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		pid_t pid = fork();
		int st, k;

		if (pid < 0) { perror("fork"); return 1; }
		if (pid == 0)
			_exit(cases[i].fn());
		for (k = 0; k < 150; k++) {	/* 15 s */
			usleep(100000);
			if (waitpid(pid, &st, WNOHANG) == pid)
				break;
		}
		if (k == 150) {
			kill(pid, SIGKILL); waitpid(pid, &st, 0);
			printf("HANG   %s\n", cases[i].name); bad++;
		} else if (!WIFEXITED(st)) {
			printf("SIGNAL %-20s %d\n", cases[i].name, WTERMSIG(st));
			bad++;
		} else if (WEXITSTATUS(st)) {
			printf("FAIL   %-20s code %d\n", cases[i].name,
			       WEXITSTATUS(st));
			bad++;
		} else
			printf("ok     %s\n", cases[i].name);
	}
	return bad != 0;
}
