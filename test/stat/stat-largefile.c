/* A file larger than 4 GB must come back from stat() with its size intact.
   With _FILE_OFFSET_BITS=64 that is what off_t promises, and every st_size
   the library reports has to be wide enough to carry it. */

#define _LARGEFILE_SOURCE 1
#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <features.h>

#if defined __UCLIBC__ && !defined __UCLIBC_HAS_LFS__

#include <stdio.h>
int main (void)
{
  puts ("needs UCLIBC_HAS_LFS");
  return 23;			/* SKIP */
}

#else

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>

#define NAME "stat-largefile.tmp"

/* 5 GiB: past the 4 GiB where an unsigned 32-bit field wraps, and past the
   2 GiB where a signed one goes negative. */
static const long long BIG = 5LL * 1024 * 1024 * 1024;

static int failed;

static void
check (const char *what, long long got)
{
  if (got == BIG)
    printf ("%-10s %lld  ok\n", what, got);
  else
    {
      printf ("%-10s %lld  FAIL, expected %lld\n", what, got, BIG);
      failed = 1;
    }
}

int
main (void)
{
  struct rlimit rl;
  struct stat s;
  struct stat64 s64;
  int fd;

  /* Without a 64-bit off_t the rest of the test cannot even be expressed:
     the constant would not survive the assignment. */
  printf ("sizeof(off_t)   %d\n", (int) sizeof (off_t));
  printf ("sizeof(st_size) %d  (struct stat)\n",
	  (int) sizeof (((struct stat *) 0)->st_size));
  printf ("sizeof(st_size) %d  (struct stat64)\n",
	  (int) sizeof (((struct stat64 *) 0)->st_size));

  if (sizeof (off_t) < 8)
    {
      puts ("off_t is not 64 bit although _FILE_OFFSET_BITS=64 was asked for");
      return 1;
    }

  if (getrlimit (RLIMIT_FSIZE, &rl) == 0
      && rl.rlim_cur != RLIM_INFINITY && (long long) rl.rlim_cur < BIG)
    {
      printf ("RLIMIT_FSIZE is %lld, below the %lld this test needs\n",
	      (long long) rl.rlim_cur, BIG);
      return 23;		/* SKIP */
    }

  fd = open (NAME, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    {
      perror ("open");
      return 1;
    }

  /* Sparse on purpose: nothing is written, so this costs metadata and not
     five gigabytes of a RAM-backed root filesystem. */
  if (ftruncate (fd, (off_t) BIG) < 0)
    {
      int e = errno;
      printf ("ftruncate to %lld: %s\n", BIG, strerror (e));
      close (fd);
      unlink (NAME);
      /* A file system that will not hold a file this large is not a libc
	 fault -- say so rather than reporting a failure. */
      if (e == EFBIG || e == ENOSPC || e == EINVAL || e == EPERM)
	return 23;		/* SKIP */
      return 1;
    }

  memset (&s, 0, sizeof s);
  if (fstat (fd, &s) < 0)
    perror ("fstat"), failed = 1;
  else
    check ("fstat", (long long) s.st_size);

  memset (&s, 0, sizeof s);
  if (stat (NAME, &s) < 0)
    perror ("stat"), failed = 1;
  else
    check ("stat", (long long) s.st_size);

  memset (&s, 0, sizeof s);
  if (lstat (NAME, &s) < 0)
    perror ("lstat"), failed = 1;
  else
    check ("lstat", (long long) s.st_size);

  memset (&s64, 0, sizeof s64);
  if (fstat64 (fd, &s64) < 0)
    perror ("fstat64"), failed = 1;
  else
    check ("fstat64", (long long) s64.st_size);

  memset (&s64, 0, sizeof s64);
  if (stat64 (NAME, &s64) < 0)
    perror ("stat64"), failed = 1;
  else
    check ("stat64", (long long) s64.st_size);

  memset (&s64, 0, sizeof s64);
  if (lstat64 (NAME, &s64) < 0)
    perror ("lstat64"), failed = 1;
  else
    check ("lstat64", (long long) s64.st_size);

  /* lseek agrees with the kernel independently of struct stat, so a
     disagreement here tells which of the two is wrong. */
  check ("lseek_end", (long long) lseek (fd, 0, SEEK_END));

  close (fd);
  unlink (NAME);
  return failed;
}

#endif
