/* sinh, tanh and their float and long double entry points against the
   correctly rounded result.

   This replaces sinh_test() and tanh_test() in libm-test.inc, which measured
   the same six functions against glibc's ulp tables at ten named points each.
   See ulp-check.h.  */

#include "ulp-check.h"
#include "sinh-ref.h"

/* Measured over the 1000 points of the table: fdlibm misses the correctly
   rounded result 239 times for sinh and 303 for tanh, CORE-MATH none.  Those
   counts are high because both functions are built out of exp and expm1 and
   inherit their error before adding their own -- and because the table is
   biased towards small |x|, where the cancellation is.  Neither has an
   optimized or an accurate variant -- Arm has no portable scalar sinh or tanh
   and glibc ships the same fdlibm code -- so all three settings share one
   number.

   Unlike sin and cos, these two want no help from x87-precision.h: they do no
   compensated reduction, so the extra width of the x87 registers only helps.
   Measured on i686 with the FPU as Linux leaves it, sinh misses 217 and tanh
   231, both below what a target that rounds every operation to double gives.
   The budget is set from the higher, uniform number.  */
#define SINH_BUDGET	BUDGET(250, 250, 250)
#define TANH_BUDGET	BUDGET(315, 315, 315)

#define F_BUDGET	0

/* How far the worst point may be, and this is a requirement rather than an
   observation.  It follows the implementation, not the setting -- and there is
   no variant of this function, so all three settings run the same fdlibm code
   and the accurate column is one and not nought.  fdlibm's own claim is an
   error below one ulp, so the neighbouring representable value is as far as it
   may land.  */
#define STEPS	BUDGET(1, 1, 1)

/* The cases sinh_test() checked, for each of the three entry points. */
#define SINH_SPECIALS(fn, expect, type)					   \
do {									   \
	fails += expect(#fn "(0)", fn((type)0), (type)0);		   \
	fails += expect(#fn "(-0)", fn(-(type)0), -(type)0);		   \
	fails += expect(#fn "(inf)", fn((type)INFINITY), (type)INFINITY);  \
	fails += expect(#fn "(-inf)", fn(-(type)INFINITY), -(type)INFINITY); \
	fails += expect(#fn "(nan)", fn((type)NAN), (type)NAN);		   \
} while (0)

/* And the ones tanh_test() checked. */
#define TANH_SPECIALS(fn, expect, type)					   \
do {									   \
	fails += expect(#fn "(0)", fn((type)0), (type)0);		   \
	fails += expect(#fn "(-0)", fn(-(type)0), -(type)0);		   \
	fails += expect(#fn "(inf)", fn((type)INFINITY), (type)1);	   \
	fails += expect(#fn "(-inf)", fn(-(type)INFINITY), -(type)1);	   \
	fails += expect(#fn "(nan)", fn((type)NAN), (type)NAN);		   \
} while (0)

int main(void)
{
	int fails = 0;

	fails += ulp_sweep1_d("sinh", sinh, sinh_ref, NELEM(sinh_ref),
			      SINH_BUDGET, STEPS);
	fails += ulp_sweep1_f("sinhf", sinhf, sinhf_ref, NELEM(sinhf_ref),
			      F_BUDGET, STEPS);
	fails += ulp_sweep1_l("sinhl", sinhl, sinh_ref, NELEM(sinh_ref),
			      SINH_BUDGET, STEPS);

	fails += ulp_sweep1_d("tanh", tanh, tanh_ref, NELEM(tanh_ref),
			      TANH_BUDGET, STEPS);
	fails += ulp_sweep1_f("tanhf", tanhf, tanhf_ref, NELEM(tanhf_ref),
			      F_BUDGET, STEPS);
	fails += ulp_sweep1_l("tanhl", tanhl, tanh_ref, NELEM(tanh_ref),
			      TANH_BUDGET, STEPS);

	SINH_SPECIALS(sinh, ulp_expect_d, double);
	SINH_SPECIALS(sinhf, ulp_expect_f, float);
	SINH_SPECIALS(sinhl, ulp_expect_l, long double);

	TANH_SPECIALS(tanh, ulp_expect_d, double);
	TANH_SPECIALS(tanhf, ulp_expect_f, float);
	TANH_SPECIALS(tanhl, ulp_expect_l, long double);

	return fails != 0;
}
