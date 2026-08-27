/* mktime() across the Gregorian century boundaries.

   A 400-year Gregorian cycle is 146097 days.  mktime() splits the year into
   whole 400-year blocks plus a remainder, so a wrong block length shows up
   only once the first block is complete -- for tm_year that is the year 2300,
   which is why nothing before it can find the error.

   The expected values were produced with glibc and are checkable by hand:
   2100-01-01 minus 2000-01-01 is 3155760000 s, and 3155760000 / 86400 is
   36524 days, the length of a century that ends on a non-leap year.  */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const struct
{
  int year;
  long long want;
} bounds[] =
{
  { 1970,              0LL },
  { 2000,      946684800LL },
  { 2038,     2145916800LL },
  { 2100,     4102444800LL },
  { 2200,     7258118400LL },
  { 2299,    10382256000LL },
  { 2300,    10413792000LL },		/* first complete 400-year block */
  { 2301,    10445328000LL },
  { 2400,    13569465600LL },
  { 2401,    13601088000LL },
  { 2500,    16725225600LL },
  { 2600,    19880899200LL },
  { 2700,    23036572800LL },
  { 2800,    26192246400LL },
  { 3000,    32503680000LL },
};

/* True if the value cannot be held by this target's time_t at all, in which
   case there is nothing to test rather than something to report.  */
static int
out_of_range (long long v)
{
  if (sizeof (time_t) >= 8)
    return 0;
  return v > 2147483647LL || v < -2147483648LL;
}

static int
do_test (void)
{
  unsigned i;
  int result = 0;
  int skipped = 0;

  /* A named zone would need a zoneinfo database, which uClibc-ng does not
     read; a POSIX TZ string is understood everywhere.  */
  setenv ("TZ", "UTC0", 1);
  tzset ();

  for (i = 0; i < sizeof bounds / sizeof *bounds; i++)
    {
      struct tm tm;
      time_t got;

      if (out_of_range (bounds[i].want))
	{
	  ++skipped;
	  continue;
	}

      memset (&tm, 0, sizeof tm);
      tm.tm_year = bounds[i].year - 1900;
      tm.tm_mon = 0;
      tm.tm_mday = 1;
      tm.tm_isdst = 0;

      got = mktime (&tm);
      if ((long long) got != bounds[i].want)
	{
	  printf ("mktime(%d-01-01) = %lld, want %lld (off by %lld days)\n",
		  bounds[i].year, (long long) got, bounds[i].want,
		  ((long long) got - bounds[i].want) / 86400);
	  result = 1;
	  continue;
	}

      /* The other direction, which needs no table: whatever mktime accepted
	 must come back unchanged.  */
      if (gmtime_r (&got, &tm) == NULL)
	{
	  printf ("gmtime_r(%lld) failed for %d-01-01\n",
		  (long long) got, bounds[i].year);
	  result = 1;
	  continue;
	}
      if (tm.tm_year + 1900 != bounds[i].year || tm.tm_mon != 0
	  || tm.tm_mday != 1 || tm.tm_yday != 0)
	{
	  printf ("gmtime_r(%lld) = %d-%02d-%02d yday=%d, want %d-01-01 yday=0\n",
		  (long long) got, tm.tm_year + 1900, tm.tm_mon + 1,
		  tm.tm_mday, tm.tm_yday, bounds[i].year);
	  result = 1;
	}
    }

  if (skipped)
    printf ("%d of %d dates skipped: they do not fit a %d-byte time_t\n",
	    skipped, (int) (sizeof bounds / sizeof *bounds),
	    (int) sizeof (time_t));

  return result;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
