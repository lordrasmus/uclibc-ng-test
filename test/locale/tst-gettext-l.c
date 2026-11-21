/* The *_l forms of the gettext family, POSIX.1-2024.

   uClibc-ng has no message catalogues: the whole of libintl here is a set
   of stubs that hand the message id back.  The _l forms are the same, with
   a locale_t they ignore.  So what this checks is that they exist, that
   they are callable from C with the C linkage the library exports, and
   that they return what the stubs promise -- including which of the two
   plural forms ngettext_l picks.

   glibc does not have these yet, so there it reports SKIP.

   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.  */

#include <stdio.h>

#if !defined __UCLIBC__ || !defined __UCLIBC_HAS_LIBINTL__ \
    || !defined __UCLIBC_HAS_XLOCALE__

int main(void)
{
# ifndef __UCLIBC__
	printf("SKIP: the *_l forms of gettext are uClibc-ng only so far\n");
# elif !defined __UCLIBC_HAS_LIBINTL__
	printf("SKIP: UCLIBC_HAS_LIBINTL is off\n");
# else
	printf("SKIP: UCLIBC_HAS_XLOCALE is off\n");
# endif
	return 0;
}

#else

# include <libintl.h>
# include <locale.h>
# include <string.h>

static int fails;

static void
check(const char *what, const char *got, const char *want)
{
	if (got == NULL) {
		printf("FAIL: %s returned NULL\n", what);
		fails++;
		return;
	}
	if (strcmp(got, want) != 0) {
		printf("FAIL: %s returned \"%s\", expected \"%s\"\n",
		       what, got, want);
		fails++;
		return;
	}
	printf("PASS: %s -> \"%s\"\n", what, got);
}

int main(void)
{
	locale_t loc;

	loc = newlocale(LC_ALL_MASK, "C", (locale_t) 0);
	if (loc == (locale_t) 0) {
		printf("SKIP: newlocale(\"C\"): not available\n");
		return 0;
	}

	check("gettext_l", gettext_l("msg", loc), "msg");
	check("dgettext_l", dgettext_l("dom", "msg", loc), "msg");
	check("dcgettext_l", dcgettext_l("dom", "msg", LC_MESSAGES, loc),
	      "msg");

	/* The stub picks the singular for n == 1 and the plural for
	   anything else; that is the one decision it makes.  */
	check("ngettext_l n=1", ngettext_l("one", "many", 1, loc), "one");
	check("ngettext_l n=0", ngettext_l("one", "many", 0, loc), "many");
	check("ngettext_l n=2", ngettext_l("one", "many", 2, loc), "many");
	check("dngettext_l n=1",
	      dngettext_l("dom", "one", "many", 1, loc), "one");
	check("dngettext_l n=5",
	      dngettext_l("dom", "one", "many", 5, loc), "many");
	check("dcngettext_l n=1",
	      dcngettext_l("dom", "one", "many", 1, LC_MESSAGES, loc), "one");
	check("dcngettext_l n=3",
	      dcngettext_l("dom", "one", "many", 3, LC_MESSAGES, loc),
	      "many");

	/* A null locale_t is what a caller gets from an uninitialised
	   variable; the stubs ignore it, so they must not touch it.  */
	check("gettext_l with (locale_t) 0",
	      gettext_l("msg", (locale_t) 0), "msg");

	freelocale(loc);

	if (fails) {
		printf("%d failure(s)\n", fails);
		return 1;
	}
	printf("all ok\n");
	return 0;
}

#endif
