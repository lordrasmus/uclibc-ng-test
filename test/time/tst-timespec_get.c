/* timespec_get (ISO C11).  Built as strict C11 (see Makefile.in), so it
   also checks that <time.h> gives struct timespec, TIME_UTC and
   timespec_get to a program that asks for nothing but C11.  */

#include <stdio.h>
#include <time.h>

int main(void)
{
	struct timespec ts;
	time_t before, after;
	int err = 0;

	before = time(NULL);
	if (timespec_get(&ts, TIME_UTC) != TIME_UTC) {
		puts("timespec_get(TIME_UTC) did not return TIME_UTC");
		return 1;
	}
	after = time(NULL);

	if (ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000) {
		printf("tv_nsec out of range: %ld\n", ts.tv_nsec);
		err = 1;
	}
	/* time() may read the coarse clock, up to a tick behind.  */
	if (ts.tv_sec < before - 1 || ts.tv_sec > after + 1) {
		printf("tv_sec %lld not within time() %lld..%lld\n",
		       (long long)ts.tv_sec, (long long)before,
		       (long long)after);
		err = 1;
	}

	/* 0 is no valid base; the result is then 0.  */
	if (timespec_get(&ts, 0) != 0) {
		puts("timespec_get with base 0 did not return 0");
		err = 1;
	}

	return err;
}
