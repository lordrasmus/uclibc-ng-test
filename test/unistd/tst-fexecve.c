/* fexecve runs the program behind a descriptor: one opened for reading,
   and one opened with O_PATH, which only execveat (AT_EMPTY_PATH) can
   take -- the /proc/self/fd fallback cannot exec it on every kernel.
   The program runs itself with "child" and expects exit status 42.  */

#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

static int
run (int fd, const char *how, char *self)
{
	char *args[] = { self, (char *) "child", NULL };
	int status;
	pid_t pid;

	pid = vfork ();
	if (pid == 0) {
		fexecve (fd, args, environ);
		_exit (errno == ENOSYS ? 77 : 127);
	}
	if (pid < 0) {
		perror ("vfork");
		return 1;
	}
	if (waitpid (pid, &status, 0) != pid) {
		perror ("waitpid");
		return 1;
	}
	if (!WIFEXITED (status) || WEXITSTATUS (status) != 42) {
		printf ("fexecve with a descriptor %s: status %#x\n", how, status);
		return 1;
	}
	return 0;
}

int
main (int argc, char **argv)
{
	int fd, err = 0;

	if (argc > 1 && strcmp (argv[1], "child") == 0)
		return 42;

	fd = open (argv[0], O_RDONLY | O_CLOEXEC);
	if (fd < 0) {
		perror (argv[0]);
		return 1;
	}
	err |= run (fd, "opened for reading", argv[0]);
	close (fd);

#ifdef O_PATH
	fd = open (argv[0], O_PATH | O_CLOEXEC);
	if (fd < 0) {
		perror (argv[0]);
		return 1;
	}
	err |= run (fd, "opened with O_PATH", argv[0]);
	close (fd);
#endif

	errno = 0;
	if (fexecve (-1, argv, environ) != -1 || errno != EINVAL) {
		printf ("fexecve (-1): errno %d, expected EINVAL\n", errno);
		err = 1;
	}
	return err;
}
