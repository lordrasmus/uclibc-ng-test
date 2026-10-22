/* timespec_getres (ISO C23).  Built with only _ISOC23_SOURCE (see
   Makefile.in), so it also checks that <time.h> declares it then.  */

#include <stdio.h>
#include <time.h>

int main(void)
{
	struct timespec res;
	int err = 0;

	if (timespec_getres(&res, TIME_UTC) != TIME_UTC) {
		puts("timespec_getres(TIME_UTC) did not return TIME_UTC");
		return 1;
	}
	/* some resolution, and not coarser than a second */
	if (res.tv_sec != 0 || res.tv_nsec <= 0 || res.tv_nsec >= 1000000000) {
		printf("resolution %lld.%09ld is not plausible\n",
		       (long long)res.tv_sec, res.tv_nsec);
		err = 1;
	}

	/* ts may be null: then it only says whether the base is supported */
	if (timespec_getres(NULL, TIME_UTC) != TIME_UTC) {
		puts("timespec_getres(NULL, TIME_UTC) did not return TIME_UTC");
		err = 1;
	}
	if (timespec_getres(&res, 0) != 0) {
		puts("timespec_getres with base 0 did not return 0");
		err = 1;
	}

	return err;
}
