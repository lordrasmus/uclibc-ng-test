/* posix_typed_mem_open, posix_typed_mem_get_info and posix_mem_offset.

   Linux has no typed memory objects, so all three only report that.  What
   this checks is that they report it the way the standard says, because
   the errors are the whole interface here.

   The tflag rules are the easy ones to get wrong.  POSIX says "At most,
   one of the following flags ... may be specified", and names EINVAL for
   "more than one" only -- so no flag at all is a valid request and has to
   be answered like a missing object, not like a bad argument.

   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.  */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

/* The flags come with the declarations, so they say whether this libc
   offers the option group at all.  glibc does not have these functions.  */
#if !defined POSIX_TYPED_MEM_ALLOCATE \
    || (defined __UCLIBC__ && !defined __UCLIBC_HAS_ADVANCED_REALTIME__)

int main(void)
{
# if !defined POSIX_TYPED_MEM_ALLOCATE
	printf("SKIP: no posix_typed_mem_* in this libc\n");
# else
	printf("SKIP: UCLIBC_HAS_ADVANCED_REALTIME is off\n");
# endif
	return 0;
}

#else

static int fails;

static void
check_open(const char *what, int tflag, int want)
{
	int r;

	errno = 0;
	r = posix_typed_mem_open("/no-such-typed-memory-object", O_RDONLY,
				 tflag);
	if (r != -1) {
		printf("FAIL: %s: returned %d, expected -1\n", what, r);
		fails++;
		close(r);
		return;
	}
	if (errno != want) {
		printf("FAIL: %s: errno %d (%s), expected %d (%s)\n",
		       what, errno, strerror(errno), want, strerror(want));
		fails++;
		return;
	}
	printf("PASS: %s -> %s\n", what, strerror(want));
}

int main(void)
{
	struct posix_typed_mem_info info;
	size_t contig;
	off_t off;
	int fd, r;
	char c;

	/* A libc that has the option says so in one of two ways, and a
	   program has to accept either: the macro, or sysconf when the
	   macro is not defined.  uClibc-ng answers neither way today --
	   <unistd.h> undefines _POSIX_TYPED_MEMORY_OBJECTS and sysconf
	   has no case for it -- so only a positive answer counts here.  */
#ifdef _POSIX_TYPED_MEMORY_OBJECTS
	if (_POSIX_TYPED_MEMORY_OBJECTS > 0) {
		printf("SKIP: this system has typed memory objects\n");
		return 0;
	}
#endif
	if (sysconf(_SC_TYPED_MEMORY_OBJECTS) > 0) {
		printf("SKIP: this system has typed memory objects\n");
		return 0;
	}

	/* No flag at all: allowed, so the object is simply not there.  */
	check_open("tflag 0", 0, ENOENT);

	check_open("POSIX_TYPED_MEM_ALLOCATE",
		   POSIX_TYPED_MEM_ALLOCATE, ENOENT);
	check_open("POSIX_TYPED_MEM_ALLOCATE_CONTIG",
		   POSIX_TYPED_MEM_ALLOCATE_CONTIG, ENOENT);
	check_open("POSIX_TYPED_MEM_MAP_ALLOCATABLE",
		   POSIX_TYPED_MEM_MAP_ALLOCATABLE, ENOENT);

	/* More than one is the case POSIX gives EINVAL for.  */
	check_open("ALLOCATE|ALLOCATE_CONTIG",
		   POSIX_TYPED_MEM_ALLOCATE | POSIX_TYPED_MEM_ALLOCATE_CONTIG,
		   EINVAL);
	check_open("all three",
		   POSIX_TYPED_MEM_ALLOCATE | POSIX_TYPED_MEM_ALLOCATE_CONTIG
		   | POSIX_TYPED_MEM_MAP_ALLOCATABLE, EINVAL);

	/* Not covered by the standard; a bad argument is the sane answer,
	   and it must not be mistaken for one of the valid flags.  */
	check_open("unknown bit", 0x40, EINVAL);
	check_open("INT_MIN", (-2147483647 - 1), EINVAL);

	/* get_info wants a descriptor of a typed memory object.  An
	   ordinary open one is not, and a closed number is EBADF.  */
	fd = open("/dev/null", O_RDONLY);
	if (fd < 0) {
		printf("FAIL: open /dev/null: %s\n", strerror(errno));
		return 1;
	}
	memset(&info, 0xa5, sizeof(info));
	r = posix_typed_mem_get_info(fd, &info);
	if (r == 0) {
		printf("FAIL: get_info on /dev/null succeeded\n");
		fails++;
	} else {
		printf("PASS: get_info on a plain fd -> %s\n", strerror(r));
	}
	close(fd);

	r = posix_typed_mem_get_info(fd, &info);
	if (r != EBADF) {
		printf("FAIL: get_info on a closed fd: %d (%s), expected "
		       "EBADF\n", r, strerror(r));
		fails++;
	} else {
		printf("PASS: get_info on a closed fd -> EBADF\n");
	}

	/* posix_mem_offset asks which typed memory object an address came
	   from.  The stack did not come from one.  */
	off = 0;
	contig = 0;
	fd = -1;
	r = posix_mem_offset(&c, sizeof(c), &off, &contig, &fd);
	if (r == 0) {
		printf("FAIL: posix_mem_offset on the stack succeeded\n");
		fails++;
	} else {
		printf("PASS: posix_mem_offset -> %s\n", strerror(r));
	}

	if (fails) {
		printf("%d failure(s)\n", fails);
		return 1;
	}
	printf("all ok\n");
	return 0;
}

#endif
