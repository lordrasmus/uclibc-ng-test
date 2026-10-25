/* dlopen twenty modules with initial-exec TLS, more than the main
   thread's DTV has slots for at first.  Each module's variable lies in
   the static TLS block of every thread; a thread started before the
   dlopen has to get its initial value too.  Both threads check, for
   every module, that the module's own access and dlsym, which goes
   through __tls_get_addr, reach the same variable.  Up to uClibc-ng's
   fix for it, ld.so stopped the program once a module's DTV slot lay
   beyond the thread's DTV ("map->l_tls_modid <= dtv[-1].counter
   FAILED!"), and NPTL's hook wrote past the DTV's end.  */

#include <dlfcn.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define NMOD 20

static void *handle[NMOD];
static pthread_barrier_t barrier;

static int
check (const char *who, int set)
{
  int i, bad = 0;

  for (i = 0; i < NMOD; ++i)
    {
      int *(*addr) (void) = (int *(*) (void)) dlsym (handle[i], "ie_addr");
      int *direct, *viadlsym;

      if (addr == NULL)
	{
	  printf ("%s: module %d: no ie_addr: %s\n", who, i, dlerror ());
	  return 1;
	}
      direct = addr ();
      viadlsym = dlsym (handle[i], "ie_var");
      if (viadlsym != direct)
	{
	  printf ("%s: module %d: dlsym gives %p, the module %p\n",
		  who, i, (void *) viadlsym, (void *) direct);
	  bad = 1;
	  continue;
	}
      if (*direct != 100 + i)
	{
	  printf ("%s: module %d: initial value %d, expected %d\n",
		  who, i, *direct, 100 + i);
	  bad = 1;
	}
      *direct = set + i;
    }
  /* Each thread has its own copy: what this one wrote stays.  */
  for (i = 0; i < NMOD; ++i)
    {
      int *v = dlsym (handle[i], "ie_var");
      if (v != NULL && *v != set + i)
	{
	  printf ("%s: module %d: %d after writing %d\n", who, i, *v, set + i);
	  bad = 1;
	}
    }
  return bad;
}

static void *
thread (void *arg)
{
  (void) arg;
  pthread_barrier_wait (&barrier);	/* the modules are loaded */
  return (void *) (long) check ("thread", 2000);
}

int
main (void)
{
  pthread_t t;
  void *ret;
  int i, bad;
  char name[32];

  pthread_barrier_init (&barrier, NULL, 2);
  if (pthread_create (&t, NULL, thread, NULL) != 0)
    {
      puts ("pthread_create failed");
      return 1;
    }
  for (i = 0; i < NMOD; ++i)
    {
      snprintf (name, sizeof name, "tst-tlsmod-ie%d.so", i);
      handle[i] = dlopen (name, RTLD_NOW);
      if (handle[i] == NULL)
	{
	  printf ("dlopen %s: %s\n", name, dlerror ());
	  exit (1);
	}
    }
  bad = check ("main", 1000);
  pthread_barrier_wait (&barrier);
  pthread_join (t, &ret);
  return bad | (ret != NULL);
}
