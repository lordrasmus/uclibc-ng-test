/* The int a C11 thread returns, or passes to thrd_exit, is what
   thrd_join hands back.  A C11 thread function returns int where a POSIX
   one returns void *; on some ABIs (m68k) the two come back in different
   registers, so this checks that the value takes the right way.  */

#include <stdio.h>
#include <threads.h>

static thrd_t main_thread;

static int
returns (void *arg)
{
  return *(int *) arg;
}

static int
exits (void *arg)
{
  if (thrd_equal (thrd_current (), main_thread))
    thrd_exit (1000);
  thrd_exit (*(int *) arg);
}

static int
check (thrd_start_t func, int val)
{
  thrd_t t;
  int res = 0;

  if (thrd_create (&t, func, &val) != thrd_success)
    {
      puts ("thrd_create failed");
      return 1;
    }
  if (thrd_join (t, &res) != thrd_success)
    {
      puts ("thrd_join failed");
      return 1;
    }
  if (res != val)
    {
      printf ("thread result %d, expected %d\n", res, val);
      return 1;
    }
  return 0;
}

static int
do_test (void)
{
  int err = 0;

  main_thread = thrd_current ();
  err |= check (returns, 42);
  err |= check (returns, -7);
  err |= check (exits, 4711);
  err |= check (exits, -1);
  return err;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
