/* confstr names that describe the loaded uClibc-ng itself.

   _CS_UCLIBC_VERSION has to match the headers we were built against.
   _CS_UCLIBC_ATOMICS is answered only where the architecture says how
   it does its atomics; everywhere else it has to fail with EINVAL.
   _CS_GNU_LIBC_VERSION has to stay unanswered: programs read it as
   "this is glibc".  */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define STR_(x) #x
#define STR(x) STR_(x)

int
main (void)
{
#ifndef _CS_UCLIBC_VERSION
  puts ("SKIP: no _CS_UCLIBC_VERSION in these headers");
  return 0;
#else
  char buf[64];
  size_t n;
  int bad = 0;

  n = confstr (_CS_UCLIBC_VERSION, buf, sizeof buf);
  printf ("UCLIBC_VERSION: %s\n", n ? buf : "(none)");
  if (n == 0 || strcmp (buf, "uClibc-ng " STR (__UCLIBC_MAJOR__) "."
			STR (__UCLIBC_MINOR__) "."
			STR (__UCLIBC_SUBLEVEL__)) != 0)
    {
      puts ("FAIL: version does not match the headers");
      bad = 1;
    }
  else if (n != strlen (buf) + 1)
    {
      printf ("FAIL: returned %zu for a string of %zu\n", n, strlen (buf));
      bad = 1;
    }

  errno = 0;
  n = confstr (_CS_UCLIBC_ATOMICS, buf, sizeof buf);
  if (n != 0)
    printf ("UCLIBC_ATOMICS: %s\n", buf);
  else if (errno == EINVAL)
    puts ("UCLIBC_ATOMICS: not answered on this architecture");
  else
    {
      printf ("FAIL: _CS_UCLIBC_ATOMICS gave 0 with errno %d\n", errno);
      bad = 1;
    }
#if defined __sparc__ && !defined __arch64__
  if (n == 0)
    {
      puts ("FAIL: sparc has to answer _CS_UCLIBC_ATOMICS");
      bad = 1;
    }
#endif

  errno = 0;
  n = confstr (_CS_GNU_LIBC_VERSION, buf, sizeof buf);
  if (n != 0 || errno != EINVAL)
    {
      printf ("FAIL: _CS_GNU_LIBC_VERSION answered (%zu, errno %d)\n",
	      n, errno);
      bad = 1;
    }

  return bad;
#endif
}
