/* The helpers of libc-test (https://wiki.musl-libc.org/libc-test.html) that
   the tests taken from it use, from src/common/test.h, print.c and setrlim.c.

   libc-test is licensed under the following standard MIT license:

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

#ifndef LIBC_TEST_H
#define LIBC_TEST_H

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

/* Each test is a single file, so the helpers are static here.  */
static volatile int t_status = 0;

#define T_LOC2(l) __FILE__ ":" #l
#define T_LOC1(l) T_LOC2(l)
#define t_error(...) t_printf(T_LOC1(__LINE__) ": " __VA_ARGS__)

static int __attribute__((unused)) t_printf(const char *s, ...)
{
	va_list ap;
	char buf[512];
	int n;

	t_status = 1;
	va_start(ap, s);
	n = vsnprintf(buf, sizeof buf, s, ap);
	va_end(ap);
	if (n < 0)
		n = 0;
	else if (n >= (int) sizeof buf) {
		n = sizeof buf;
		buf[n - 1] = '\n';
		buf[n - 2] = '.';
		buf[n - 3] = '.';
		buf[n - 4] = '.';
	}
	return write(1, buf, n);
}

static int __attribute__((unused)) t_setrlim(int r, long lim)
{
	struct rlimit rl;

	if (getrlimit(r, &rl)) {
		t_error("getrlimit %d: %s\n", r, strerror(errno));
		return -1;
	}
	if (lim > (long) rl.rlim_max)
		return -1;
	if (lim == (long) rl.rlim_max && lim == (long) rl.rlim_cur)
		return 0;
	rl.rlim_max = lim;
	rl.rlim_cur = lim;
	if (setrlimit(r, &rl)) {
		t_error("setrlimit(%d, %ld): %s\n", r, lim, strerror(errno));
		return -1;
	}
	return 0;
}

#endif
