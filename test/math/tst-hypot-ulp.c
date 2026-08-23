/* Same idea as tst-exp-ulp.c: the guest has no reference of its own, so the
   correctly rounded results come from a generated header, and what a variant may
   miss depends on which implementation the library was built with.

   The pairs in hypot-ref.h are deliberately biased towards hard cases -- see the
   comment there -- so these counts say nothing about how often hypot is wrong in
   practice.  They only have to separate the variants from each other.  */

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <features.h>

#include "hypot-ref.h"

/* Budgets, and where they come from.  Measured on x86-64 over the 1275 pairs:
   fdlibm misses 248, glibc's version 6, CORE-MATH none.  The budgets sit just
   above that, on purpose: a target that scores worse is the interesting case and
   should be looked at, not accommodated.  Which input falls on the wrong side of a
   rounding boundary does move with the compiler and with fma contraction, so some
   arch will trip these -- that is the point.  When one does, find out why before
   raising the number, and write down what came out.  */
#if defined __UCLIBC_LIBM_ACCURATE__
# define TIER		"accurate"
# define MAX_WRONG	0
#elif defined __UCLIBC_LIBM_OPTIMIZED__
# define TIER		"optimized"
# define MAX_WRONG	8
#else
# define TIER		"small"
# define MAX_WRONG	260
#endif

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
	const int n = (int)(sizeof hypot_ref / sizeof hypot_ref[0]);
	int wrong = 0, i;
	long worst = 0;
	double wx = 0, wy = 0;

	for (i = 0; i < n; i++) {
		long s = steps(hypot(hypot_ref[i].x, hypot_ref[i].y),
			       hypot_ref[i].want);

		if (s != 0)
			wrong++;
		if (s > worst) {
			worst = s;
			wx = hypot_ref[i].x;
			wy = hypot_ref[i].y;
		}
	}

	printf("libm variant %s: hypot %d of %d pairs off, worst %ld step(s) at (%a,%a)\n",
	       TIER, wrong, n, worst, wx, wy);

	if (worst > MAX_STEPS) {
		printf("FAIL: more than %d step(s) from the correctly rounded value\n",
		       MAX_STEPS);
		return 1;
	}
	if (wrong > MAX_WRONG) {
		printf("FAIL: %d pairs off, %s allows at most %d\n",
		       wrong, TIER, MAX_WRONG);
		return 1;
	}
	return 0;
}
