/* clock_gettime leaves the caller's timespec alone when it fails.
   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.

   With 64-bit time on a 32-bit target the result arrives in a buffer of
   the library's and is copied out; that copy must not happen when the
   system call failed, or the caller gets whatever was on the stack.  */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void)
{
	struct timespec ts, before;
	int ret;

	memset(&ts, 0x5a, sizeof ts);
	before = ts;
	/* Positive clock ids past the kernel's MAX_CLOCKS are invalid; the
	   negative ones name CPU clocks of other processes.  */
	errno = 0;
	ret = clock_gettime((clockid_t) 1000, &ts);
	if (ret != -1 || errno != EINVAL) {
		printf("clock_gettime(1000) returned %d, errno %d\n", ret, errno);
		return 1;
	}
	if (memcmp(&ts, &before, sizeof ts) != 0) {
		printf("a failed clock_gettime changed the timespec\n");
		return 1;
	}
	return 0;
}
