/* mktime() across the Gregorian century boundaries, and before the Epoch.

   A 400-year Gregorian cycle is 146097 days.  mktime() splits the year into
   whole 400-year blocks plus a remainder, so a wrong block length shows up
   only once the first block is complete -- for tm_year that is the year 2300,
   which is why nothing before it can find the error.

   The dates before 1970 are here for a second reason: they make the dividend
   negative in the day and second arithmetic, which none of the later dates
   do.  A sign handled in the wrong place stays invisible until then, and it
   also stays invisible on whole days, where the remainder is zero -- hence
   both 1969-12-31 00:00:00 and 1969-12-31 23:59:59.

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
  int year, mon, mday, hour, min, sec;
  long long want;
} bounds[] =
{
  /* Before the Epoch: negative seconds since 1970. */
  { 1900,  1,  1,  0,  0,  0,    -2208988800LL },
  { 1938,  4, 24, 22, 13, 20,    -1000000000LL },
  { 1969, 12, 31,  0,  0,  0,         -86400LL },
  { 1969, 12, 31, 23, 59, 59,             -1LL },

  { 1970,  1,  1,  0,  0,  0,              0LL },
  { 2000,  1,  1,  0,  0,  0,      946684800LL },
  { 2038,  1,  1,  0,  0,  0,     2145916800LL },
  { 2100,  1,  1,  0,  0,  0,     4102444800LL },
  { 2200,  1,  1,  0,  0,  0,     7258118400LL },
  { 2299,  1,  1,  0,  0,  0,    10382256000LL },
  { 2300,  1,  1,  0,  0,  0,    10413792000LL },  /* first whole 400y block */
  { 2301,  1,  1,  0,  0,  0,    10445328000LL },
  { 2400,  1,  1,  0,  0,  0,    13569465600LL },
  { 2401,  1,  1,  0,  0,  0,    13601088000LL },
  { 2500,  1,  1,  0,  0,  0,    16725225600LL },
  { 2600,  1,  1,  0,  0,  0,    19880899200LL },
  { 2700,  1,  1,  0,  0,  0,    23036572800LL },
  { 2800,  1,  1,  0,  0,  0,    26192246400LL },
  { 3000,  1,  1,  0,  0,  0,    32503680000LL },
};

/* True if the value cannot be held by this target's time_t at all, in which
   case there is nothing to test rather than something to report.

   The round trip through time_t catches any narrowing, whatever the width:
   a 32-bit time_t drops 1900 and everything past 2038 while keeping 1938 and
   1969.  The unsigned test in front of it is not needed by anything uClibc-ng
   builds today -- every bits/typesizes.h types time_t as a signed long or
   __S64_TYPE -- but neither C nor POSIX requires that, so it is cheap to not
   depend on it.  Both tests fold away at compile time.  */
static int
out_of_range (long long v)
{
  if (v < 0 && (time_t) -1 > 0)
    return 1;
  return (long long) (time_t) v != v;
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
      tm.tm_mon = bounds[i].mon - 1;
      tm.tm_mday = bounds[i].mday;
      tm.tm_hour = bounds[i].hour;
      tm.tm_min = bounds[i].min;
      tm.tm_sec = bounds[i].sec;
      tm.tm_isdst = 0;

      got = mktime (&tm);
      if ((long long) got != bounds[i].want)
	{
	  printf ("mktime(%04d-%02d-%02d %02d:%02d:%02d) = %lld, want %lld"
		  " (off by %lld s)\n",
		  bounds[i].year, bounds[i].mon, bounds[i].mday,
		  bounds[i].hour, bounds[i].min, bounds[i].sec,
		  (long long) got, bounds[i].want,
		  (long long) got - bounds[i].want);
	  result = 1;
	  continue;
	}

      /* The other direction, which needs no table: whatever mktime accepted
	 must come back unchanged.  */
      if (gmtime_r (&got, &tm) == NULL)
	{
	  printf ("gmtime_r(%lld) failed for %04d-%02d-%02d\n",
		  (long long) got, bounds[i].year, bounds[i].mon,
		  bounds[i].mday);
	  result = 1;
	  continue;
	}
      if (tm.tm_year + 1900 != bounds[i].year || tm.tm_mon + 1 != bounds[i].mon
	  || tm.tm_mday != bounds[i].mday || tm.tm_hour != bounds[i].hour
	  || tm.tm_min != bounds[i].min || tm.tm_sec != bounds[i].sec)
	{
	  printf ("gmtime_r(%lld) = %04d-%02d-%02d %02d:%02d:%02d,"
		  " want %04d-%02d-%02d %02d:%02d:%02d\n",
		  (long long) got,
		  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		  tm.tm_hour, tm.tm_min, tm.tm_sec,
		  bounds[i].year, bounds[i].mon, bounds[i].mday,
		  bounds[i].hour, bounds[i].min, bounds[i].sec);
	  result = 1;
	}
    }

  if (skipped)
    printf ("%d of %d dates skipped: they do not fit this target's time_t\n",
	    skipped, (int) (sizeof bounds / sizeof *bounds));

  return result;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
