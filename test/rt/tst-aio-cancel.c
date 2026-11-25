/* aio_cancel: what it reports and what it leaves behind.
   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.

   Every case runs in a child of its own, because the ones that found
   bugs here hung rather than failed.  */

#include <aio.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
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

static void cb_init(struct aiocb *cb, int fd, void *buf, size_t n)
{
	memset(cb, 0, sizeof *cb);
	cb->aio_fildes = fd;
	cb->aio_buf = buf;
	cb->aio_nbytes = n;
}

/* A request still queued behind another one.  Cancelling it must not
   wait for the thread that holds it: that thread needs the queue lock
   aio_cancel is holding.  */
static int c_queued(void)
{
	int p[2]; struct aiocb c1, c2; int r;

	if (pipe(p)) return 90;
	fill_pipe(p[1]);
	cb_init(&c1, p[1], fill, sizeof fill);
	cb_init(&c2, p[1], fill, 1);
	if (aio_write(&c1) || aio_write(&c2)) return 91;
	usleep(200000);
	r = aio_cancel(p[1], &c2);
	if (r != AIO_CANCELED) { printf("  aio_cancel = %d\n", r); return 1; }
	if (aio_error(&c2) != ECANCELED) {
		printf("  aio_error = %d\n", aio_error(&c2)); return 2;
	}
	if (aio_return(&c2) != -1) return 3;
	return 0;
}

static int c_running(void)
{
	int p[2]; struct aiocb c1; int r;

	if (pipe(p)) return 90;
	fill_pipe(p[1]);
	cb_init(&c1, p[1], fill, sizeof fill);
	if (aio_write(&c1)) return 91;
	usleep(200000);
	r = aio_cancel(p[1], &c1);
	/* POSIX allows AIO_NOTCANCELED once the request has started.  */
	if (r != AIO_CANCELED && r != AIO_NOTCANCELED) {
		printf("  aio_cancel = %d\n", r); return 1;
	}
	if (r == AIO_CANCELED && aio_error(&c1) != ECANCELED) return 2;
	return 0;
}

static int c_all(void)			/* cb == NULL */
{
	int p[2]; struct aiocb c[3]; int i, r;

	if (pipe(p)) return 90;
	fill_pipe(p[1]);
	for (i = 0; i < 3; i++) {
		cb_init(&c[i], p[1], fill, sizeof fill);
		if (aio_write(&c[i])) return 91;
	}
	usleep(200000);
	r = aio_cancel(p[1], NULL);
	if (r != AIO_CANCELED && r != AIO_NOTCANCELED) {
		printf("  aio_cancel = %d\n", r); return 1;
	}
	for (i = 0; i < 3; i++) {
		int k;
		for (k = 0; k < 50 && aio_error(&c[i]) == EINPROGRESS; k++)
			usleep(100000);
		if (aio_error(&c[i]) == EINPROGRESS) return 2;
	}
	return 0;
}

static int c_done(void)			/* nothing left to cancel */
{
	int p[2]; struct aiocb c1;

	if (pipe(p)) return 90;
	cb_init(&c1, p[1], fill, 1);
	if (aio_write(&c1)) return 91;
	while (aio_error(&c1) == EINPROGRESS)
		usleep(1000);
	if (aio_cancel(p[1], &c1) != AIO_ALLDONE) return 1;
	return 0;
}

static int c_wrong_fd(void)
{
	int p[2]; struct aiocb c1;

	if (pipe(p)) return 90;
	cb_init(&c1, p[1], fill, 1);
	errno = 0;
	if (aio_cancel(p[0], &c1) != -1 || errno != EINVAL) return 1;
	return 0;
}

static int c_bad_fd(void)
{
	int p[2];

	if (pipe(p)) return 90;
	close(p[0]); close(p[1]);
	errno = 0;
	if (aio_cancel(p[0], NULL) != -1 || errno != EBADF) {
		printf("  errno = %d\n", errno); return 1;
	}
	return 0;
}

static int c_idle(void)			/* an open fd with nothing queued */
{
	int p[2];

	if (pipe(p)) return 90;
	if (aio_cancel(p[1], NULL) != AIO_ALLDONE) return 1;
	return 0;
}

static int c_twice(void)
{
	int p[2]; struct aiocb c1; int r1, r2;

	if (pipe(p)) return 90;
	fill_pipe(p[1]);
	cb_init(&c1, p[1], fill, sizeof fill);
	if (aio_write(&c1)) return 91;
	usleep(200000);
	r1 = aio_cancel(p[1], &c1);
	r2 = aio_cancel(p[1], &c1);
	if (r1 != AIO_CANCELED && r1 != AIO_NOTCANCELED) return 1;
	if (r2 != AIO_ALLDONE && r2 != AIO_NOTCANCELED) {
		printf("  second aio_cancel = %d\n", r2); return 2;
	}
	return 0;
}

/* The block is the caller's again the moment aio_cancel says CANCELED:
   it has to survive being reused right away.  */
static int c_reuse(void)
{
	int p[2]; struct aiocb c1, c2; int r;

	if (pipe(p)) return 90;
	fill_pipe(p[1]);
	cb_init(&c1, p[1], fill, sizeof fill);
	cb_init(&c2, p[1], fill, 1);
	if (aio_write(&c1) || aio_write(&c2)) return 91;
	usleep(200000);
	r = aio_cancel(p[1], &c2);
	if (r != AIO_CANCELED) return 1;
	memset(&c2, 0x5a, sizeof c2);	/* scribble over it */
	usleep(300000);
	return 0;
}

/* A notification function that leaves through pthread_exit must not
   send the worker's cleanup handler through a second time: that would
   publish into an aiocb its owner has had back and unlink from a queue
   that is gone.  The visible half of it is the callback itself, which
   has to run exactly once.  */
static volatile sig_atomic_t notified;

static void exit_cb(union sigval v) { (void)v; notified++; pthread_exit(0); }

static int c_notify_exit(void)
{
	int p[2]; struct aiocb c1; int k;

	if (pipe(p)) return 90;
	fill_pipe(p[1]);
	cb_init(&c1, p[1], fill, sizeof fill);
	c1.aio_sigevent.sigev_notify = SIGEV_THREAD;
	c1.aio_sigevent.sigev_notify_function = exit_cb;
	if (aio_write(&c1)) return 91;
	usleep(200000);
	aio_cancel(p[1], &c1);
	for (k = 0; k < 30 && aio_error(&c1) == EINPROGRESS; k++)
		usleep(100000);
	if (aio_error(&c1) == EINPROGRESS) { printf("  stuck\n"); return 1; }
	usleep(300000);
	if (notified != 1) {
		printf("  the callback ran %d times\n", (int)notified);
		return 2;
	}
	close(p[0]);
	return 0;
}

/* The notification function runs the caller's code, so it must be able
   to take signals; the worker thread has them all blocked.  */
static volatile sig_atomic_t masked = -1, ran;

static void mask_cb(union sigval v)
{
	sigset_t set;

	(void)v;
	sigprocmask(SIG_BLOCK, 0, &set);
	masked = sigismember(&set, SIGUSR1) || sigismember(&set, SIGTERM);
	ran = 1;
}

static int c_notify_mask(void)
{
	int p[2]; struct aiocb c1; int k;

	if (pipe(p)) return 90;
	cb_init(&c1, p[1], fill, 1);
	c1.aio_sigevent.sigev_notify = SIGEV_THREAD;
	c1.aio_sigevent.sigev_notify_function = mask_cb;
	if (aio_write(&c1)) return 91;
	for (k = 0; k < 50 && !ran; k++)
		usleep(100000);
	if (!ran) { printf("  no callback\n"); return 1; }
	if (masked) { printf("  signals blocked in the callback\n"); return 2; }
	return 0;
}

static const struct { const char *name; int (*fn)(void); } cases[] = {
	{ "queued",		c_queued },
	{ "running",		c_running },
	{ "all",		c_all },
	{ "already done",	c_done },
	{ "wrong fd",		c_wrong_fd },
	{ "closed fd",		c_bad_fd },
	{ "idle fd",		c_idle },
	{ "twice",		c_twice },
	{ "block reused",	c_reuse },
	{ "notify pthread_exit", c_notify_exit },
	{ "notify signal mask",	c_notify_mask },
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
