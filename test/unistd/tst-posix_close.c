/* posix_close (POSIX.1-2024): closes like close, refuses an unknown flag
   without closing.  */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
	int fds[2], err = 0;

	if (pipe(fds) != 0) {
		perror("pipe");
		return 1;
	}

	errno = 0;
	if (posix_close(fds[0], 1 | POSIX_CLOSE_RESTART) != -1 || errno != EINVAL) {
		puts("an invalid flag did not fail with EINVAL");
		err = 1;
	}
	if (fcntl(fds[0], F_GETFD) == -1) {
		puts("an invalid flag closed the descriptor");
		err = 1;
	}

	if (posix_close(fds[0], 0) != 0) {
		perror("posix_close(fd, 0)");
		err = 1;
	}
	if (fcntl(fds[0], F_GETFD) != -1 || errno != EBADF) {
		puts("posix_close(fd, 0) left the descriptor open");
		err = 1;
	}

	if (posix_close(fds[1], POSIX_CLOSE_RESTART) != 0) {
		perror("posix_close(fd, POSIX_CLOSE_RESTART)");
		err = 1;
	}

	errno = 0;
	if (posix_close(-1, 0) != -1 || errno != EBADF) {
		puts("posix_close(-1, 0) did not fail with EBADF");
		err = 1;
	}

	return err;
}
