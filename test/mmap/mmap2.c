/* mmap() with an offset that does not fit a 32-bit argument.
 *
 * The point of this test is the offset, not any particular file: on 32-bit
 * targets an offset near 4 GiB can only be expressed through the mmap2
 * syscall, which takes it in pages, so this exercises the conversion in the
 * C library.  It used to map the fixed physical address 0xfffff000 out of
 * /dev/mem, which is not a property of the library at all -- whether
 * anything mappable lives there depends on the platform, and the test was
 * switched off for arm, riscv32 and xtensa one architecture at a time.
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */
#define _FILE_OFFSET_BITS 64
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>

#define FATAL do { fprintf(stderr, "Error at line %d, file %s (%d) [%s]\n", \
  __LINE__, __FILE__, errno, strerror(errno)); exit(1); } while(0)

int main(void)
{
	char name[] = "mmap2.XXXXXX";
	long page = sysconf(_SC_PAGESIZE);
	unsigned char *map;
	off_t off;
	long i;
	int fd;

	/* The largest offset this off_t can express, page aligned and with room
	   for one page: beyond 32 bits where off_t is 64 bits wide, and just
	   below 2 GiB where it is not -- the conversion to pages is what is
	   being tested, and it happens either way.  */
	if (sizeof(off_t) >= 8)
		off = (off_t) 0xfffff000UL;
	else
		off = (off_t) 0x7fffe000L;

	if (off % page) {
		printf("offset not page aligned for a page size of %ld\n", page);
		return 23;	/* SKIP */
	}

	fd = mkstemp(name);
	if (fd == -1)
		FATAL;
	unlink(name);

	/* A hole, so nothing is actually allocated.  */
	if (ftruncate(fd, off + page) == -1) {
		if (errno == EFBIG || errno == EINVAL || errno == ENOSPC
		    || errno == EPERM) {
			printf("cannot size a file to %llu bytes: %s\n",
			       (unsigned long long) off + page, strerror(errno));
			close(fd);
			return 23;	/* SKIP */
		}
		FATAL;
	}

	map = mmap(NULL, page, PROT_READ, MAP_PRIVATE, fd, off);
	if (map == MAP_FAILED)
		FATAL;
	printf("mapped %ld bytes at offset %llu to %p\n",
	       page, (unsigned long long) off, map);

	/* Reading a hole gives zeroes; anything else means the offset was not
	   passed through correctly.  */
	for (i = 0; i < page; i++)
		if (map[i] != 0) {
			printf("byte %ld of the hole is %02x, expected 00\n",
			       i, map[i]);
			munmap(map, page);
			close(fd);
			return 1;
		}

	if (munmap(map, page) == -1)
		FATAL;
	close(fd);
	return 0;
}
