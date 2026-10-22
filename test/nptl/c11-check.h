/* The two checks the C11 thread tests from glibc use from its
   support/check.h.  */
#ifndef C11_CHECK_H
#define C11_CHECK_H

#include <stdio.h>
#include <stdlib.h>

#define FAIL_EXIT1(...)						\
  do {								\
    printf ("%s:%d: error: ", __FILE__, __LINE__);		\
    printf (__VA_ARGS__);					\
    putchar ('\n');						\
    exit (1);							\
  } while (0)

#define TEST_VERIFY(expr)					\
  do {								\
    if (!(expr))						\
      FAIL_EXIT1 ("test failed: %s", #expr);			\
  } while (0)

#endif
