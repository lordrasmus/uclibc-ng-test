/* A static program that trylocks but never calls pthread_mutex_lock must
   still get the real lock behind uClibc's own locks.
   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.

   uClibc locks its internal mutexes through __pthread_mutex_lock and
   unlocks them through __pthread_mutex_unlock; __uClibc_main.o defines
   both as dummies for programs without threads.  pthread_mutex_trylock
   brings the real unlock, so it has to bring the real lock as well, or
   the two would not match.  The dummy lock locks nothing, which a
   trylock on the same mutex shows.  Linked statically (see
   Makefile.in): a shared libc has the real functions anyway.  */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>

extern int __pthread_mutex_lock (pthread_mutex_t *);

int main(void)
{
	pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
	int r;

	r = pthread_mutex_trylock(&m);
	if (r != 0) {
		printf("trylock on a free mutex: %d\n", r);
		return 1;
	}
	pthread_mutex_unlock(&m);

	if (__pthread_mutex_lock(&m) != 0) {
		puts("__pthread_mutex_lock failed");
		return 1;
	}
	r = pthread_mutex_trylock(&m);
	if (r != EBUSY) {
		printf("trylock after the internal lock: %d, not EBUSY; "
		       "the internal lock is the dummy\n", r);
		return 1;
	}
	pthread_mutex_unlock(&m);
	return 0;
}
