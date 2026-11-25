/* aio_suspend: the timeout it accepts and the one it keeps.
   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.  */

#include <aio.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
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

/* A timeout that is not a valid timespec is EINVAL, whether or not a
   request is outstanding.  */
static int bad_timeout(long sec, long nsec)
{
	int p[2]; struct aiocb c1; const struct aiocb *list[1] = { &c1 };
	struct timespec ts = { sec, nsec };

	if (pipe(p)) return 90;
	fill_pipe(p[1]);
	cb_init(&c1, p[1], fill, sizeof fill);
	if (aio_write(&c1)) return 91;
	errno = 0;
	if (aio_suspend(list, 1, &ts) != -1 || errno != EINVAL) {
		printf("  aio_suspend errno = %d\n", errno);
		return 1;
	}
	return 0;
}

static int s_nsec_neg(void)	{ return bad_timeout(0, -1); }
static int s_nsec_big(void)	{ return bad_timeout(0, 2500000000L); }
static int s_nsec_12(void)	{ return bad_timeout(0, 1200000000L); }
static int s_sec_neg(void)	{ return bad_timeout(-1, 0); }

/* A valid timeout has to expire, and roughly on time.  */
static int s_timeout(void)
{
	int p[2]; struct aiocb c1; const struct aiocb *list[1] = { &c1 };
	struct timespec ts = { 0, 300000000L }, a, b;
	long ms;

	if (pipe(p)) return 90;
	fill_pipe(p[1]);
	cb_init(&c1, p[1], fill, sizeof fill);
	if (aio_write(&c1)) return 91;
	clock_gettime(CLOCK_MONOTONIC, &a);
	errno = 0;
	if (aio_suspend(list, 1, &ts) != -1 || errno != EAGAIN) {
		printf("  aio_suspend errno = %d\n", errno); return 1;
	}
	clock_gettime(CLOCK_MONOTONIC, &b);
	ms = (b.tv_sec - a.tv_sec) * 1000 + (b.tv_nsec - a.tv_nsec) / 1000000;
	if (ms < 250) { printf("  returned after %ld ms\n", ms); return 2; }
	return 0;
}

/* A timeout of zero polls.  */
static int s_poll(void)
{
	int p[2]; struct aiocb c1; const struct aiocb *list[1] = { &c1 };
	struct timespec ts = { 0, 0 };

	if (pipe(p)) return 90;
	fill_pipe(p[1]);
	cb_init(&c1, p[1], fill, sizeof fill);
	if (aio_write(&c1)) return 91;
	errno = 0;
	if (aio_suspend(list, 1, &ts) != -1 || errno != EAGAIN) {
		printf("  aio_suspend errno = %d\n", errno); return 1;
	}
	return 0;
}

/* A finished request ends the wait at once, even behind NULL entries.  */
static int s_done(void)
{
	int p[2]; struct aiocb c1; const struct aiocb *list[3] = { 0, &c1, 0 };
	char buf[8];

	if (pipe(p)) return 90;
	if (write(p[1], "12345678", 8) != 8) return 90;
	cb_init(&c1, p[0], buf, 8);
	if (aio_read(&c1)) return 91;
	if (aio_suspend(list, 3, 0)) {
		printf("  aio_suspend errno = %d\n", errno); return 1;
	}
	if (aio_error(&c1) == EINPROGRESS) return 2;
	return 0;
}

static const struct { const char *name; int (*fn)(void); } cases[] = {
	{ "tv_nsec -1",		s_nsec_neg },
	{ "tv_nsec 2.5e9",	s_nsec_big },
	{ "tv_nsec 1.2e9",	s_nsec_12 },
	{ "tv_sec -1",		s_sec_neg },
	{ "timeout expires",	s_timeout },
	{ "zero timeout polls",	s_poll },
	{ "finished request",	s_done },
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
