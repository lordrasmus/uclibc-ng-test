/* Test program for rpmatch function.
   Copyright (C) 1998, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Jochen Hein <jochen.hein@delphi.central.de>.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>

/* The cases of glibc's tst-rpmatch.sh, which calls this program once per
   line; here the program walks them itself.  */
static const struct
{
  const char *locale, *string;
  int result;
} cases[] =
{
  { "C", "Yes", 1 }, { "C", "yes", 1 }, { "C", "YES", 1 },
  { "C", "YeS", 1 }, { "C", "YEs", 1 }, { "C", "yEs", 1 },
  { "C", "yES", 1 }, { "C", "yeS", 1 }, { "C", "No", 0 },
  { "C", "no", 0 }, { "C", "nonsens", 0 }, { "C", "Error", -1 },
  { "de_DE.ISO-8859-1", "Yes", 1 }, { "de_DE.ISO-8859-1", "Ja", 1 },
  { "de_DE.ISO-8859-1", "Jammerschade", 1 },
  { "de_DE.ISO-8859-1", "dejavu", -1 },
  { "de_DE.ISO-8859-1", "Nein", 0 },
  { "de_DE.ISO-8859-1", "Fehler", -1 },
  { "de_DE.ISO-8859-1", "jein", 1 },
};

int
main (void)
{
  int result = EXIT_SUCCESS;
  size_t i;

  for (i = 0; i < sizeof cases / sizeof cases[0]; i++)
    {
      int r;

      if (setlocale (LC_ALL, cases[i].locale) == NULL)
	{
	  printf ("SKIP: locale %s not available\n", cases[i].locale);
	  continue;
	}
      r = rpmatch (cases[i].string);
      if (r != cases[i].result)
	{
	  printf ("Failed: Locale %s, String %s, Exp: %d, got %d\n",
		  cases[i].locale, cases[i].string, cases[i].result, r);
	  result = EXIT_FAILURE;
	}
    }
  return result;
}
