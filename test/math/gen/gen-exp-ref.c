/* Host-side generator for the reference table -- see README.md in this directory
   for what to link against and how the budgets in the tests are derived.  */
/* Emits the sample points with their correctly rounded results (CORE-MATH), and
   counts how often each of the three variants misses them. */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
double fd_exp(double);   /* SMALL */
double exp(double);      /* OPTIMIZED (ARM) */
double cr_exp(double);   /* ACCURATE, and the reference */
static long steps(double a, double b)
{
	union { double d; int64_t i; } ua = {a}, ub = {b};
	if (a == b) return 0;
	long d = ua.i - ub.i;
	return d < 0 ? -d : d;
}
int main(int argc, char **argv)
{
	const int N = 1000;
	int gen = argc > 1;
	long nfd = 0, narm = 0, maxfd = 0, maxarm = 0;
	if (gen) printf("static const struct { double x, want; } exp_ref[] = {\n");
	for (int i = 0; i < N; i++) {
		/* cover -745..708: subnormal results at one end, no overflow at the other */
		double x = -745.0 + 1453.0 * i / (N - 1);
		double want = cr_exp(x);
		if (gen) printf("\t{ %a, %a },\n", x, want);
		long sf = steps(fd_exp(x), want), sa = steps(exp(x), want);
		nfd += (sf != 0); narm += (sa != 0);
		if (sf > maxfd) maxfd = sf;
		if (sa > maxarm) maxarm = sa;
	}
	if (gen) printf("};\n");
	else printf("of %d points, off by at least one step:\n  SMALL (fdlibm)  %ld  (worst %ld)\n  OPTIMIZED (ARM) %ld  (worst %ld)\n  ACCURATE        0  (the reference)\n",
		     N, nfd, maxfd, narm, maxarm);
	return 0;
}
