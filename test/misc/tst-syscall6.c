#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/uio.h>
#include <sys/types.h>
#include <linux/fs.h> /* for RWF_HIPRI */

int main()
{
#if defined SYS_preadv2 && defined SYS_pwritev2 && defined RWF_HIPRI
	char tmp[] = "/tmp/tst-preadv2-XXXXXX";
	int fd;
	struct iovec iov[2];
	char *str0 = "hello ";
	char *str1 = "world\n";
	char input[16];
	long nio;
	int bad = 0;

	fd = mkstemp (tmp);
	if (fd == -1) {
		puts ("mkstemp failed");
		return 1;
	}
	/* Remove the name immediately, including when an assertion fails. */
	if (unlink(tmp) != 0) {
		perror("unlink");
		close(fd);
		return 1;
	}
	if (lseek(fd, 3, SEEK_SET) != 3) {
		perror("lseek");
		close(fd);
		return 1;
	}

	iov[0].iov_base = str0;
	iov[0].iov_len = strlen(str0);
	iov[1].iov_base = str1;
	iov[1].iov_len = strlen(str1) + 1; /* null terminator */

	nio = syscall(SYS_pwritev2, fd, iov, 2, 0, 0, RWF_DSYNC);
	if (nio == -1 && errno == ENOSYS) {
		puts("pwritev2 is not implemented here");
		close(fd);
		return 23;
	}

	if (nio != (long) (strlen(str0) + strlen(str1) + 1)) {
		printf ("pwritev2 wrote %ld bytes, expected %zu (errno %d)\n", nio,
			strlen(str0) + strlen(str1) + 1, errno);
		bad = 1;
		goto out;
	}
	if (lseek(fd, 0, SEEK_CUR) != 3) {
		puts("pwritev2 changed the file position");
		bad = 1;
	}

	/* Read in the second string into the first buffer */
	iov[0].iov_base = input;
	iov[0].iov_len = strlen(str1) + 1; /* null terminator */
	memset(input, 0xa5, sizeof input);
	nio = syscall(SYS_preadv2, fd, iov, 1, strlen(str0), 0, RWF_HIPRI);
	if (nio != (long) strlen(str1) + 1) {
		printf ("preadv2 read %ld bytes, expected %zu (errno %d)\n",
			nio, strlen(str1) + 1, errno);
		bad = 1;
		goto out;
	}

	if (memcmp(input, str1, strlen(str1) + 1) != 0) {
		puts("preadv2 returned different data");
		bad = 1;
	} else
		printf ("syscall(SYS_preadv2) read %s", (char *) iov[0].iov_base);
	if ((unsigned char) input[strlen(str1) + 1] != 0xa5) {
		puts("preadv2 wrote past its buffer length");
		bad = 1;
	}
	if (lseek(fd, 0, SEEK_CUR) != 3) {
		puts("preadv2 changed the file position");
		bad = 1;
	}

out:
	if (close(fd) != 0) {
		puts ("failed to close read fd");
		return 1;
	}

	return bad;
#else
  return 23;
#endif
}
