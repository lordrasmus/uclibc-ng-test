/* Sizes past what a 32-bit field can hold must survive ftruncate(), lseek()
   and the whole stat() family.  off_t is 64 bit in uClibc-ng -- features.h
   forces _FILE_OFFSET_BITS to 64 and rejects anything else -- so every one of
   these calls has to carry the value whole.

   The four sizes differ in how a 32-bit field would mangle them, which turns
   a failure into a diagnosis:

     2 GiB + 1   smallest value a signed 32-bit field cannot hold
     3 GiB       top bit set: a signed 32-bit field reads it as negative
     4 GiB + 1   just past where an unsigned 32-bit field wraps
     8 GiB       the low 32 bits are zero, so a truncated value reads as 0  */

#define _LARGEFILE_SOURCE 1
#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>

#define NAME "stat-largefile.tmp"

static const long long sizes[] = {
  (2LL << 30) + 1,
  3LL << 30,
  (4LL << 30) + 1,
  8LL << 30,
};

static int failed;
static int measured;

static void
cmp (const char *what, long long want, long long got)
{
  if (got != want)
    {
      printf ("  %-10s %lld, expected %lld\n", what, got, want);
      failed = 1;
    }
}

/* Returns 0 when the size could be measured, 1 when the environment refused
   it -- a file system that will not hold the file says nothing about libc.  */
static int
one_size (long long want)
{
  struct stat s;
  struct stat64 s64;
  struct rlimit rl;
  int fd, e;

  if (getrlimit (RLIMIT_FSIZE, &rl) == 0
      && rl.rlim_cur != RLIM_INFINITY && (long long) rl.rlim_cur < want)
    {
      printf ("%-12lld skipped, RLIMIT_FSIZE is %lld\n",
	      want, (long long) rl.rlim_cur);
      return 1;
    }

  fd = open (NAME, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    {
      perror ("open");
      failed = 1;
      return 1;
    }

  /* Sparse on purpose: nothing is written, so this costs an inode and not
     eight gigabytes of a RAM-backed root file system.  */
  if (ftruncate (fd, (off_t) want) < 0)
    {
      e = errno;
      close (fd);
      unlink (NAME);
      if (e == EFBIG || e == ENOSPC || e == EINVAL || e == EPERM)
	{
	  printf ("%-12lld skipped, ftruncate: %s\n", want, strerror (e));
	  return 1;
	}
      printf ("%-12lld ftruncate: %s\n", want, strerror (e));
      failed = 1;
      return 1;
    }

  printf ("%-12lld\n", want);
  measured = 1;

  /* lseek reaches the kernel's idea of the size without going through
     struct stat, so it tells the two apart when they disagree.  */
  cmp ("lseek_set", want, (long long) lseek (fd, (off_t) want, SEEK_SET));
  cmp ("lseek_end", want, (long long) lseek (fd, 0, SEEK_END));

  memset (&s, 0, sizeof s);
  if (fstat (fd, &s) < 0)
    perror ("  fstat"), failed = 1;
  else
    cmp ("fstat", want, (long long) s.st_size);

  memset (&s, 0, sizeof s);
  if (stat (NAME, &s) < 0)
    perror ("  stat"), failed = 1;
  else
    cmp ("stat", want, (long long) s.st_size);

  memset (&s, 0, sizeof s);
  if (lstat (NAME, &s) < 0)
    perror ("  lstat"), failed = 1;
  else
    cmp ("lstat", want, (long long) s.st_size);

  memset (&s64, 0, sizeof s64);
  if (fstat64 (fd, &s64) < 0)
    perror ("  fstat64"), failed = 1;
  else
    cmp ("fstat64", want, (long long) s64.st_size);

  memset (&s64, 0, sizeof s64);
  if (stat64 (NAME, &s64) < 0)
    perror ("  stat64"), failed = 1;
  else
    cmp ("stat64", want, (long long) s64.st_size);

  memset (&s64, 0, sizeof s64);
  if (lstat64 (NAME, &s64) < 0)
    perror ("  lstat64"), failed = 1;
  else
    cmp ("lstat64", want, (long long) s64.st_size);

  close (fd);
  unlink (NAME);
  return 0;
}

int
main (void)
{
  size_t i;

  printf ("sizeof(off_t)   %d\n", (int) sizeof (off_t));
  printf ("sizeof(st_size) %d (struct stat), %d (struct stat64)\n",
	  (int) sizeof (((struct stat *) 0)->st_size),
	  (int) sizeof (((struct stat64 *) 0)->st_size));

  if (sizeof (off_t) < 8)
    {
      puts ("off_t is not 64 bit, which uClibc-ng guarantees");
      return 1;
    }

  for (i = 0; i < sizeof sizes / sizeof sizes[0]; i++)
    one_size (sizes[i]);

  if (!measured)
    {
      puts ("no size could be created here");
      return 23;		/* SKIP */
    }
  return failed;
}
