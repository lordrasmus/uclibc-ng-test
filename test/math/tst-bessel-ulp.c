/* j0, j1, y0, y1, jn and yn against the correctly rounded result.
 *
 * This replaces j0_test() and its five siblings in libm-test.inc, whose calls
 * had been behind #if 0 -- so none of the six had been tested at all.  See
 * ulp-check.h.
 *
 * Only the double entry points: w_j0f.c and w_j0l.c hide theirs behind
 * #ifndef __DO_XSI_MATH__, so jnf and jnl do not exist when XSI math is on,
 * which is also the only setting in which the Bessel functions are built.  That
 * guard looks like it is the wrong way round -- w_j0.c has none -- but this
 * test is not the place to change it.
 */

#include <stdio.h>
#include <math.h>
#include "ulp-check.h"

#ifdef __DO_XSI_MATH__

#include "bessel-ref.h"

/* The size the functions work at: they decay like sqrt(2/(pi x)), and below 1
   they are of order one.  Measuring the error against this rather than against
   the result is what makes a point near a zero mean anything -- see
   ulp_scaled_steps_d.  */
static double envelope(double x)
{
	x = fabs(x);
	return x < 1.0 ? 1.0 : sqrt(2.0 / (M_PI * x));
}

/* Measured over the points of the tables: fdlibm misses the correctly rounded
   result at about 40% of them, never by more than three steps of the local
   scale for j0, j1, y0 and y1.  There is no optimized and no accurate variant
   -- CORE-MATH has no Bessel functions and nobody else has anything but this
   same fdlibm code -- so all three settings share one number.  */
#define J0_BUDGET	BUDGET(400, 400, 400)
#define J1_BUDGET	BUDGET(480, 480, 480)
#define Y0_BUDGET	BUDGET(440, 440, 440)
#define Y1_BUDGET	BUDGET(465, 465, 465)
#define JN_BUDGET	BUDGET(260, 260, 260)
#define YN_BUDGET	BUDGET(300, 300, 300)

/* How far the worst point may be, and this is a requirement rather than an
   observation.  It follows the implementation, not the setting -- and there is
   no variant of this function, so all three settings run the same fdlibm code
   and the accurate column is one and not nought.  fdlibm's own claim is an
   error below one ulp, so the neighbouring representable value is as far as it
   may land.  */
#define BASE_STEPS	BUDGET(1, 1, 1)
#define N_STEPS		BUDGET(1, 1, 1)

/* jn does not meet that, and is expected to fail here.  It computes the ratio
   Jn/J0 by backward recurrence and then normalises with j0:
 *
 *	b = (t * __ieee754_j0(x) / b);		libm/e_jn.c
 *
 * Near a zero of j0 that factor has full absolute but no relative accuracy, and
 * jn inherits it.  At x = 2.4048255576957729, the first zero of j0 as a double,
 * jn(3, x) and jn(5, x) return -inf where the answers are 0.199 and 0.0164, and
 * jn(4, x) is 37 percent low; a third of the points of jn_ref sit on and beside
 * that zero and the seven after it, so the test finds them.  Measured on i686,
 * where the x87 keeps j0 more accurate in absolute terms and so softens it, the
 * worst point is 25192 steps out; on a target that rounds every operation to
 * double it is 5.8e14.  A fix has to normalise with something that is not small
 * there.  */

int main(void)
{
	int fails = 0;

	fails += ulp_sweep_scaled_d("j0", j0, envelope, j0_ref,
				    NELEM(j0_ref), J0_BUDGET, BASE_STEPS);
	fails += ulp_sweep_scaled_d("j1", j1, envelope, j1_ref,
				    NELEM(j1_ref), J1_BUDGET, BASE_STEPS);
	fails += ulp_sweep_scaled_d("y0", y0, envelope, y0_ref,
				    NELEM(y0_ref), Y0_BUDGET, BASE_STEPS);
	fails += ulp_sweep_scaled_d("y1", y1, envelope, y1_ref,
				    NELEM(y1_ref), Y1_BUDGET, BASE_STEPS);
	fails += ulp_sweep_scaled_n("jn", jn, envelope, jn_ref,
				    NELEM(jn_ref), JN_BUDGET, N_STEPS);
	fails += ulp_sweep_scaled_n("yn", yn, envelope, yn_ref,
				    NELEM(yn_ref), YN_BUDGET, N_STEPS);

	/* The cases the six bodies in libm-test.inc checked.  j0 and j1 are
	   even and odd, defined for every real; y0 and y1 are -inf at zero and
	   a domain error to the left of it.  */
	fails += ulp_expect_d("j0(nan)", j0(NAN), NAN);
	fails += ulp_expect_d("j0(inf)", j0(INFINITY), 0.0);
	fails += ulp_expect_d("j0(0)", j0(0.0), 1.0);
	fails += ulp_expect_d("j1(nan)", j1(NAN), NAN);
	fails += ulp_expect_d("j1(inf)", j1(INFINITY), 0.0);
	fails += ulp_expect_d("j1(0)", j1(0.0), 0.0);
	fails += ulp_expect_d("j1(-0)", j1(-0.0), -0.0);

	fails += ulp_expect_d("y0(nan)", y0(NAN), NAN);
	fails += ulp_expect_d("y0(inf)", y0(INFINITY), 0.0);
	fails += ulp_expect_d("y0(0)", y0(0.0), -INFINITY);
	/* NaN, not -inf: the SVID answer of -HUGE is only reached through
	   __kernel_standard, and w_j0.c calls it only when _LIB_VERSION is not
	   _IEEE_, which is what s_lib_version.c sets by default.  So the value
	   comes from e_j0.c, where a negative argument returns zero/zero.  */
	fails += ulp_expect_d("y0(-1)", y0(-1.0), NAN);
	fails += ulp_expect_d("y1(nan)", y1(NAN), NAN);
	fails += ulp_expect_d("y1(inf)", y1(INFINITY), 0.0);
	fails += ulp_expect_d("y1(0)", y1(0.0), -INFINITY);
	fails += ulp_expect_d("y1(-1)", y1(-1.0), NAN);

	fails += ulp_expect_d("jn(0,nan)", jn(0, NAN), NAN);
	fails += ulp_expect_d("jn(0,inf)", jn(0, INFINITY), 0.0);
	fails += ulp_expect_d("jn(0,0)", jn(0, 0.0), 1.0);
	fails += ulp_expect_d("jn(1,0)", jn(1, 0.0), 0.0);
	fails += ulp_expect_d("yn(0,nan)", yn(0, NAN), NAN);
	fails += ulp_expect_d("yn(0,inf)", yn(0, INFINITY), 0.0);
	fails += ulp_expect_d("yn(0,0)", yn(0, 0.0), -INFINITY);
	fails += ulp_expect_d("yn(0,-1)", yn(0, -1.0), NAN);

	return fails != 0;
}

#else  /* !__DO_XSI_MATH__ */

int main(void)
{
	printf("SKIP: the Bessel functions are not built without XSI math\n");
	return 0;
}

#endif
