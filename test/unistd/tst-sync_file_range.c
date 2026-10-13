/* sync_file_range() carries two 64-bit values, an offset and a length, and on
   a 32-bit ABI each of them is split into a register pair.  Whether to split
   is decided by __WORDSIZE, which is the wrong question on an ILP32 ABI with
   64-bit registers (mips n32) and was not asked at all for the plain 64-bit
   case, so a split happened there too.

   Unlike readahead(), this one can be caught from userspace, because the
   kernel validates the flags: anything outside SYNC_FILE_RANGE_WAIT_BEFORE |
   WRITE | WAIT_AFTER is EINVAL.  Split, the arguments move along by one
   register and the kernel reads some half of the length as the flags.

   The trick in the values below is that it has to work on either endianness.
   A split passes the two halves in the order the machine stores them, so a
   test that relies on the upper half landing somewhere specific only catches
   one of the two.  Giving a value the SAME two halves settles that: whichever
   half the kernel picks up as the flags, it is the same number, and 8 is
   outside the valid set.  A library that hands the value over whole passes
   the real flags and the call succeeds.  */

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

#if !defined __NR_sync_file_range && !defined __NR_sync_file_range2

int
main (void)
{
  puts ("this kernel has no sync_file_range syscall");
  return 23;			/* SKIP */
}

#else

#define NAME "tst-sync_file_range.tmp"

/* Both halves 8: invalid as a flag word, and the same whichever half a split
   leaves behind.  */
#define BOTH_HALVES_8	(((long long) 8 << 32) | 8)

static int failed;
static int fd;

static void
one (const char *what, long long off, long long len, int flags)
{
  errno = 0;
  if (sync_file_range (fd, (off64_t) off, (off64_t) len, flags) != 0)
    {
      printf ("  %-26s off %lld len %lld: %s\n", what, off, len,
	      strerror (errno));
      failed = 1;
    }
}

int
main (void)
{
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

  errno = 0;
  if (sync_file_range (fd, 0, 0, SYNC_FILE_RANGE_WRITE) != 0 && errno == ENOSYS)
    {
      puts ("sync_file_range is not implemented here");
      close (fd);
      unlink (NAME);
      return 23;		/* SKIP */
    }

  one ("plain", 0, 4096, SYNC_FILE_RANGE_WRITE);
  one ("length past 32 bits", 0, 8LL << 30, SYNC_FILE_RANGE_WRITE);
  one ("offset past 32 bits", 8LL << 30, 4096, SYNC_FILE_RANGE_WRITE);

  /* The two that bite: a split leaves an 8 where the flags belong.  */
  one ("length with equal halves", 0, BOTH_HALVES_8, SYNC_FILE_RANGE_WRITE);
  one ("offset with equal halves", BOTH_HALVES_8, BOTH_HALVES_8,
       SYNC_FILE_RANGE_WRITE);

  one ("all three flags", 0, 4096,
       SYNC_FILE_RANGE_WAIT_BEFORE | SYNC_FILE_RANGE_WRITE
       | SYNC_FILE_RANGE_WAIT_AFTER);

  /* Everything above rests on the kernel turning down a flag it does not
     know, so ask it -- but only now, and only if nothing failed.  Asking
     earlier would be asking through the very wrapper under test: where the
     arguments shift, the 8 never reaches the kernel either, and the answer
     comes back "accepts anything" exactly when the fault is present.  With
     the calls above intact, the shift is ruled out and the question is
     honest.  */
  if (!failed)
    {
      errno = 0;
      if (sync_file_range (fd, 0, 4096, 8) == 0)
	{
	  puts ("inconclusive: this kernel accepts a flag it should refuse,");
	  puts ("so a shifted argument would not have shown up here");
	  close (fd);
	  unlink (NAME);
	  return 23;		/* SKIP */
	}
    }

  close (fd);
  unlink (NAME);
  return failed;
}

#endif
