/* printf and strtod of double, value by value against glibc.

   printf-strtod-ref.h holds bit patterns with glibc's %.17g and %a for
   each (gen/gen-printf-strtod-ref.c).  The inputs are never computed in
   floating point here, so the target's arithmetic cannot spoil them.

   - printf %.17g must give glibc's text: 17 significant digits round to
     exactly one string.
   - printf %a may differ from glibc in form (where a subnormal puts its
     point), so it is decoded with integer arithmetic only and must give
     the same bits.
   - strtod must read both of glibc's strings back to the same bits, and
     the edge strings to what glibc made of them.  */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "printf-strtod-ref.h"

#if defined __UCLIBC__ && !defined __UCLIBC_HAS_HEXADECIMAL_FLOATS__
# define HEX 0				/* neither printf nor strtod know hex */
#else
# define HEX 1
#endif

/* [-]0xH[.HHH]p[+-]D to the bits of the double it names; 0 if the text is
   malformed or names no double exactly.  */
static int hex_to_bits (const char *s, uint64_t *out)
{
  uint64_t sign = 0, m = 0;
  int e = 0, esign = 1, digits = 0, frac = 0, point = 0, msb, exp, sh;

  if (*s == '-')
    {
      sign = 1ull << 63;
      s++;
    }
  if (s[0] != '0' || (s[1] | 0x20) != 'x')
    return 0;
  for (s += 2; ; s++)
    {
      int d;
      if (*s == '.' && !point)
	{
	  point = 1;
	  continue;
	}
      if (*s >= '0' && *s <= '9')
	d = *s - '0';
      else if ((*s | 0x20) >= 'a' && (*s | 0x20) <= 'f')
	d = (*s | 0x20) - 'a' + 10;
      else
	break;
      if (m >> 60)
	return 0;
      m = m << 4 | d;
      digits++;
      frac += point;
    }
  if (!digits || (*s | 0x20) != 'p')
    return 0;
  s++;
  if (*s == '-' || *s == '+')
    esign = *s++ == '-' ? -1 : 1;
  if (*s < '0' || *s > '9')
    return 0;
  while (*s >= '0' && *s <= '9')
    if ((e = e * 10 + (*s++ - '0')) > 100000)
      return 0;
  if (*s)
    return 0;
  e = esign * e - 4 * frac;		/* the value is m * 2^e */

  if (!m)
    {
      *out = sign;
      return 1;
    }
  msb = 63 - __builtin_clzll (m);
  exp = msb + e;
  if (exp > 1023)
    return 0;
  if (exp >= -1022)
    {
      sh = msb - 52;
      if (sh > 0 && (m & ((1ull << sh) - 1)))
	return 0;
      m = sh > 0 ? m >> sh : m << -sh;
      *out = sign | (uint64_t) (exp + 1023) << 52 | (m & 0xfffffffffffffull);
      return 1;
    }
  sh = -1074 - e;			/* subnormal: m' * 2^-1074 */
  if (sh > 0)
    {
      if (sh > 63 || (m & ((1ull << sh) - 1)))
	return 0;
      m >>= sh;
    }
  else
    m <<= -sh;
  if (m >> 52)
    return 0;
  *out = sign | m;
  return 1;
}

static uint64_t bits_of (double d)
{
  uint64_t b;
  memcpy (&b, &d, 8);
  return b;
}

static int report (int *count, const char *what, const char *in,
		   const char *got, const char *want)
{
  if ((*count)++ < 5)
    printf ("%s %s: \"%s\", want \"%s\"\n", what, in, got, want);
  return 1;
}

int main (void)
{
  int n = sizeof t / sizeof *t, i;
  int bad_g = 0, bad_a = 0, bad_sg = 0, bad_sa = 0, bad_edge = 0;
  char buf[64], in[24], got[24], want[24];

  for (i = 0; i < n; i++)
    {
      double v;
      uint64_t b;

      memcpy (&v, &t[i].bits, 8);
      sprintf (in, "%016llx", (unsigned long long) t[i].bits);
      sprintf (want, "%016llx", (unsigned long long) t[i].bits);

      snprintf (buf, sizeof buf, "%.17g", v);
      if (strcmp (buf, t[i].g17))
	report (&bad_g, "printf %.17g", in, buf, t[i].g17);

      b = bits_of (strtod (t[i].g17, NULL));
      if (b != t[i].bits)
	{
	  sprintf (got, "%016llx", (unsigned long long) b);
	  report (&bad_sg, "strtod", t[i].g17, got, want);
	}

      if (!HEX)
	continue;
      snprintf (buf, sizeof buf, "%a", v);
      if (!hex_to_bits (buf, &b) || b != t[i].bits)
	report (&bad_a, "printf %a", in, buf, t[i].a);

      b = bits_of (strtod (t[i].a, NULL));
      if (b != t[i].bits)
	{
	  sprintf (got, "%016llx", (unsigned long long) b);
	  report (&bad_sa, "strtod", t[i].a, got, want);
	}
    }

  for (i = 0; i < (int) (sizeof edge / sizeof *edge); i++)
    {
      uint64_t b;

      if (!HEX && (edge[i].s[1] | 0x20) == 'x')
	continue;
      b = bits_of (strtod (edge[i].s, NULL));
      if (b != edge[i].bits)
	{
	  sprintf (got, "%016llx", (unsigned long long) b);
	  sprintf (want, "%016llx", (unsigned long long) edge[i].bits);
	  report (&bad_edge, "strtod", edge[i].s, got, want);
	}
    }

  printf ("%d values: printf %%.17g %d, printf %%a %d, strtod %%.17g %d, "
	  "strtod %%a %d, strtod edge %d wrong%s\n", n, bad_g, bad_a, bad_sg,
	  bad_sa, bad_edge, HEX ? "" : " (no hex floats)");
  return bad_g || bad_a || bad_sg || bad_sa || bad_edge;
}
