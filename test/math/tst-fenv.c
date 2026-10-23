/* The floating-point environment: every function, for whatever the target
   defines.  A port without an FPU has only FE_TONEAREST and no exception
   macros; then the loops below are empty and what remains is the contract
   for that minimal environment.  */

#include <features.h>
#include <stdio.h>

#ifndef __UCLIBC_HAS_FENV__
int main (void)
{
	puts ("no fenv in this configuration");
	return 23;
}
#else

#include <fenv.h>

static int errors;

#define CHECK(cond)							\
	do {								\
		if (!(cond)) {						\
			printf ("FAIL line %d: %s\n", __LINE__, #cond);	\
			errors++;					\
		}							\
	} while (0)

static const int excepts[] = {
#ifdef FE_INVALID
	FE_INVALID,
#endif
#ifdef FE_DIVBYZERO
	FE_DIVBYZERO,
#endif
#ifdef FE_OVERFLOW
	FE_OVERFLOW,
#endif
#ifdef FE_UNDERFLOW
	FE_UNDERFLOW,
#endif
#ifdef FE_INEXACT
	FE_INEXACT,
#endif
	0
};

static const int modes[] = {
	FE_TONEAREST,
#ifdef FE_TOWARDZERO
	FE_TOWARDZERO,
#endif
#ifdef FE_DOWNWARD
	FE_DOWNWARD,
#endif
#ifdef FE_UPWARD
	FE_UPWARD,
#endif
};

#define NMODES (sizeof (modes) / sizeof (modes[0]))

static void test_flags (void)
{
	const int *e;

	CHECK (feclearexcept (FE_ALL_EXCEPT) == 0);
	CHECK (fetestexcept (FE_ALL_EXCEPT) == 0);
	CHECK (feraiseexcept (0) == 0);

	for (e = excepts; *e; e++) {
		fexcept_t saved;

		CHECK (feraiseexcept (*e) == 0);
		/* Overflow and underflow may bring inexact along.  */
		CHECK (fetestexcept (*e) == *e);
		CHECK (fegetexceptflag (&saved, *e) == 0);
		CHECK (feclearexcept (*e) == 0);
		CHECK (fetestexcept (*e) == 0);
		CHECK (fesetexceptflag (&saved, *e) == 0);
		CHECK (fetestexcept (*e) == *e);
		CHECK (feclearexcept (FE_ALL_EXCEPT) == 0);
		CHECK (fesetexcept (*e) == 0);
		CHECK (fetestexcept (FE_ALL_EXCEPT) == *e);
		CHECK (feclearexcept (FE_ALL_EXCEPT) == 0);
	}
}

#ifdef __UCLIBC_HAS_FPU__
/* The flags have to come from the arithmetic itself, not only from
   feraiseexcept, or the port reads some register the FPU never writes.  */
static void test_arith_flags (void)
{
	volatile double zero = 0.0, one = 1.0, three = 3.0, big = 1e308;
	volatile double r;

	(void) zero; (void) one; (void) three; (void) big;
	feclearexcept (FE_ALL_EXCEPT);
# ifdef FE_DIVBYZERO
	r = one / zero;
	CHECK (fetestexcept (FE_DIVBYZERO));
	feclearexcept (FE_ALL_EXCEPT);
# endif
# ifdef FE_INVALID
	r = zero / zero;
	CHECK (fetestexcept (FE_INVALID));
	feclearexcept (FE_ALL_EXCEPT);
# endif
# ifdef FE_OVERFLOW
	r = big * big;
	CHECK (fetestexcept (FE_OVERFLOW));
	feclearexcept (FE_ALL_EXCEPT);
# endif
# ifdef FE_INEXACT
	r = one / three;
	CHECK (fetestexcept (FE_INEXACT));
	feclearexcept (FE_ALL_EXCEPT);
# endif
	(void) r;
}
#endif

static void test_rounding (void)
{
	unsigned int i;

	CHECK (fegetround () == FE_TONEAREST);
	for (i = 0; i < NMODES; i++) {
		CHECK (fesetround (modes[i]) == 0);
		CHECK (fegetround () == modes[i]);
	}
#if defined __UCLIBC_HAS_FPU__ && defined FE_UPWARD && defined FE_DOWNWARD
	{
		volatile double one = 1.0, three = 3.0, up, down;

		fesetround (FE_UPWARD);
		up = one / three;
		fesetround (FE_DOWNWARD);
		down = one / three;
		CHECK (up > down);
	}
#endif
	CHECK (fesetround (FE_TONEAREST) == 0);
}

static void test_env (void)
{
	fenv_t env;
	unsigned int i;

	for (i = 0; i < NMODES; i++) {
		CHECK (fesetround (modes[i]) == 0);
		CHECK (fegetenv (&env) == 0);
		CHECK (fesetround (FE_TONEAREST) == 0);
		CHECK (fesetenv (&env) == 0);
		CHECK (fegetround () == modes[i]);
	}

	CHECK (fesetenv (FE_DFL_ENV) == 0);
	CHECK (fegetround () == FE_TONEAREST);
	CHECK (fetestexcept (FE_ALL_EXCEPT) == 0);

	if (excepts[0]) {
		int e = excepts[0];

		feraiseexcept (e);
		CHECK (feholdexcept (&env) == 0);
		CHECK (fetestexcept (FE_ALL_EXCEPT) == 0);
		CHECK (feupdateenv (&env) == 0);
		CHECK (fetestexcept (e) == e);

		CHECK (fegetenv (&env) == 0);
		feclearexcept (FE_ALL_EXCEPT);
		CHECK (fesetenv (&env) == 0);
		CHECK (fetestexcept (e) == e);
		feclearexcept (FE_ALL_EXCEPT);
	} else {
		CHECK (feholdexcept (&env) == 0);
		CHECK (feupdateenv (&env) == 0);
	}
	CHECK (fesetenv (FE_DFL_ENV) == 0);
}

static void test_mode (void)
{
	femode_t mode;
	unsigned int i;

	for (i = 0; i < NMODES; i++) {
		CHECK (fesetround (modes[i]) == 0);
		CHECK (fegetmode (&mode) == 0);
		CHECK (fesetround (FE_TONEAREST) == 0);
		CHECK (fesetmode (&mode) == 0);
		CHECK (fegetround () == modes[i]);
	}
	CHECK (fesetmode (FE_DFL_MODE) == 0);
	CHECK (fegetround () == FE_TONEAREST);
}

int main (void)
{
	test_flags ();
#ifdef __UCLIBC_HAS_FPU__
	test_arith_flags ();
#endif
	test_rounding ();
	test_env ();
	test_mode ();

	printf ("%d exception(s), %u rounding mode(s), %d error(s)\n",
		(int) (sizeof (excepts) / sizeof (excepts[0])) - 1,
		(unsigned int) NMODES, errors);
	return errors != 0;
}
#endif
