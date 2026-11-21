/* posix_devctl, POSIX.1-2024.

   Linux has no devctl, so this is ioctl with the size argument dropped
   and the driver's answer handed back as a return value instead of in
   errno.  That mapping is what gets checked: a call that works gives 0,
   a call that fails gives the error number, and errno is not where the
   answer lives.

   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.  */

#include <stdio.h>

/* <devctl.h> is POSIX.1-2024 and new; glibc does not install one.  */
#if defined __UCLIBC__ && defined __UCLIBC_HAS_ADVANCED_REALTIME__
# define HAVE_DEVCTL 1
#endif

#ifndef HAVE_DEVCTL

int main(void)
{
# if defined __UCLIBC__
	printf("SKIP: UCLIBC_HAS_ADVANCED_REALTIME is off\n");
# else
	printf("SKIP: no <devctl.h> in this libc\n");
# endif
	return 0;
}

#else

# include <devctl.h>
# include <errno.h>
# include <fcntl.h>
# include <string.h>
# include <termios.h>
# include <unistd.h>
# include <sys/ioctl.h>

static int fails;

int main(void)
{
	int fd, r, dev_info_ptr, pending, pipefd[2];
	struct termios t;

	/* FIONREAD on a pipe says how much is waiting, so the call can be
	   checked by its result and not just by its return value.  A pipe
	   is used because FIONREAD is not required of every device.  */
	if (pipe(pipefd) != 0) {
		printf("SKIP: pipe: %s\n", strerror(errno));
		return 0;
	}
	if (write(pipefd[1], "hello", 5) != 5) {
		printf("SKIP: write to pipe: %s\n", strerror(errno));
		return 0;
	}
	pending = -1;
	dev_info_ptr = 0x55;
	errno = 0;
	r = posix_devctl(pipefd[0], FIONREAD, &pending, sizeof(pending),
			 &dev_info_ptr);
	if (r != 0) {
		printf("FAIL: FIONREAD on a pipe: %d (%s)\n",
		       r, strerror(r));
		fails++;
	} else if (pending != 5) {
		printf("FAIL: FIONREAD says %d bytes, wrote 5\n", pending);
		fails++;
	} else {
		printf("PASS: FIONREAD on a pipe -> 5 bytes waiting\n");
	}
	close(pipefd[0]);
	close(pipefd[1]);
	fd = pipefd[0];

	/* A closed descriptor: the error comes back as the return value,
	   not through errno.  */
	errno = 0;
	r = posix_devctl(fd, FIONREAD, &dev_info_ptr, sizeof(dev_info_ptr),
			 NULL);
	if (r != EBADF) {
		printf("FAIL: closed fd: %d (%s), expected EBADF\n",
		       r, strerror(r));
		fails++;
	} else {
		printf("PASS: closed fd -> EBADF\n");
	}

	/* A terminal request on something that is not a terminal.  */
	fd = open("/dev/null", O_RDONLY);
	if (fd >= 0) {
		r = posix_devctl(fd, TCGETS, &t, sizeof(t), NULL);
		if (r != ENOTTY && r != EINVAL) {
			printf("FAIL: TCGETS on /dev/null: %d (%s), "
			       "expected ENOTTY\n", r, strerror(r));
			fails++;
		} else {
			printf("PASS: TCGETS on /dev/null -> %s\n",
			       strerror(r));
		}
		close(fd);
	}

	if (fails) {
		printf("%d failure(s)\n", fails);
		return 1;
	}
	printf("all ok\n");
	return 0;
}

#endif
