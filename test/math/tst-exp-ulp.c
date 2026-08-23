/* How far exp() may stray from the correctly rounded result depends on which
   implementation the library was built with -- UCLIBC_LIBM_SMALL, _OPTIMIZED
   or _ACCURATE.  The budgets below are what those variants actually did when
   this test was written; they carry margin, because which input lands on the
   wrong side of a rounding boundary shifts with the compiler and with fma
   contraction.  The point is not to pin a number, it is to notice a variant
   that no longer behaves like itself.  */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <features.h>

#include "exp-ref.h"

/* Measured on x86-64 over the 1000 points: fdlibm misses 101, Arm's routines 2,
   CORE-MATH none.  The budgets sit just above that -- see tst-hypot-ulp.c for why
   they are kept tight rather than roomy.  */
#if defined __UCLIBC_LIBM_ACCURATE__
# define TIER		"accurate"
# define MAX_WRONG	0
#elif defined __UCLIBC_LIBM_OPTIMIZED__
# define TIER		"optimized"
# define MAX_WRONG	4
#else
# define TIER		"small"
# define MAX_WRONG	110
#endif

/* Never more than one representable step away, whichever variant it is. */
#define MAX_STEPS	1

static long steps(double a, double b)
{
	union { double d; int64_t i; } ua, ub;
	long d;

	if (a == b)
		return 0;
	ua.d = a;
	ub.d = b;
	d = (long)(ua.i - ub.i);
	return d < 0 ? -d : d;
}

int main(void)
{
	const int n = (int)(sizeof exp_ref / sizeof exp_ref[0]);
	int wrong = 0, i;
	long worst = 0;
	double worst_x = 0;

	for (i = 0; i < n; i++) {
		long s = steps(exp(exp_ref[i].x), exp_ref[i].want);

		if (s != 0)
			wrong++;
		if (s > worst) {
			worst = s;
			worst_x = exp_ref[i].x;
		}
	}

	printf("libm variant %s: %d of %d points off, worst %ld step(s) at x=%a\n",
	       TIER, wrong, n, worst, worst_x);

	if (worst > MAX_STEPS) {
		printf("FAIL: more than %d step(s) away from the correctly rounded value\n",
		       MAX_STEPS);
		return 1;
	}
	if (wrong > MAX_WRONG) {
		printf("FAIL: %d points off, %s allows at most %d\n",
		       wrong, TIER, MAX_WRONG);
		return 1;
	}
	return 0;
}
