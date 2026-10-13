/* posix_fadvise() hands the kernel a 64-bit offset and a 64-bit length.  On a
   32-bit ABI each of them is split into a register pair, and uClibc-ng decides
   that by __WORDSIZE alone.  That is wrong for an ILP32 ABI that runs on
   64-bit registers -- mips n32, x86 x32 -- where the kernel wants each value
   whole: splitting shifts every later argument one register along.

   The shift is visible, and that is the point of this test.  The advice is the
   last argument, so once it has moved the kernel reads it from the wrong place
   and turns down a perfectly ordinary advice with EINVAL.

   posix_fadvise() reports the error as its return value and leaves errno
   alone.  */

#define _GNU_SOURCE 1
#define _LARGEFILE_SOURCE 1
#define _LARGEFILE64_SOURCE 1
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/syscall.h>

#if !defined __NR_fadvise64 && !defined __NR_fadvise64_64

int
main (void)
{
  puts ("this kernel has no fadvise64 syscall");
  return 23;			/* SKIP */
}

#else

#define NAME "tst-fadvise.tmp"

static const struct
{
  const char *name;
  int advice;
} advices[] = {
  { "NORMAL",     POSIX_FADV_NORMAL },
  { "SEQUENTIAL", POSIX_FADV_SEQUENTIAL },
  { "RANDOM",     POSIX_FADV_RANDOM },
  { "WILLNEED",   POSIX_FADV_WILLNEED },
  { "DONTNEED",   POSIX_FADV_DONTNEED },
  { "NOREUSE",    POSIX_FADV_NOREUSE },
};

/* Offsets a 32-bit register pair would take apart differently.  */
static const long long offsets[] = {
  0,
  (2LL << 30) + 1,
  (4LL << 30) + 1,
  8LL << 30,
};

static int failed;

static void
check (const char *call, const char *advice, long long off, int ret)
{
  if (ret != 0)
    {
      printf ("  %s(%lld, 4096, %s) said %s\n", call, off, advice,
	      strerror (ret));
      failed = 1;
    }
}

int
main (void)
{
  size_t a, o;
  int fd, ret;

  fd = open (NAME, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    {
      perror ("open");
      return 1;
    }
  if (write (fd, "x", 1) != 1)
    {
      perror ("write");
      close (fd);
      unlink (NAME);
      return 1;
    }

  ret = posix_fadvise (fd, 0, 1, POSIX_FADV_NORMAL);
  if (ret == ENOSYS)
    {
      puts ("posix_fadvise is not implemented here");
      close (fd);
      unlink (NAME);
      return 23;		/* SKIP */
    }

  /* Does this kernel reject a bad advice at all?  If it does not, a shifted
     argument cannot show up as EINVAL and the rest of the test is blind.  Say
     so rather than report a pass that means nothing.  */
  if (posix_fadvise (fd, 0, 1, 12345) == 0)
    puts ("note: this kernel accepts any advice, so a shifted argument "
	  "would go unnoticed");

  for (a = 0; a < sizeof advices / sizeof advices[0]; a++)
    for (o = 0; o < sizeof offsets / sizeof offsets[0]; o++)
      {
	check ("posix_fadvise", advices[a].name, offsets[o],
	       posix_fadvise (fd, (off_t) offsets[o], 4096, advices[a].advice));
	check ("posix_fadvise64", advices[a].name, offsets[o],
	       posix_fadvise64 (fd, (off64_t) offsets[o], 4096,
				advices[a].advice));
      }

  /* A length past 32 bits has to survive as well.  */
  check ("posix_fadvise", "WILLNEED len=8G", 0,
	 posix_fadvise (fd, 0, 8LL << 30, POSIX_FADV_WILLNEED));
  check ("posix_fadvise64", "WILLNEED len=8G", 0,
	 posix_fadvise64 (fd, 0, 8LL << 30, POSIX_FADV_WILLNEED));

  close (fd);
  unlink (NAME);
  return failed;
}

#endif
