/* Shared harness for the tst-*-ulp tests.

   Each of those checks one function against correctly rounded results that were
   computed on the host (see gen/), because the guest has no reference of its
   own: uClibc-ng's long double is a wrapper around double on most ports, so
   expl is the very function under test.

   How far a result may stray from that reference depends on which
   implementation the library was configured with -- UCLIBC_LIBM_SMALL,
   _OPTIMIZED or _ACCURATE.  A test therefore states three budgets and lets
   BUDGET() pick the one in force.

   The float and long double entry points are not separate implementations:
   libm/w_*f.c and libm/ldouble_wrappers.c compute in double and convert.  So
   they are checked against the same setting, and against a table of their own
   only where the conversion can round a second time.  */

#ifndef ULP_CHECK_H
#define ULP_CHECK_H

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <features.h>

#if defined __UCLIBC_LIBM_ACCURATE__
# define TIER				"accurate"
# define BUDGET(small, optimized, accurate)	(accurate)
#elif defined __UCLIBC_LIBM_OPTIMIZED__
# define TIER				"optimized"
# define BUDGET(small, optimized, accurate)	(optimized)
#else
# define TIER				"small"
# define BUDGET(small, optimized, accurate)	(small)
#endif

/* How far the worst point may be is stated per function, not once for all of
   them: exp, hypot and expm1 never miss by more than one representable value in
   any variant, but fdlibm's erfc reaches two, and three where the compiler
   contracts a multiply and an add.  A variant that lands further away than its
   own measurement did is broken in a different way than one that merely misses
   more often, which is why the two limits are separate.  */

#define NELEM(a)	((int)(sizeof (a) / sizeof (a)[0]))

/* Generated tables.  Results in them are positive, which is what makes the
   integer subtraction in ulp_steps_* a distance.  */
struct ulp_d1 { double x, want; };
struct ulp_d2 { double x, y, want; };
struct ulp_f1 { float x, want; };
struct ulp_f2 { float x, y, want; };

/* Distance in representable values, not in ulp: an ulp changes size at every
   power of two, this does not, and the budgets are counted in these.  */
static inline long ulp_steps_d(double got, double want)
{
	union { double d; int64_t i; } a, b;
	long d;

	if (got == want)
		return 0;
	a.d = got;
	b.d = want;
	d = (long)(a.i - b.i);
	return d < 0 ? -d : d;
}

static inline long ulp_steps_f(float got, float want)
{
	union { float f; int32_t i; } a, b;
	long d;

	if (got == want)
		return 0;
	a.f = got;
	b.f = want;
	d = (long)(a.i - b.i);
	return d < 0 ? -d : d;
}

/* The long double entry points return the double result widened, so compare
   there: the reference table stays the double one.  */
static inline long ulp_steps_l(long double got, double want)
{
	return ulp_steps_d((double)got, want);
}

static inline int ulp_verdict(const char *name, int wrong, int n, long worst,
			      int max_wrong, int max_steps, double wx,
			      double wy, int arity)
{
	if (arity == 2)
		printf("%s variant, %s: %d of %d off, worst %ld step(s) at (%a,%a)\n",
		       TIER, name, wrong, n, worst, wx, wy);
	else
		printf("%s variant, %s: %d of %d off, worst %ld step(s) at %a\n",
		       TIER, name, wrong, n, worst, wx);

	if (worst > max_steps) {
		printf("FAIL: %s is %ld step(s) from the correctly rounded value, %d allowed\n",
		       name, worst, max_steps);
		return 1;
	}
	if (wrong > max_wrong) {
		printf("FAIL: %s off at %d of %d points, %s allows %d\n",
		       name, wrong, n, TIER, max_wrong);
		return 1;
	}
	return 0;
}

/* One sweep per (type, arity).  Bodies are identical apart from the types, so
   they are written once here rather than once per test.  */
#define ULP_DEFINE_SWEEP1(sfx, type, table, steps)			\
static inline int ulp_sweep1_##sfx(const char *name, type (*fn)(type),	\
				   const struct table *t, int n,	\
				   int max_wrong, int max_steps)	\
{									\
	int i, wrong = 0;						\
	long worst = 0;							\
	double wx = 0;							\
									\
	for (i = 0; i < n; i++) {					\
		long s = steps(fn(t[i].x), t[i].want);			\
									\
		if (s != 0)						\
			wrong++;					\
		if (s > worst) {					\
			worst = s;					\
			wx = t[i].x;					\
		}							\
	}								\
	return ulp_verdict(name, wrong, n, worst, max_wrong, max_steps,	\
			   wx, 0, 1);					\
}

#define ULP_DEFINE_SWEEP2(sfx, type, table, steps)			\
static inline int ulp_sweep2_##sfx(const char *name,			\
				   type (*fn)(type, type),		\
				   const struct table *t, int n,	\
				   int max_wrong, int max_steps)	\
{									\
	int i, wrong = 0;						\
	long worst = 0;							\
	double wx = 0, wy = 0;						\
									\
	for (i = 0; i < n; i++) {					\
		long s = steps(fn(t[i].x, t[i].y), t[i].want);		\
									\
		if (s != 0)						\
			wrong++;					\
		if (s > worst) {					\
			worst = s;					\
			wx = t[i].x;					\
			wy = t[i].y;					\
		}							\
	}								\
	return ulp_verdict(name, wrong, n, worst, max_wrong, max_steps,	\
			   wx, wy, 2);					\
}

ULP_DEFINE_SWEEP1(d, double, ulp_d1, ulp_steps_d)
ULP_DEFINE_SWEEP1(f, float, ulp_f1, ulp_steps_f)
ULP_DEFINE_SWEEP1(l, long double, ulp_d1, ulp_steps_l)
ULP_DEFINE_SWEEP2(d, double, ulp_d2, ulp_steps_d)
ULP_DEFINE_SWEEP2(f, float, ulp_f2, ulp_steps_f)
ULP_DEFINE_SWEEP2(l, long double, ulp_d2, ulp_steps_l)

/* Special cases are exact: no budget, and +0 is not -0.  */
#define ULP_DEFINE_EXPECT(sfx, type)					\
static inline int ulp_expect_##sfx(const char *what, type got, type want) \
{									\
	int ok;								\
									\
	if (isnan(want))						\
		ok = isnan(got);					\
	else if (want == 0)						\
		ok = got == 0 && signbit(got) == signbit(want);		\
	else								\
		ok = got == want;					\
									\
	if (!ok)							\
		printf("FAIL: %s gave %a, expected %a\n", what,		\
		       (double)got, (double)want);			\
	return !ok;							\
}

ULP_DEFINE_EXPECT(d, double)
ULP_DEFINE_EXPECT(f, float)
ULP_DEFINE_EXPECT(l, long double)

#endif /* ULP_CHECK_H */
