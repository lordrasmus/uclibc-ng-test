/* Tests for pread and pwrite.
   Copyright (C) 1998, 2000 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@cygnus.com>, 1998.

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

#include <errno.h>
#include <error.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>


/* Allow testing of the 64-bit versions as well.  */
#ifndef PREAD
# define PREAD pread
# define PWRITE pwrite
# define IO_OFFSET off_t
# define IO_STAT struct stat
# define IO_FSTAT fstat
# define IO_MKSTEMP mkstemp
#endif

#define STRINGIFY(s) STRINGIFY2 (s)
#define STRINGIFY2(s) #s

/* Prototype for our test function.  */
extern void do_prepare (int argc, char *argv[]);
extern int do_test (int argc, char *argv[]);

/* We have a preparation function.  */
#define PREPARE do_prepare

/* We might need a bit longer timeout.  */
#define TIMEOUT 20 /* sec */

/* This defines the `main' function and some more.  */
#include "../test-skeleton.c"

/* These are for the temporary file we generate.  */
char *name;
int fd;

void
do_prepare (int argc, char *argv[])
{
   size_t name_len;

#define FNAME FNAME2(TRUNCATE)
#define FNAME2(s) "/" STRINGIFY(s) "XXXXXX"

   name_len = strlen (test_dir);
   name = malloc (name_len + sizeof (FNAME));
   if (name == NULL)
     error (EXIT_FAILURE, errno, "cannot allocate file name");
   mempcpy (mempcpy (name, test_dir, name_len), FNAME, sizeof (FNAME));
   add_temp_file (name);

   /* Open our test file.   */
   fd = IO_MKSTEMP (name);
   if (fd == -1)
     error (EXIT_FAILURE, errno, "cannot open test file `%s'", name);
}

/* Sparse writes consume only a few blocks, even at offsets above 4 GiB.
   fstat checks the offset independently of the matching read wrapper. */
static int
check_large_offsets (void)
{
  static const long long offsets[] = { 0x80000011LL, 0x100000025LL };
  unsigned char data[37], got[37], low[37];
  IO_STAT st;
  size_t i, j;

  if (sizeof (IO_OFFSET) < 8)
    return 0;
  if (PREAD (fd, low, sizeof low, 0) != sizeof low)
    error (EXIT_FAILURE, errno, "reading low-offset sentinel");

  for (i = 0; i < sizeof offsets / sizeof offsets[0]; i++)
    {
      for (j = 0; j < sizeof data; j++)
        data[j] = (unsigned char) (j * 7 + i + 1);
      if (PWRITE (fd, data, sizeof data, (IO_OFFSET) offsets[i]) != sizeof data)
        error (EXIT_FAILURE, errno, "%s at %lld", STRINGIFY (PWRITE), offsets[i]);
      if (lseek (fd, 0, SEEK_CUR) != 1000)
        error (EXIT_FAILURE, 0, "%s changed the file position", STRINGIFY (PWRITE));
      if (IO_FSTAT (fd, &st) != 0 || st.st_size != offsets[i] + sizeof data)
        error (EXIT_FAILURE, errno, "wrong file size after writing at %lld", offsets[i]);
    }

  /* Read both ranges after both writes, so aliasing between offsets cannot
     pass just because pread and pwrite make the same mistake. */
  for (i = 0; i < sizeof offsets / sizeof offsets[0]; i++)
    {
      for (j = 0; j < sizeof data; j++)
        data[j] = (unsigned char) (j * 7 + i + 1);
      memset (got, 0xa5, sizeof got);
      if (PREAD (fd, got, sizeof got, (IO_OFFSET) offsets[i]) != sizeof got
          || memcmp (data, got, sizeof got))
        error (EXIT_FAILURE, errno, "%s data mismatch at %lld", STRINGIFY (PREAD), offsets[i]);
      if (lseek (fd, 0, SEEK_CUR) != 1000)
        error (EXIT_FAILURE, 0, "%s changed the file position", STRINGIFY (PREAD));
    }
  if (PREAD (fd, got, sizeof got, 0) != sizeof got
      || memcmp (low, got, sizeof got))
    error (EXIT_FAILURE, 0, "large-offset writes changed the low-offset sentinel");
  if (lseek (fd, 0, SEEK_CUR) != 1000)
    error (EXIT_FAILURE, 0, "sentinel read changed the file position");
  return 0;
}


int
do_test (int argc, char *argv[])
{
  char buf[1000];
  char res[1000];
  int i;

  memset (buf, '\0', sizeof (buf));
  memset (res, '\xff', sizeof (res));

  if (write (fd, buf, sizeof (buf)) != sizeof (buf))
    error (EXIT_FAILURE, errno, "during write");

  for (i = 100; i < 200; ++i)
    buf[i] = i;
  if (PWRITE (fd, buf + 100, 100, 100) != 100)
    error (EXIT_FAILURE, errno, "during %s", STRINGIFY (PWRITE));
  if (lseek (fd, 0, SEEK_CUR) != sizeof buf)
    error (EXIT_FAILURE, 0, "%s changed the file position", STRINGIFY (PWRITE));

  for (i = 450; i < 600; ++i)
    buf[i] = i;
  if (PWRITE (fd, buf + 450, 150, 450) != 150)
    error (EXIT_FAILURE, errno, "during %s", STRINGIFY (PWRITE));
  if (lseek (fd, 0, SEEK_CUR) != sizeof buf)
    error (EXIT_FAILURE, 0, "%s changed the file position", STRINGIFY (PWRITE));

  if (PREAD (fd, res, sizeof (buf) - 50, 50) != sizeof (buf) - 50)
    error (EXIT_FAILURE, errno, "during %s", STRINGIFY (PREAD));
  if (lseek (fd, 0, SEEK_CUR) != sizeof buf)
    error (EXIT_FAILURE, 0, "%s changed the file position", STRINGIFY (PREAD));
  if (memcmp (buf + 50, res, sizeof (buf) - 50))
    error (EXIT_FAILURE, 0, "%s returned different data", STRINGIFY (PREAD));

  check_large_offsets ();

  close (fd);
  unlink (name);

  return 0;
}
