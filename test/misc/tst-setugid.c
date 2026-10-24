/* A setuid or setgid program gets its closed standard descriptors opened
   onto /dev/null, and sees issetugid () == 1 and secure_getenv () == NULL.

   As root, the test copies itself, makes the copy setuid root and then
   setgid root, drops to an unprivileged user and starts the copy with
   standard input closed.  The copy checks and answers through its exit
   status.  Without root, or where the file system ignores the set-id bits,
   it says SKIP.

   glibc has no issetugid; there only the other two are checked.

   vfork rather than fork, so that it runs on noMMU targets as well; the
   child only changes its credentials, closes a descriptor and execs.  */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define COPY	"tst-setugid.copy"
#define VAR	"TST_SETUGID"
#define NOBODY	65534

/* uClibc-ng defines __GLIBC__ too.  */
#if defined __UCLIBC__ || !defined __GLIBC__
# define HAVE_ISSETUGID 1
#endif

#define BAD_FD		1
#define BAD_ISSETUGID	2
#define BAD_GETENV	4
#define NOT_SETID	77

static int child(void)
{
	int bad = 0;

	if (geteuid() == getuid() && getegid() == getgid())
		return NOT_SETID;
	if (fcntl(0, F_GETFD) == -1)
		bad |= BAD_FD;
#ifdef HAVE_ISSETUGID
	if (issetugid() != 1)
		bad |= BAD_ISSETUGID;
#endif
	if (getenv(VAR) == NULL || secure_getenv(VAR) != NULL)
		bad |= BAD_GETENV;
	return bad;
}

static int copy_self(const char *self)
{
	char buf[4096];
	ssize_t n;
	int in, out;

	in = open("/proc/self/exe", O_RDONLY);
	if (in < 0)
		in = open(self, O_RDONLY);
	if (in < 0)
		return -1;
	out = open(COPY, O_WRONLY | O_CREAT | O_TRUNC, 0700);
	if (out < 0) {
		close(in);
		return -1;
	}
	while ((n = read(in, buf, sizeof buf)) > 0)
		if (write(out, buf, n) != n) {
			n = -1;
			break;
		}
	close(in);
	if (close(out) != 0 || n < 0)
		return -1;
	return 0;
}

/* 0 passed, 1 failed, 2 skipped.  */
static int run(const char *what, mode_t mode)
{
	int status;
	pid_t pid;

	if (chown(COPY, 0, 0) != 0 || chmod(COPY, mode) != 0) {
		printf("%s: chown/chmod: %s\n", what, strerror(errno));
		return 1;
	}

	pid = vfork();
	if (pid == 0) {
		if (setgid(NOBODY) != 0 || setuid(NOBODY) != 0)
			_exit(126);
		close(0);
		execl(COPY, COPY, "child", (char *) NULL);
		_exit(127);
	}
	if (pid < 0 || waitpid(pid, &status, 0) != pid) {
		printf("%s: vfork/waitpid: %s\n", what, strerror(errno));
		return 1;
	}
	if (!WIFEXITED(status)) {
		printf("%s: child did not exit normally (status %#x)\n", what, status);
		return 1;
	}
	switch (WEXITSTATUS(status)) {
	case 0:
		return 0;
	case NOT_SETID:
		printf("SKIP %s: the file system ignores the set-id bit\n", what);
		return 2;
	case 126:
	case 127:
		printf("%s: could not drop privileges or exec (%d)\n", what,
		       WEXITSTATUS(status));
		return 1;
	}
	if (WEXITSTATUS(status) & BAD_FD)
		printf("%s: closed descriptor 0 was not opened onto /dev/null\n", what);
	if (WEXITSTATUS(status) & BAD_ISSETUGID)
		printf("%s: issetugid () is not 1\n", what);
	if (WEXITSTATUS(status) & BAD_GETENV)
		printf("%s: secure_getenv () returned the variable\n", what);
	return 1;
}

int main(int argc, char **argv)
{
	int r, failed = 0;

	if (argc > 1 && strcmp(argv[1], "child") == 0)
		return child();

	if (getuid() != 0) {
		printf("SKIP: needs root\n");
		return 0;
	}
	if (copy_self(argv[0]) != 0) {
		printf("cannot copy the test program: %s\n", strerror(errno));
		return 1;
	}
	setenv(VAR, "1", 1);

	r = run("setuid", S_ISUID | 0755);
	failed |= r == 1;
	r = run("setgid", S_ISGID | 0755);
	failed |= r == 1;

	unlink(COPY);
	return failed;
}
