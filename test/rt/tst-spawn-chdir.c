/* posix_spawn_file_actions_addchdir and _addfchdir, POSIX.1-2024.

   The child prints its working directory after the action, so what it prints says where
   it ended up.  That is the one thing worth checking: the directory has
   to change in the child and not in the caller, and it has to change at
   that point in the action list, so a later addopen sees it.

   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.  */

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* glibc has had these under the _np names since 2.29, before POSIX took
   them in; test those there, so the expectations below are checked
   against the implementation everyone compares to.  */
#if !defined __UCLIBC__ && defined __GLIBC__
# define posix_spawn_file_actions_addchdir posix_spawn_file_actions_addchdir_np
# define posix_spawn_file_actions_addfchdir posix_spawn_file_actions_addfchdir_np
# define HAVE_SPAWN_CHDIR 1
#elif defined __UCLIBC__ && defined __UCLIBC_HAS_ADVANCED_REALTIME__
# define HAVE_SPAWN_CHDIR 1
#endif

#ifndef HAVE_SPAWN_CHDIR

int main(void)
{
# if defined __UCLIBC__
	printf("SKIP: UCLIBC_HAS_ADVANCED_REALTIME is off\n");
# else
	printf("SKIP: no posix_spawn_file_actions_addchdir in this libc\n");
# endif
	return 0;
}

#else

extern char **environ;

static int fails;
static char dir[] = "/tmp/tst-spawn-chdir.XXXXXX";

static void
cleanup(void)
{
	char p[sizeof(dir) + 16];

	snprintf(p, sizeof(p), "%s/out", dir);
	unlink(p);
	rmdir(dir);
}

/* Spawn a shell that prints its working directory and give back what it
   said.  /bin/sh is used rather than /bin/pwd because a busybox rootfs
   has the one and not always the other.  */
static int
pwd_says(posix_spawn_file_actions_t *fa, char *out, size_t len)
{
	char *argv[] = { (char *) "sh", (char *) "-c", (char *) "pwd", NULL };
	int pipefd[2], status, r;
	pid_t pid;
	ssize_t n;

	if (pipe(pipefd) != 0) {
		printf("FAIL: pipe: %s\n", strerror(errno));
		return -1;
	}
	if (posix_spawn_file_actions_adddup2(fa, pipefd[1], 1) != 0) {
		printf("FAIL: adddup2\n");
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}
	r = posix_spawn(&pid, "/bin/sh", fa, NULL, argv, environ);
	close(pipefd[1]);
	if (r != 0) {
		printf("SKIP: cannot spawn /bin/sh: %s\n", strerror(r));
		close(pipefd[0]);
		return -2;
	}
	n = read(pipefd[0], out, len - 1);
	close(pipefd[0]);
	if (waitpid(pid, &status, 0) < 0) {
		printf("FAIL: waitpid: %s\n", strerror(errno));
		return -1;
	}
	if (n <= 0) {
		printf("FAIL: child printed nothing\n");
		return -1;
	}
	out[n] = '\0';
	while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r'))
		out[--n] = '\0';
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		printf("FAIL: child status %d\n", status);
		return -1;
	}
	return 0;
}

int main(void)
{
	posix_spawn_file_actions_t fa;
	char got[512], before[512], p[sizeof(dir) + 16];
	int fd, r;

	if (getcwd(before, sizeof(before)) == NULL) {
		printf("FAIL: getcwd: %s\n", strerror(errno));
		return 1;
	}
	if (mkdtemp(dir) == NULL) {
		printf("SKIP: mkdtemp: %s\n", strerror(errno));
		return 0;
	}
	atexit(cleanup);

	/* addchdir: the child ends up in dir.  */
	if (posix_spawn_file_actions_init(&fa) != 0) {
		printf("FAIL: init\n");
		return 1;
	}
	r = posix_spawn_file_actions_addchdir(&fa, dir);
	if (r != 0) {
		printf("FAIL: addchdir: %s\n", strerror(r));
		posix_spawn_file_actions_destroy(&fa);
		return 1;
	}
	r = pwd_says(&fa, got, sizeof(got));
	posix_spawn_file_actions_destroy(&fa);
	if (r == -2)
		return 0;
	if (r == 0) {
		if (strcmp(got, dir) != 0) {
			printf("FAIL: addchdir: child in %s, expected %s\n",
			       got, dir);
			fails++;
		} else {
			printf("PASS: addchdir -> %s\n", got);
		}
	} else {
		fails++;
	}

	/* addfchdir: same, by descriptor.  */
	fd = open(dir, O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		printf("FAIL: open %s: %s\n", dir, strerror(errno));
		return 1;
	}
	if (posix_spawn_file_actions_init(&fa) != 0) {
		printf("FAIL: init\n");
		return 1;
	}
	r = posix_spawn_file_actions_addfchdir(&fa, fd);
	if (r != 0) {
		printf("FAIL: addfchdir: %s\n", strerror(r));
		posix_spawn_file_actions_destroy(&fa);
		close(fd);
		return 1;
	}
	r = pwd_says(&fa, got, sizeof(got));
	posix_spawn_file_actions_destroy(&fa);
	if (r == 0) {
		if (strcmp(got, dir) != 0) {
			printf("FAIL: addfchdir: child in %s, expected %s\n",
			       got, dir);
			fails++;
		} else {
			printf("PASS: addfchdir -> %s\n", got);
		}
	} else if (r != -2) {
		fails++;
	}
	close(fd);

	/* The caller must not have moved.  */
	if (getcwd(got, sizeof(got)) == NULL || strcmp(got, before) != 0) {
		printf("FAIL: caller is in %s, was in %s\n", got, before);
		fails++;
	} else {
		printf("PASS: caller did not move\n");
	}

	/* The action list is ordered: a file opened after the chdir is
	   created in the new directory, by a relative name.  */
	if (posix_spawn_file_actions_init(&fa) != 0) {
		printf("FAIL: init\n");
		return 1;
	}
	if (posix_spawn_file_actions_addchdir(&fa, dir) != 0
	    || posix_spawn_file_actions_addopen(&fa, 3, "out",
						O_WRONLY | O_CREAT, 0600)
	       != 0) {
		printf("FAIL: addchdir+addopen\n");
		posix_spawn_file_actions_destroy(&fa);
		return 1;
	}
	r = pwd_says(&fa, got, sizeof(got));
	posix_spawn_file_actions_destroy(&fa);
	if (r == 0) {
		struct stat st;

		snprintf(p, sizeof(p), "%s/out", dir);
		if (stat(p, &st) != 0) {
			printf("FAIL: %s was not created: %s\n", p,
			       strerror(errno));
			fails++;
		} else {
			printf("PASS: addopen after addchdir created %s\n", p);
		}
	} else if (r != -2) {
		fails++;
	}

	/* A negative descriptor is the one error the standard names:
	   "[EBADF] The value specified by fildes is negative."  glibc's
	   _np version does not check and returns 0, so there this is
	   reported rather than counted.  */
	if (posix_spawn_file_actions_init(&fa) != 0) {
		printf("FAIL: init\n");
		return 1;
	}
	r = posix_spawn_file_actions_addfchdir(&fa, -1);
	if (r == EBADF) {
		printf("PASS: addfchdir(-1) -> EBADF\n");
	} else {
#if defined __UCLIBC__
		printf("FAIL: addfchdir(-1): %d (%s), expected EBADF\n",
		       r, strerror(r));
		fails++;
#else
		printf("INFO: addfchdir(-1) returned %d (%s); POSIX.1-2024 "
		       "asks for EBADF\n", r, strerror(r));
#endif
	}

	/* A descriptor that is merely not open yet is not an error here:
	   it has to be open in the child, which has its own table.  */
	r = posix_spawn_file_actions_addfchdir(&fa, 9999);
	if (r != 0) {
		printf("FAIL: addfchdir(9999): %d (%s), expected 0 -- a "
		       "closed number is not an error at this point\n",
		       r, strerror(r));
		fails++;
	} else {
		printf("PASS: addfchdir(9999) accepted\n");
	}
	posix_spawn_file_actions_destroy(&fa);

	if (fails) {
		printf("%d failure(s)\n", fails);
		return 1;
	}
	printf("all ok\n");
	return 0;
}

#endif
