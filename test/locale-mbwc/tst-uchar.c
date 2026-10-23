/* mbrtoc16, mbrtoc32, c16rtomb and mbrtowc on partial and
   supplementary characters; expectations from glibc.  */
#include <errno.h>
#include <langinfo.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
/* Without UCLIBC_HAS_UCHAR uClibc installs no <uchar.h>.  */
#if !defined __UCLIBC__ || defined __UCLIBC_HAS_UCHAR__
# include <uchar.h>
#endif

static int fails;

#define T(expr, want) do { \
	unsigned long long got_ = (unsigned long long) (expr); \
	if (got_ != (unsigned long long) (want)) { \
		printf("FAIL line %d: %s = %#llx, want %#llx\n", __LINE__, \
		       #expr, got_, (unsigned long long) (want)); \
		fails++; \
	} \
} while (0)

#if defined __UCLIBC__ && !defined __UCLIBC_HAS_UCHAR__
int main(void)
{
	printf("SKIP: UCLIBC_HAS_UCHAR is off\n");
	return 0;
}
#else
int main(void)
{
	mbstate_t st;
	char16_t c16;
	char32_t c32;
	wchar_t wc;
	char buf[8];
	int i;

	(void)(
	setlocale(LC_CTYPE, "en_US.UTF-8") ||
	setlocale(LC_CTYPE, "en_GB.UTF-8") ||
	setlocale(LC_CTYPE, "C.UTF-8") ||
	setlocale(LC_CTYPE, "") );
	if (strcmp(nl_langinfo(CODESET), "UTF-8")) {
		printf("SKIP: no UTF-8 locale (codeset=%s)\n", nl_langinfo(CODESET));
		return 0;
	}

	/* mbrtowc looks at no byte when n is 0.  */
	memset(&st, 0, sizeof st);
	T(mbrtowc(&wc, "", 0, &st), -2);
	T(mbrtowc(&wc, "a", 0, &st), -2);
	T(mbsinit(&st), 1);

	/* A two-byte character split in two.  */
	memset(&st, 0, sizeof st);
	T(mbrtoc16(&c16, "\xc3", 1, &st), -2);
	c16 = 0;
	T(mbrtoc16(&c16, "\xa4", 1, &st), 1);
	T(c16, 0xe4);
	T(mbsinit(&st), 1);

	/* A supplementary character whole, then byte by byte.  */
	memset(&st, 0, sizeof st);
	T(mbrtoc16(&c16, "\xf0\x9f\x98\x80", 4, &st), 4);
	T(c16, 0xd83d);
	T(mbsinit(&st), 0);
	T(mbrtoc16(&c16, "", 1, &st), -3);
	T(c16, 0xde00);
	T(mbsinit(&st), 1);

	memset(&st, 0, sizeof st);
	for (i = 0; i < 3; i++)
		T(mbrtoc16(&c16, "\xf0\x9f\x98\x80" + i, 1, &st), -2);
	T(mbrtoc16(&c16, "\x80", 1, &st), 1);
	T(c16, 0xd83d);
	T(mbrtoc16(&c16, "", 1, &st), -3);
	T(c16, 0xde00);

	/* The same with the functions' own state.  */
	T(mbrtoc16(&c16, "\xc3", 1, NULL), -2);
	T(mbrtoc16(&c16, "\xa4", 1, NULL), 1);
	T(c16, 0xe4);
	T(mbrtoc32(&c32, "\xf0\x9f", 2, NULL), -2);
	T(mbrtoc32(&c32, "\x98\x80", 2, NULL), 2);
	T(c32, 0x1f600);

	/* And back.  */
	memset(&st, 0, sizeof st);
	T(c16rtomb(buf, 0xd83d, &st), 0);
	T(mbsinit(&st), 0);
	T(c16rtomb(buf, 0xde00, &st), 4);
	T(memcmp(buf, "\xf0\x9f\x98\x80", 4), 0);
	T(c16rtomb(buf, 0xd83d, NULL), 0);
	T(c16rtomb(buf, 0xde00, NULL), 4);
	errno = 0;
	T(c16rtomb(buf, 0xde00, NULL), -1);
	T(errno, EILSEQ);

	if (fails)
		return 1;
	printf("PASS\n");
	return 0;
}
#endif
