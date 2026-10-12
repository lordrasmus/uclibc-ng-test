/* getauxval() answers for every entry the kernel actually passed.

   The oracle is /proc/self/auxv: the kernel's own copy of the vector the
   program was started with.  Comparing against it covers the whole
   function without the test having to know which entries libc keeps in
   its cache and which ones it has to search for -- that boundary is an
   implementation detail, and a test that encodes it would stop testing
   the search path the day the cache grows.

   The same oracle decides whether this target has an auxiliary vector at
   all.  It does not on binfmt_flat, where the kernel builds the initial
   stack from argc/argv/envp only; there the right answer to every
   request is ENOENT, and that is what gets checked instead.

   Note that merely compiling this is a test as well: sys/auxv.h was kept
   out of static sysroots long after getauxval() had learned to work
   without ld.so.  */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/auxv.h>
#include <sys/types.h>
#include <unistd.h>

/* The kernel writes the vector as pairs of elf_addr_t, which is the word
   size on every port -- so a pair of unsigned long reads it on 32 and 64
   bit alike, without dragging in Elf32_auxv_t vs Elf64_auxv_t.  */
#define MAXENT 64
static unsigned long a_type[MAXENT];
static unsigned long a_val[MAXENT];
static unsigned nent;
static int have_oracle;

static void
read_proc_auxv (void)
{
  unsigned long pair[2];
  FILE *f = fopen ("/proc/self/auxv", "rb");

  if (f == NULL)
    {
      printf ("/proc/self/auxv is not readable (%m): the value checks are"
	      " skipped, only the not-found behaviour is tested\n");
      return;
    }

  have_oracle = 1;
  while (nent < MAXENT && fread (pair, sizeof pair, 1, f) == 1)
    {
      if (pair[0] == AT_NULL)
	break;
      a_type[nent] = pair[0];
      a_val[nent] = pair[1];
      nent++;
    }
  fclose (f);
}

/* AT_NULL is the terminator, never a request, and libc caches entries in
   an array indexed by type -- so index 0 cannot tell "absent" from
   "present and zero".  Nothing asks for it; leave it alone.  */
static int
present (unsigned long type)
{
  unsigned i;

  for (i = 0; i < nent; i++)
    if (a_type[i] == type)
      return 1;
  return 0;
}

static int
occurrences (unsigned long type)
{
  unsigned i, n = 0;

  for (i = 0; i < nent; i++)
    if (a_type[i] == type)
      n++;
  return n;
}

/* Cross-check one entry against a value obtained a different way.  Only
   runs when the kernel really passed that entry.  */
static int
check_against (unsigned long type, const char *name, unsigned long expect,
	       const char *how)
{
  unsigned long got;

  if (!have_oracle || !present (type))
    return 0;

  got = getauxval (type);
  if (got != expect)
    {
      printf ("%s = %lu, but %s says %lu\n", name, got, how, expect);
      return 1;
    }
  return 0;
}

static int
check_absent (unsigned long type, const char *what)
{
  unsigned long got;

  errno = 0;
  got = getauxval (type);
  if (got != 0 || errno != ENOENT)
    {
      printf ("getauxval(%lu) (%s) returned %lu with errno %d, expected 0"
	      " with ENOENT (%d)\n", type, what, got, errno, ENOENT);
      return 1;
    }
  return 0;
}

static int
do_test (void)
{
  unsigned long type;
  unsigned i;
  int result = 0;

  read_proc_auxv ();

  /* 1. Every entry the kernel passed must come back unchanged.  */
  for (i = 0; i < nent; i++)
    {
      unsigned long got;

      if (occurrences (a_type[i]) != 1)
	{
	  printf ("type %lu appears more than once in the vector; which one"
		  " getauxval returns is not specified, skipping it\n",
		  a_type[i]);
	  continue;
	}

#ifndef __UCLIBC__
      /* Only for the control build against another libc: glibc answers
	 AT_HWCAP and AT_HWCAP2 from GLRO(dl_hwcap), which on x86 holds its
	 own feature bits rather than what the kernel passed.  uClibc-ng
	 reports the vector, so it is held to it.  */
      if (a_type[i] == AT_HWCAP
# ifdef AT_HWCAP2
	  || a_type[i] == AT_HWCAP2
# endif
	 )
	continue;
#endif

      errno = 0;
      got = getauxval (a_type[i]);
      if (got != a_val[i])
	{
	  printf ("getauxval(%lu) = 0x%lx, /proc/self/auxv says 0x%lx\n",
		  a_type[i], got, a_val[i]);
	  result = 1;
	}
    }

  if (have_oracle)
    printf ("checked %u entries against /proc/self/auxv\n", nent);

  /* 2. Two of them can be confirmed without /proc at all, which is worth
     doing: it catches a vector that is self-consistently wrong.  */
  result |= check_against (AT_PAGESZ, "AT_PAGESZ",
			   (unsigned long) sysconf (_SC_PAGESIZE),
			   "sysconf(_SC_PAGESIZE)");
  result |= check_against (AT_UID, "AT_UID", (unsigned long) getuid (),
			   "getuid()");
  result |= check_against (AT_EUID, "AT_EUID", (unsigned long) geteuid (),
			   "geteuid()");
  result |= check_against (AT_GID, "AT_GID", (unsigned long) getgid (),
			   "getgid()");
  result |= check_against (AT_EGID, "AT_EGID", (unsigned long) getegid (),
			   "getegid()");

  /* 3. A type that was not passed must report ENOENT rather than a stale
     or zero value.  Take one small and one large number, because a cache
     indexed by type misses them on different paths.  */
  if (have_oracle)
    {
      for (type = 1; type < 1000 && present (type); type++)
	;
      result |= check_absent (type, "lowest unused type");
    }
  result |= check_absent (0xfeedUL, "far outside any defined type");

  if (have_oracle && nent == 0)
    printf ("this target starts programs without an auxiliary vector;"
	    " only the not-found behaviour applies\n");

  return result;
}

#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
