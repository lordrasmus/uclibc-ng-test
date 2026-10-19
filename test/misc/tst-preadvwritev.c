/* Tests for preadv and pwritev.
   Copyright (C) 2016 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>

#if defined __UCLIBC__
/* uClibc declares these with a 64-bit offset even on a 32-bit ABI. */
# define PREADV preadv
# define PWRITEV pwritev
#else
# define PREADV preadv64
# define PWRITEV pwritev64
#endif
#define IO_OFFSET off64_t
#define IO_STAT struct stat64
#define IO_FSTAT fstat64
#define IO_OPEN open64

static void do_prepare (void);
#define PREPARE(argc, argv)     do_prepare ()
static int do_test (void);
#define TEST_FUNCTION           do_test ()
#include "test-skeleton.c"

static char *temp_filename;
static int temp_fd;

static void
do_prepare (void)
{
  temp_fd = create_temp_file ("tst-preadvwritev.", &temp_filename);
  if (temp_fd == -1)
    {
      printf ("cannot create temporary file: %m\n");
      exit (1);
    }
  close (temp_fd);
  temp_fd = IO_OPEN (temp_filename, O_RDWR);
  if (temp_fd == -1)
    {
      perror ("reopening temporary file");
      exit (1);
    }
}

#define FAIL(str) \
  do { printf ("error: %s (line %d)\n", str, __LINE__); return 1; } while (0)

static int
do_test_with_offset (IO_OFFSET offset)
{
  struct iovec iov[2];
  ssize_t ret;
  unsigned char expected[2][96], got[96];
  IO_STAT st;
  int region, j;

  if (lseek (temp_fd, 19, SEEK_SET) != 19)
    FAIL ("cannot set initial file position");

  for (region = 0; region < 2; region++)
    {
      for (j = 0; j < 96; j++)
        expected[region][j] = (unsigned char) (j * 7 + region * 53
                                             + ((unsigned int) offset & 255));
      iov[0].iov_base = expected[region];
      iov[0].iov_len = 32;
      iov[1].iov_base = expected[region] + 32;
      iov[1].iov_len = 64;
      ret = PWRITEV (temp_fd, iov, 2, offset + region * 96);
      if (ret != 96)
        {
          printf ("pwritev at %lld returned %ld (errno %d), expected 96\n",
                  (long long) offset + region * 96, (long) ret, errno);
          return 1;
        }
      if (lseek (temp_fd, 0, SEEK_CUR) != 19)
        FAIL ("pwritev changed the file position");
    }
  /* An independent size check catches matching truncation in both wrappers. */
  if (IO_FSTAT (temp_fd, &st) != 0 || st.st_size != offset + 192)
    FAIL ("wrong file size after pwritev");

  for (region = 0; region < 2; region++)
    {
      memset (got, 0xa5, sizeof got);
      iov[0].iov_base = got;
      iov[0].iov_len = 32;
      iov[1].iov_base = got + 32;
      iov[1].iov_len = 64;
      ret = PREADV (temp_fd, iov, 2, offset + region * 96);
      if (ret != 96)
        FAIL ("preadv returned an unexpected byte count");
      if (memcmp (expected[region], got, sizeof got))
        FAIL ("preadv returned different data");
      if (lseek (temp_fd, 0, SEEK_CUR) != 19)
        FAIL ("preadv changed the file position");
    }

  return 0;
}

static int
do_test (void)
{
  struct iovec iov;
  unsigned char low[96], got[96];
  static const long long offsets[] = { 0x80000011LL, 0x100000025LL };
  size_t i;

  if (do_test_with_offset (0))
    return 1;
  iov.iov_base = low;
  iov.iov_len = sizeof low;
  if (PREADV (temp_fd, &iov, 1, 0) != sizeof low)
    FAIL ("cannot read low-offset sentinel");
  if (sizeof (IO_OFFSET) >= 8)
    for (i = 0; i < sizeof offsets / sizeof offsets[0]; i++)
      if (do_test_with_offset ((IO_OFFSET) offsets[i]))
        return 1;
  iov.iov_base = got;
  if (PREADV (temp_fd, &iov, 1, 0) != sizeof got
      || memcmp (low, got, sizeof got))
    FAIL ("large-offset writes changed the low-offset sentinel");
  if (lseek (temp_fd, 0, SEEK_CUR) != 19)
    FAIL ("sentinel read changed the file position");
  return 0;
}
