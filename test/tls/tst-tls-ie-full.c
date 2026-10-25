/* A library with more initial-exec TLS than the static TLS block keeps
   for dlopen: dlopen has to fail and say why, and the program go on --
   uClibc-ng used to stop it with exit status 30.  With the room raised
   by LD_STATIC_TLS_EXTRA (on glibc the optional_static_tls tunable),
   the same library loads.  */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define EXTRA "131072"

/* microblaze gcc emits general-dynamic even for tls_model
   ("initial-exec"), so the library needs no static TLS there.  */
#ifdef __microblaze__
# define NO_INITIAL_EXEC 1
#else
# define NO_INITIAL_EXEC 0
#endif

int
main (int argc, char **argv)
{
  void *h;

  if (getenv ("LD_STATIC_TLS_EXTRA") == NULL)
    {
      h = dlopen ("tst-tlsmod-ie-big.so", RTLD_NOW);
      if (h != NULL && NO_INITIAL_EXEC)
	{
	  puts ("skipped: the compiler emits no initial-exec TLS");
	  dlclose (h);
	}
      else if (h != NULL)
	{
	  puts ("dlopen of a library whose TLS does not fit succeeded");
	  return 1;
	}
      else
	printf ("dlopen failed as it should: %s\n", dlerror ());

      /* The program goes on, and can still load what fits.  */
      h = dlopen ("tst-tlsmod-ie0.so", RTLD_NOW);
      if (h == NULL)
	{
	  printf ("dlopen of a small library after that: %s\n", dlerror ());
	  return 1;
	}

      /* Again with more room.  */
      setenv ("LD_STATIC_TLS_EXTRA", EXTRA, 1);
      setenv ("GLIBC_TUNABLES", "glibc.rtld.optional_static_tls=" EXTRA, 1);
      fflush (stdout);
      execv ("/proc/self/exe", argv);
      perror ("execv");
      return 1;
    }

  (void) argc;
  h = dlopen ("tst-tlsmod-ie-big.so", RTLD_NOW);
  if (h == NULL)
    {
      printf ("with LD_STATIC_TLS_EXTRA=" EXTRA ": %s\n", dlerror ());
      return 1;
    }
  int (*check) (void) = (int (*) (void)) dlsym (h, "ie_big_check");
  if (check == NULL || !check ())
    {
      puts ("the library's TLS is not what it should be");
      return 1;
    }
  return 0;
}
