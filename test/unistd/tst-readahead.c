/* readahead() carries a 64-bit offset.  On a 32-bit ABI the value is split
   into a register pair, and uClibc-ng decides that by __WORDSIZE alone, which
   is wrong for an ILP32 ABI on 64-bit registers (mips n32, x86 x32): there the
   kernel wants the offset whole, and a split shifts the length along with it.

   WHAT THIS TEST CHECKS: that the kernel accepts the call at all -- that it
   succeeds where it has to and fails where it has to.

   WHAT IT DOES NOT CHECK, and this matters because a green result here reads
   like more than it is: whether the kernel then reads the range we asked for.
   readahead() is advisory and answers 0 whether or not it understood its
   arguments, and no errno reports a wrong offset.  The one effect that can be
   observed is pages arriving in the page cache -- which this test does not
   try, because the suite usually runs on a RAM-backed root file system, where
   every page is resident from the start and the kernel skips readahead
   altogether.  Run from a disk-backed file system the effect would be
   measurable, and anyone who wants to go that far has a way in: read the
   cache residency of a mapping over the range with mincore(2) before and
   after, on a file large enough that it is not already cached.

   Measured, not assumed: on mips n32 the library hands the kernel the upper
   half of the offset as the offset and the lower half as the length, and this
   test passes anyway.

   So a PASS here means "the call went through", never "the offset arrived".
   For an argument that shifted, the companion test tst-fadvise is the one that
   bites: its advice sits in the last register, so a shift turns a valid advice
   into EINVAL.  */

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

#ifndef __NR_readahead

int
main (void)
{
  puts ("this kernel has no readahead syscall");
  return 23;			/* SKIP */
}

#else

#define NAME "tst-readahead.tmp"

/* Offsets a 32-bit register pair would take apart differently.  */
static const long long offsets[] = {
  0,
  (2LL << 30) + 1,
  (4LL << 30) + 1,
  8LL << 30,
};

static int failed;

int
main (void)
{
  char buf[4096];
  int fd, pipefd[2];
  size_t i;
  ssize_t r;

  fd = open (NAME, O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (fd < 0)
    {
      perror ("open");
      return 1;
    }
  memset (buf, 'x', sizeof buf);
  if (write (fd, buf, sizeof buf) != (ssize_t) sizeof buf)
    {
      perror ("write");
      close (fd);
      unlink (NAME);
      return 1;
    }

  errno = 0;
  r = readahead (fd, 0, sizeof buf);
  if (r < 0 && errno == ENOSYS)
    {
      puts ("readahead is not implemented here");
      close (fd);
      unlink (NAME);
      return 23;		/* SKIP */
    }

  /* These offsets are not verified -- see the top of the file.  They are here
     so a wrapper that turns a large offset into an error is caught.  */
  for (i = 0; i < sizeof offsets / sizeof offsets[0]; i++)
    {
      errno = 0;
      r = readahead (fd, (off64_t) offsets[i], sizeof buf);
      if (r < 0)
	{
	  printf ("  readahead(%lld, %zu) said %s\n", offsets[i], sizeof buf,
		  strerror (errno));
	  failed = 1;
	}
    }

  /* A length past 32 bits.  */
  errno = 0;
  if (readahead (fd, 0, (size_t) -1) < 0 && errno != EINVAL)
    {
      printf ("  readahead(0, SIZE_MAX) said %s\n", strerror (errno));
      failed = 1;
    }

  /* The file descriptor is the first argument, so these two say whether it
     still arrives where the kernel looks for it.  */
  errno = 0;
  if (readahead (-1, 0, sizeof buf) >= 0 || errno != EBADF)
    {
      printf ("  readahead on a closed descriptor said %s, expected EBADF\n",
	      strerror (errno));
      failed = 1;
    }

  if (pipe (pipefd) == 0)
    {
      /* The kernel serves only regular files and block devices here.  */
      errno = 0;
      if (readahead (pipefd[0], 0, sizeof buf) >= 0)
	{
	  puts ("  readahead on a pipe succeeded, expected a refusal");
	  failed = 1;
	}
      close (pipefd[0]);
      close (pipefd[1]);
    }

  close (fd);
  unlink (NAME);
  return failed;
}

#endif
