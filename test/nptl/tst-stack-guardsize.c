/* The guard of a thread stack comes on top of the requested stack size.
   POSIX: with guard protection "the implementation allocates extra memory
   at the overflow end of the stack".  So a thread created with a 256 KB
   stack and a 64 KB guard must be able to use (nearly) 256 KB, and a
   guard as large as the default stack must not make pthread_create fail.
   glibc does this since 2.27 (BZ #22637); uClibc-ng, like musl, also
   adds static TLS and the thread descriptor, which this test checks more
   tightly there.  */

#include <alloca.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define STACK	(256 * 1024)
#define GUARD	(64 * 1024)
/* Room for the thread's own frames.  uClibc-ng puts the descriptor and
   static TLS on top of the request as well, as musl does; glibc still
   takes them out of it, so it gets more room.  */
#ifdef __UCLIBC__
# define SLACK	(4 * 1024)
#else
# define SLACK	(16 * 1024)
#endif

/* Without an MMU nothing protects a guard; uClibc-ng keeps it to one page
   and pthread_getattr_np reports that.  __ARCH_USE_MMU__ is uClibc-ng's.  */
#if defined __UCLIBC__ && !defined __ARCH_USE_MMU__
# define EXPECTED_GUARD(g, ps)	((g) > (ps) ? (ps) : (g))
#else
# define EXPECTED_GUARD(g, ps)	(g)
#endif

static void *use_stack(void *arg)
{
	size_t n = (size_t) arg;
	volatile char *p = alloca(n);

	memset((char *) p, 0x5a, n);
	return (void *) (unsigned long) p[n / 2];
}

static void *report(void *arg)
{
	pthread_attr_t *a = arg;

	return (void *) (long) pthread_getattr_np(pthread_self(), a);
}

static size_t round_up(size_t n, size_t ps)
{
	return (n + ps - 1) / ps * ps;
}

static int do_test(void)
{
	size_t ps = sysconf(_SC_PAGESIZE);
	size_t guard = round_up(GUARD, ps), stack = round_up(STACK, ps);
	size_t got, dflt;
	pthread_attr_t a, r;
	pthread_t t;
	void *res;
	int ret, err = 0;

	if (stack < PTHREAD_STACK_MIN)
		stack = round_up(PTHREAD_STACK_MIN, ps);

	/* 1. The requested stack is usable, the guard is extra.  */
	pthread_attr_init(&a);
	pthread_attr_setstacksize(&a, stack);
	pthread_attr_setguardsize(&a, guard);
	/* ia64 splits the block between the memory stack and the register
	   backing store; half of it is stack.  */
#ifdef __ia64__
	ret = pthread_create(&t, &a, use_stack, (void *) (stack / 2 - SLACK));
#else
	ret = pthread_create(&t, &a, use_stack, (void *) (stack - SLACK));
#endif
	if (ret != 0) {
		printf("pthread_create (stack %zu, guard %zu): %s\n",
		       stack, guard, strerror(ret));
		err = 1;
	} else
		pthread_join(t, &res);

	/* 2. pthread_getattr_np reports the guard and the full stack.  */
	ret = pthread_create(&t, &a, report, &r);
	if (ret == 0) {
		pthread_join(t, &res);
		if (res != NULL) {
			printf("pthread_getattr_np: %s\n", strerror((long) res));
			err = 1;
		} else {
			pthread_attr_getguardsize(&r, &got);
			if (got != EXPECTED_GUARD(guard, ps)) {
				printf("guardsize %zu, want %zu\n", got,
				       (size_t) EXPECTED_GUARD(guard, ps));
				err = 1;
			}
			pthread_attr_getstacksize(&r, &got);
			if (got < stack) {
				printf("stacksize %zu, want at least %zu\n",
				       got, stack);
				err = 1;
			}
			pthread_attr_destroy(&r);
		}
	}
	pthread_attr_destroy(&a);

	/* 3. A guard as large as the default stack still leaves the stack.
	   With 64 KB pages, 32 pages of guard are a 2 MB default stack.  */
	pthread_attr_init(&a);
	pthread_attr_getstacksize(&a, &dflt);
	pthread_attr_setguardsize(&a, dflt);
	ret = pthread_create(&t, &a, use_stack, (void *) SLACK);
	if (ret != 0) {
		printf("pthread_create (default stack %zu, guard %zu): %s\n",
		       dflt, dflt, strerror(ret));
		err = 1;
	} else
		pthread_join(t, &res);
	pthread_attr_destroy(&a);

	return err;
}

#define TEST_FUNCTION do_test()
#include "../test-skeleton.c"
