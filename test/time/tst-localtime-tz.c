/* localtime() with a zone offset, on both sides of the 2038 boundary.

   Nothing else here notices when the offset is applied wrongly, because the
   test guest runs in UTC and an offset of zero survives any sign error.
   These cases use fixed-offset POSIX TZ strings -- uClibc-ng reads no
   zoneinfo database, so a named zone would answer UTC everywhere.

   Values produced with glibc; each is the UTC column plus the offset named in
   the TZ string, which is checkable by eye (JST-9 is UTC+9, EST5 is UTC-5).  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const struct
{
  const char *tz;
  long long t;
  int year, mon, mday, hour, min, sec;
} cases[] =
{
  { "UTC0",      1000000000LL, 2001,  9,  9,  1, 46, 40 },
  { "UTC0",      1906545600LL, 2030,  6,  1, 12,  0,  0 },
  { "UTC0",      2145916800LL, 2038,  1,  1,  0,  0,  0 },
  { "UTC0",      2222164800LL, 2040,  6,  1, 12,  0,  0 },
  { "UTC0",      4102444800LL, 2100,  1,  1,  0,  0,  0 },
  { "JST-9",     1000000000LL, 2001,  9,  9, 10, 46, 40 },
  { "JST-9",     1906545600LL, 2030,  6,  1, 21,  0,  0 },
  { "JST-9",     2145916800LL, 2038,  1,  1,  9,  0,  0 },
  { "JST-9",     2222164800LL, 2040,  6,  1, 21,  0,  0 },
  { "JST-9",     4102444800LL, 2100,  1,  1,  9,  0,  0 },
  { "EST5",      1000000000LL, 2001,  9,  8, 20, 46, 40 },
  { "EST5",      1906545600LL, 2030,  6,  1,  7,  0,  0 },
  { "EST5",      2145916800LL, 2037, 12, 31, 19,  0,  0 },
  { "EST5",      2222164800LL, 2040,  6,  1,  7,  0,  0 },
  { "EST5",      4102444800LL, 2099, 12, 31, 19,  0,  0 },
  { "IST-5:30",  1000000000LL, 2001,  9,  9,  7, 16, 40 },
  { "IST-5:30",  1906545600LL, 2030,  6,  1, 17, 30,  0 },
  { "IST-5:30",  2145916800LL, 2038,  1,  1,  5, 30,  0 },
  { "IST-5:30",  2222164800LL, 2040,  6,  1, 17, 30,  0 },
  { "IST-5:30",  4102444800LL, 2100,  1,  1,  5, 30,  0 },
  { "NZST-12",   1000000000LL, 2001,  9,  9, 13, 46, 40 },
  { "NZST-12",   1906545600LL, 2030,  6,  2,  0,  0,  0 },
  { "NZST-12",   2145916800LL, 2038,  1,  1, 12,  0,  0 },
  { "NZST-12",   2222164800LL, 2040,  6,  2,  0,  0,  0 },
  { "NZST-12",   4102444800LL, 2100,  1,  1, 12,  0,  0 },
};

static int
do_test (void)
{
  unsigned i;
  int result = 0;
  int skipped = 0;

  for (i = 0; i < sizeof cases / sizeof *cases; i++)
    {
      struct tm tm;
      time_t t;

      if (sizeof (time_t) < 8 && cases[i].t > 2147483647LL)
	{
	  ++skipped;
	  continue;
	}

      setenv ("TZ", cases[i].tz, 1);
      tzset ();

      t = (time_t) cases[i].t;
      memset (&tm, 0, sizeof tm);
      if (localtime_r (&t, &tm) == NULL)
	{
	  printf ("TZ=%s localtime_r(%lld) failed\n", cases[i].tz, cases[i].t);
	  result = 1;
	  continue;
	}

      if (tm.tm_year + 1900 != cases[i].year || tm.tm_mon + 1 != cases[i].mon
	  || tm.tm_mday != cases[i].mday || tm.tm_hour != cases[i].hour
	  || tm.tm_min != cases[i].min || tm.tm_sec != cases[i].sec)
	{
	  printf ("TZ=%-9s t=%-11lld got %04d-%02d-%02d %02d:%02d:%02d,"
		  " want %04d-%02d-%02d %02d:%02d:%02d\n",
		  cases[i].tz, cases[i].t,
		  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		  tm.tm_hour, tm.tm_min, tm.tm_sec,
		  cases[i].year, cases[i].mon, cases[i].mday,
		  cases[i].hour, cases[i].min, cases[i].sec);
	  result = 1;
	}
    }

  if (skipped)
    printf ("%d of %d cases skipped: they need a time_t wider than %d bytes\n",
	    skipped, (int) (sizeof cases / sizeof *cases),
	    (int) sizeof (time_t));

  return result;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
