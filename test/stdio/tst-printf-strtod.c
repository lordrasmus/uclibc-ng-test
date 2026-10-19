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
     the edge strings to what glibc made of them.
   - strtof and strtold also check the libc-test rounding boundaries below,
     independently of printf and of conversion through double.  */
#include <float.h>
#include <math.h>
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

/* The following strtof/strtold cases are adapted from libc-test's
   src/functional/strtof.c and src/functional/strtold.c (7b95dfa).
   The 64-bit-mantissa subnormal inputs are halved for the m68k format,
   whose minimum exponent is one lower than the x86 format's.  Select by
   float.h, which describes the target ABI including compiler options.

   Copyright © 2005-2013 libc-test AUTHORS

   Permission is hereby granted, free of charge, to any person obtaining
   a copy of this software and associated documentation files (the
   "Software"), to deal in the Software without restriction, including
   without limitation the rights to use, copy, modify, merge, publish,
   distribute, sublicense, and/or sell copies of the Software, and to
   permit persons to whom the Software is furnished to do so, subject to
   the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.  */

#if FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MIN_EXP == -125 && FLT_MAX_EXP == 128
static const struct {
  const char *s;
  float want;
} float_edges[] = {
  /* Half of the smallest subnormal, just below and above the tie. */
  { ".7006492321624085354618647916449580656401309709382578858785341419448955413429303e-45", 0 },
  { ".7006492321624085354618647916449580656401309709382578858785341419448955413429304e-45", 0x1p-149f },
  /* One and a half times the smallest subnormal. */
  { ".2101947696487225606385594374934874196920392912814773657635602425834686624028790e-44", 0x1p-149f },
  { ".2101947696487225606385594374934874196920392912814773657635602425834686624028791e-44", 0x1p-148f },
  /* Smallest normal plus half an ulp. */
  { ".1175494420887210724209590083408724842314472120785184615334540294131831453944281e-37", 0x1p-126f },
  { ".1175494420887210724209590083408724842314472120785184615334540294131831453944282e-37", 0x1.000002p-126f },
  /* Overflow boundary. */
  { "340282356779733661637539395458142568447.9999999999999999999", 0x1.fffffep127f },
  { "340282356779733661637539395458142568448", INFINITY },
};
#endif

static const struct {
  const char *s;
  long double want;
} ldouble_edges[] = {
  { "0", 0.0L },
  { "12.345", 12.345L },
  { "1.2345e1", 12.345L },
  { "1e+1000000", INFINITY },
  { "1e-1000000", 0.0L },
#if FLT_RADIX == 2 && LDBL_MANT_DIG == 53 && LDBL_MIN_EXP == -1021 && LDBL_MAX_EXP == 1024
# define HAVE_LDOUBLE_BOUNDARIES 1
  { ".2470328229206232720882843964341106861825299013071623822127928412503377536351043e-323", 0 },
  { ".2470328229206232720882843964341106861825299013071623822127928412503377536351044e-323", 0x1p-1074L },
  { ".7410984687618698162648531893023320585475897039214871466383785237510132609053131e-323", 0x1p-1074L },
  { ".7410984687618698162648531893023320585475897039214871466383785237510132609053132e-323", 0x1p-1073L },
  { ".2225073858507201630123055637955676152503612414573018013083228724049586647606759e-307", 0x1p-1022L },
  { ".2225073858507201630123055637955676152503612414573018013083228724049586647606760e-307", 0x1.0000000000001p-1022L },
  { "17976931348623158079372897140530341507993413271003782693617377898044"
    "49682927647509466490179775872070963302864166928879109465555478519404"
    "02630657488671505820681908902000708383676273854845817711531764475730"
    "27006985557136695962284291481986083493647529271907416844436551070434"
    "2711559699508093042880177904174497791.999999999999999999999999999999", 0x1.fffffffffffffp1023L },
  { "17976931348623158079372897140530341507993413271003782693617377898044"
    "49682927647509466490179775872070963302864166928879109465555478519404"
    "02630657488671505820681908902000708383676273854845817711531764475730"
    "27006985557136695962284291481986083493647529271907416844436551070434"
    "2711559699508093042880177904174497792", INFINITY },
# if HEX
  { "0x1.111111111111281", 0x1.1111111111113p0L },
  { "0x1.11111111111111", 0x1.1111111111111p0L },
# endif
#elif FLT_RADIX == 2 && LDBL_MANT_DIG == 64 && LDBL_MAX_EXP == 16384 \
      && (LDBL_MIN_EXP == -16381 || LDBL_MIN_EXP == -16382)
# define HAVE_LDOUBLE_BOUNDARIES 1
# if LDBL_MIN_EXP == -16381
  /* x86: smallest subnormal is 2^-16445. */
  { ".1822599765941237301264202966809709908199525407846781671860490243514185844316698e-4950", 0 },
  { ".1822599765941237301264202966809709908199525407846781671860490243514185844316699e-4950", 0x1p-16445L },
  { ".5467799297823711903792608900429129724598576223540345015581470730542557532950096e-4950", 0x1p-16445L },
  { ".5467799297823711903792608900429129724598576223540345015581470730542557532950097e-4950", 0x1p-16444L },
# else
  /* m68k: same precision, but smallest subnormal is 2^-16446. */
  { ".0911299882970618650632101483404854954099762703923390835930245121757092922158349e-4950", 0 },
  { ".09112998829706186506321014834048549540997627039233908359302451217570929221583495e-4950", 0x1p-16446L },
  { ".2733899648911855951896304450214564862299288111770172507790735365271278766475048e-4950", 0x1p-16446L },
  { ".27338996489118559518963044502145648622992881117701725077907353652712787664750485e-4950", 0x1p-16445L },
# endif
  { ".3362103143112093506444937793915876332724499641527442230928779770593420866576777e-4931", 0x1p-16382L },
  { ".3362103143112093506444937793915876332724499641527442230928779770593420866576778e-4931", 0x1.0000000000000002p-16382L },
  { "118973149535723176505351158982948.86679662540046955672e4900", 0x1.fffffffffffffffep16383L },
  { "118973149535723176505351158982948.86679662540046955673e4900", INFINITY },
# if HEX
  { "0x8.11111111111111281", 0x8.111111111111113p0L },
  { "0x8.1111111111111111", 0x8.111111111111111p0L },
# endif
#elif FLT_RADIX == 2 && LDBL_MANT_DIG == 113 && LDBL_MIN_EXP == -16381 && LDBL_MAX_EXP == 16384
# define HAVE_LDOUBLE_BOUNDARIES 1
# if HEX
  { "0x1.111111111111111111111111111281", 0x1.1111111111111111111111111113p0L },
  { "0x1.11111111111111111111111111111", 0x1.1111111111111111111111111111p0L },
# endif
#endif
};

static int check_strtof (void)
{
  int bad = 0;
#if FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MIN_EXP == -125 && FLT_MAX_EXP == 128
  size_t i;

  for (i = 0; i < sizeof float_edges / sizeof *float_edges; i++)
    {
      char *end;
      float x = strtof (float_edges[i].s, &end);

      if (x != float_edges[i].want
          || end != float_edges[i].s + strlen (float_edges[i].s))
        if (bad++ < 5)
          printf ("strtof(%s): got %.9g, expected %.9g (endptr %s)\n",
                  float_edges[i].s, (double) x, (double) float_edges[i].want,
                  end == float_edges[i].s + strlen (float_edges[i].s) ? "ok" : "wrong");
    }
  printf ("strtof: %zu boundary values, %d wrong\n", i, bad);
#else
  puts ("strtof boundary values skipped: unsupported float format");
#endif
  return bad;
}

static int check_strtold (void)
{
  int bad = 0;
  size_t i;

  for (i = 0; i < sizeof ldouble_edges / sizeof *ldouble_edges; i++)
    {
      char *end;
      long double x = strtold (ldouble_edges[i].s, &end);

      if (x != ldouble_edges[i].want
          || end != ldouble_edges[i].s + strlen (ldouble_edges[i].s))
        if (bad++ < 5)
          printf ("strtold(%s): got %.36Lg, expected %.36Lg (endptr %s)\n",
                  ldouble_edges[i].s, x, ldouble_edges[i].want,
                  end == ldouble_edges[i].s + strlen (ldouble_edges[i].s) ? "ok" : "wrong");
    }
  printf ("strtold: %zu values, %d wrong (LDBL_MANT_DIG=%d, LDBL_MIN_EXP=%d)\n",
          i, bad, LDBL_MANT_DIG, LDBL_MIN_EXP);
#ifndef HAVE_LDOUBLE_BOUNDARIES
  puts ("strtold precision-specific values skipped: unsupported long double format");
#endif
  return bad;
}

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
  int bad_f, bad_ld;
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
  bad_f = check_strtof ();
  bad_ld = check_strtold ();
  return bad_g || bad_a || bad_sg || bad_sa || bad_edge || bad_f || bad_ld;
}
