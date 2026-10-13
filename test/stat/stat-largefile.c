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

/* ftruncate() failing proves nothing on its own: the library may be unable to
   express the size, or the file system may be unwilling to hold it, and both
   report EFBIG.  Ask a second way -- seek to the last byte and write it.  A
   seek that lands somewhere else is the library's fault and is reported as
   such; only a write the file system turns down means the environment cannot
   do this.  Returns 0 if the file now has the size, 1 if it cannot have it.  */
static int
make_size (int fd, long long want)
{
  off_t pos;
  int e;

  /* One byte first, and not for the data.  A kernel without an MMU reads a
     truncate of an EMPTY file as "this is about to be mapped shared", and a
     shared mapping there has to be one physically contiguous block, so it
     allocates the whole file up front and turns down anything past
     MAX_ORDER pages -- 4 MB -- with EFBIG (fs/ramfs/file-nommu.c,
     ramfs_nommu_resize).  Once the file holds something, that guess no longer
     applies and the plain truncate_setsize() path runs, which only moves
     i_size.  Measured on riscv32 without an MMU: empty to 8 GB is EFBIG, one
     byte to 8 GB works.

     A library that cannot express the size still fails here, because it fails
     before the kernel is asked at all.  */
  if (write (fd, "", 1) != 1)
    {
      perror ("  write");
      failed = 1;
      return 1;
    }

  if (ftruncate (fd, (off_t) want) == 0)
    return 0;
  e = errno;

  pos = lseek (fd, (off_t) (want - 1), SEEK_SET);
  if (pos != (off_t) (want - 1))
    {
      printf ("  ftruncate: %s, and lseek to %lld answered %lld\n",
	      strerror (e), want - 1, (long long) pos);
      failed = 1;
      return 1;
    }

  if (write (fd, "", 1) != 1)
    {
      printf ("  skipped: ftruncate said %s, writing the last byte said %s\n",
	      strerror (e), strerror (errno));
      return 1;
    }

  printf ("  ftruncate said %s although the file system holds the size\n",
	  strerror (e));
  failed = 1;
  return 0;
}

static void
one_size (long long want)
{
  struct stat s;
  struct stat64 s64;
  struct rlimit rl;
  int fd;

  printf ("%lld\n", want);

  if (getrlimit (RLIMIT_FSIZE, &rl) == 0
      && rl.rlim_cur != RLIM_INFINITY && (long long) rl.rlim_cur < want)
    {
      printf ("  skipped: RLIMIT_FSIZE is %lld\n", (long long) rl.rlim_cur);
      return;
    }

  fd = open (NAME, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    {
      perror ("  open");
      failed = 1;
      return;
    }

  /* Sparse on purpose: at most two bytes are written, so this costs an inode
     and not eight gigabytes of a RAM-backed root file system.  */
  if (make_size (fd, want) != 0)
    {
      close (fd);
      unlink (NAME);
      return;
    }

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

  if (failed)
    return 1;
  if (!measured)
    {
      puts ("no size could be created here");
      return 23;		/* SKIP */
    }
  return 0;
}
