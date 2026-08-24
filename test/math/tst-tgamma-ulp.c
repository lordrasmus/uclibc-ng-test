/* tgamma, tgammaf and tgammal against the correctly rounded result.

   This replaces tgamma_test() in libm-test.inc.  See ulp-check.h.

   The three settings are three different implementations here, which is not the
   usual case: the small one computes exp(lgamma(x)) and is not a gamma function
   at all, the optimized one is musl's Lanczos approximation, the accurate one is
   CORE-MATH.  */

#include "ulp-check.h"
#include "tgamma-ref.h"

/* Measured on riscv32 over the 999 points of the table.  Host numbers would be
   wrong for the small variant: it computes exp(lgamma(x)), so its result
   depends on the exp and the log underneath it, and a host measurement reaches
   glibc's rather than ours.

		misses	worst
      small	   747	  1269 steps
      optimized	   718	     6
      accurate	     0	     -

   The interesting part is not the count -- musl's Lanczos misses nearly as
   often as fdlibm -- but the worst case: 6 representable values instead of 1269.
   exp(lgamma(x)) has no error bound to speak of, because exp scales the error of
   its argument by |lgamma(x)|, which is 359 at x = 100 and 600 at x = 156, where
   that 1269 is measured.  A Lanczos approximation stays within about an ulp.  */
#define D_BUDGET	BUDGET(760, 730, 0)
#define STEPS	BUDGET(1, 1, 0)

/* tgammaf computes in double and converts, and every variant hits every one of
   the 501 float points -- even the small one, whose double error disappears in
   the conversion.  */
#define F_BUDGET	0

/* The cases tgamma_test() checked.  Note the poles differ in sign -- tgamma(+0)
   is +inf and tgamma(-0) is -inf -- and that a negative integer is a domain
   error giving NaN, not a pole.  */
#define SPECIALS(fn, expect, type)					   \
do {									   \
	fails += expect(#fn "(0)", fn((type)0), (type)INFINITY);		   \
	fails += expect(#fn "(-0)", fn(-(type)0), -(type)INFINITY);	   \
	fails += expect(#fn "(-2)", fn(-(type)2), (type)NAN);		   \
	fails += expect(#fn "(inf)", fn((type)INFINITY), (type)INFINITY);  \
	fails += expect(#fn "(-inf)", fn(-(type)INFINITY), (type)NAN);	   \
	fails += expect(#fn "(nan)", fn((type)NAN), (type)NAN);		   \
	fails += expect(#fn "(1)", fn((type)1), (type)1);		   \
	fails += expect(#fn "(4)", fn((type)4), (type)6);		   \
} while (0)

int main(void)
{
	int fails = 0;

	fails += ulp_sweep1_d("tgamma", tgamma, tgamma_ref, NELEM(tgamma_ref),
			      D_BUDGET, STEPS);
	fails += ulp_sweep1_f("tgammaf", tgammaf, tgammaf_ref,
			      NELEM(tgammaf_ref), F_BUDGET, STEPS);
	fails += ulp_sweep1_l("tgammal", tgammal, tgamma_ref, NELEM(tgamma_ref),
			      D_BUDGET, STEPS);

	SPECIALS(tgamma, ulp_expect_d, double);
	SPECIALS(tgammaf, ulp_expect_f, float);
	SPECIALS(tgammal, ulp_expect_l, long double);

	return fails != 0;
}
