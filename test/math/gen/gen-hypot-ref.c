/* Host-side generator for the reference table -- see README.md in this directory
   for what to link against and how the budgets in the tests are derived.  */
#include <stdio.h>
#include <stdint.h>
#include <math.h>
double fd_hypot(double,double); double gl_hypot(double,double); double cr_hypot(double,double);
static long steps(double a,double b){ union{double d;int64_t i;}u,v; long d;
	if(a==b) return 0; u.d=a; v.d=b; d=(long)(u.i-v.i); return d<0?-d:d; }
int main(int argc, char **argv)
{
	int emit = argc>1;
	long nfd=0,ngl=0,mfd=0,mgl=0,tab=0;
	if(emit) printf("static const struct { double x, y, want; } hypot_ref[] = {\n");
	/* the same sweep the measurements used; every 4th pair that only fdlibm
	   misses and every 40th clean pair go into the table */
	long seen_bad=0, seen_ok=0;
	for (int i=0;i<700;i++){
		double x = ldexp(1.0 + (i%97)/97.0, (i%300)-150);
		for (int j=0;j<60;j++){
			double y = ldexp(1.0 + (j%13)/13.0, (j%120)-60);
			double want = cr_hypot(x,y);
			if(!isfinite(want)||want==0) continue;
			long sf=steps(fd_hypot(x,y),want), sg=steps(gl_hypot(x,y),want);
			/* Pairs where glibc misses are rare, so take all of them:
			   without those the test cannot tell OPTIMIZED from
			   ACCURATE at all. */
			int take;
			if (sg != 0) take = 1;
			else if (sf != 0) take = (seen_bad++ % 4)==0;
			else take = (seen_ok++ % 40)==0;
			if(!take) continue;
			if(emit) printf("\t{ %a, %a, %a },\n", x,y,want);
			tab++; nfd+=(sf!=0); ngl+=(sg!=0);
			if(sf>mfd)mfd=sf; if(sg>mgl)mgl=sg;
		}
	}
	if(emit) printf("};\n");
	else printf("table: %ld pairs\n  SMALL (fdlibm)    %ld off (worst %ld)\n  OPTIMIZED (glibc) %ld off (worst %ld)\n  ACCURATE          0 (the reference)\n", tab,nfd,mfd,ngl,mgl);
	return 0;
}
