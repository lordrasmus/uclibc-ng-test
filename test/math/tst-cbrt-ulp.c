/* cbrt, cosh, log1p and their float and long double entry points against the
   correctly rounded result.

   This replaces cbrt_test(), cosh_test() and log1p_test() in libm-test.inc,
   which measured the same nine functions against glibc's ulp tables at about
   ten named points each.  See ulp-check.h.  */

#include <stdio.h>
#include "ulp-check.h"
#include "cbrt-ref.h"

/* Measured over the points of the tables: fdlibm misses the correctly rounded
   result 65 times for cbrt, 96 for cosh and 17 for log1p, never by more than
   one step.  None has an optimized or an accurate variant yet; CORE-MATH has
   all three.  */
#define CBRT_BUDGET	BUDGET(70, 70, 70)
#define COSH_BUDGET	BUDGET(102, 102, 102)
#define LOG1P_BUDGET	BUDGET(22, 22, 22)
#define F_BUDGET	0

/* How far the worst point may be, and this is a requirement rather than an
   observation.  It follows the implementation, not the setting -- and there is
   no variant of this function, so all three settings run the same fdlibm code
   and the accurate column is one and not nought.  fdlibm's own claim is an
   error below one ulp, so the neighbouring representable value is as far as it
   may land.  */
#define STEPS		BUDGET(1, 1, 1)

int main(void)
{
	int fails = 0;

	fails += ulp_sweep1_d("cbrt", cbrt, cbrt_ref, NELEM(cbrt_ref), CBRT_BUDGET, STEPS);
	fails += ulp_sweep1_f("cbrtf", cbrtf, cbrtf_ref, NELEM(cbrtf_ref), F_BUDGET, STEPS);
	fails += ulp_sweep1_l("cbrtl", cbrtl, cbrt_ref, NELEM(cbrt_ref), CBRT_BUDGET, STEPS);

	fails += ulp_sweep1_d("cosh", cosh, cosh_ref, NELEM(cosh_ref), COSH_BUDGET, STEPS);
	fails += ulp_sweep1_f("coshf", coshf, coshf_ref, NELEM(coshf_ref), F_BUDGET, STEPS);
	fails += ulp_sweep1_l("coshl", coshl, cosh_ref, NELEM(cosh_ref), COSH_BUDGET, STEPS);

	fails += ulp_sweep1_d("log1p", log1p, log1p_ref, NELEM(log1p_ref), LOG1P_BUDGET, STEPS);
	fails += ulp_sweep1_f("log1pf", log1pf, log1pf_ref, NELEM(log1pf_ref), F_BUDGET, STEPS);
	fails += ulp_sweep1_l("log1pl", log1pl, log1p_ref, NELEM(log1p_ref), LOG1P_BUDGET, STEPS);

	/* The cases the three bodies checked.  cbrt keeps the sign of its
	   argument; cosh is even and overflows; log1p is -inf at -1 and a
	   domain error below it.  */
	fails += ulp_expect_d("cbrt(0)", cbrt(0.0), 0.0);
	fails += ulp_expect_d("cbrt(-0)", cbrt(-0.0), -0.0);
	fails += ulp_expect_d("cbrt(inf)", cbrt(INFINITY), INFINITY);
	fails += ulp_expect_d("cbrt(-inf)", cbrt(-INFINITY), -INFINITY);
	fails += ulp_expect_d("cbrt(nan)", cbrt(NAN), NAN);
	fails += ulp_expect_d("cbrt(8)", cbrt(8.0), 2.0);
	fails += ulp_expect_d("cbrt(-27)", cbrt(-27.0), -3.0);

	fails += ulp_expect_d("cosh(0)", cosh(0.0), 1.0);
	fails += ulp_expect_d("cosh(-0)", cosh(-0.0), 1.0);
	fails += ulp_expect_d("cosh(inf)", cosh(INFINITY), INFINITY);
	fails += ulp_expect_d("cosh(-inf)", cosh(-INFINITY), INFINITY);
	fails += ulp_expect_d("cosh(nan)", cosh(NAN), NAN);

	fails += ulp_expect_d("log1p(0)", log1p(0.0), 0.0);
	fails += ulp_expect_d("log1p(-0)", log1p(-0.0), -0.0);
	fails += ulp_expect_d("log1p(-1)", log1p(-1.0), -INFINITY);
	fails += ulp_expect_d("log1p(-2)", log1p(-2.0), NAN);
	fails += ulp_expect_d("log1p(inf)", log1p(INFINITY), INFINITY);
	fails += ulp_expect_d("log1p(nan)", log1p(NAN), NAN);

	return fails != 0;
}
