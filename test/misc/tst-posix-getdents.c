/* posix_getdents, POSIX.1-2024.

   Reads a directory the test makes itself, so it knows what has to come
   back: one regular file, one directory, one symlink and the two dots.

   DT_FORCE_TYPE is not in the standard -- POSIX only encourages it in the
   rationale for posix_getdents -- so it is checked as what it is: a best
   effort to fill in d_type where the file system left DT_UNKNOWN.  What
   the standard does say is that flags a strictly conforming program may
   pass is zero, and that other values are implementation-defined; an
   unknown one has to be refused rather than ignored.

   Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.  */

#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* DT_FORCE_TYPE comes with posix_getdents and struct posix_dent, so it
   says whether this libc has them.  glibc does not, yet.  */
#if !defined DT_FORCE_TYPE \
    || (defined __UCLIBC__ && !defined __UCLIBC_HAS_LFS__)

int main(void)
{
# if !defined DT_FORCE_TYPE
	printf("SKIP: no posix_getdents in this libc\n");
# else
	printf("SKIP: UCLIBC_HAS_LFS is off\n");
# endif
	return 0;
}

#else

static int fails;
static char dir[] = "/tmp/tst-posix-getdents.XXXXXX";

static void
cleanup(void)
{
	char p[sizeof(dir) + 16];

	snprintf(p, sizeof(p), "%s/file", dir);
	unlink(p);
	snprintf(p, sizeof(p), "%s/link", dir);
	unlink(p);
	snprintf(p, sizeof(p), "%s/sub", dir);
	rmdir(p);
	rmdir(dir);
}

/* Walk the buffer and report what was found.  n_* count the entries whose
   d_type says so; unknown counts the ones left DT_UNKNOWN.  */
static void
walk(void *buf, ssize_t n, int *n_reg, int *n_dir, int *n_lnk, int *unknown,
     int *entries)
{
	ssize_t i;

	for (i = 0; i < n; ) {
		struct posix_dent *d = (struct posix_dent *) ((char *) buf + i);

		if (d->d_reclen == 0) {
			printf("FAIL: d_reclen 0 at offset %zd\n", i);
			fails++;
			return;
		}
		++*entries;
		switch (d->d_type) {
		case DT_REG:	 ++*n_reg; break;
		case DT_DIR:	 ++*n_dir; break;
		case DT_LNK:	 ++*n_lnk; break;
		case DT_UNKNOWN: ++*unknown; break;
		default: break;
		}
		i += d->d_reclen;
	}
	if (i != n) {
		printf("FAIL: entries end at %zd, read %zd\n", i, n);
		fails++;
	}
}

static ssize_t
read_all(int fd, char *buf, size_t len, int flags, int *n_reg, int *n_dir,
	 int *n_lnk, int *unknown, int *entries)
{
	ssize_t n, total = 0;

	if (lseek(fd, 0, SEEK_SET) == (off_t) -1) {
		printf("FAIL: rewind: %s\n", strerror(errno));
		fails++;
		return -1;
	}
	while ((n = posix_getdents(fd, buf, len, flags)) > 0) {
		walk(buf, n, n_reg, n_dir, n_lnk, unknown, entries);
		total += n;
	}
	if (n < 0) {
		printf("FAIL: posix_getdents: %s\n", strerror(errno));
		fails++;
		return -1;
	}
	return total;
}

int main(void)
{
	char buf[8192], p[sizeof(dir) + 16];
	int fd, reg, dr, lnk, unknown, entries;
	ssize_t n;

	if (mkdtemp(dir) == NULL) {
		printf("SKIP: mkdtemp: %s\n", strerror(errno));
		return 0;
	}
	atexit(cleanup);

	snprintf(p, sizeof(p), "%s/file", dir);
	fd = open(p, O_WRONLY | O_CREAT | O_EXCL, 0600);
	if (fd < 0) {
		printf("FAIL: create %s: %s\n", p, strerror(errno));
		return 1;
	}
	close(fd);
	snprintf(p, sizeof(p), "%s/sub", dir);
	if (mkdir(p, 0700) != 0) {
		printf("FAIL: mkdir %s: %s\n", p, strerror(errno));
		return 1;
	}
	snprintf(p, sizeof(p), "%s/link", dir);
	if (symlink("file", p) != 0) {
		printf("FAIL: symlink %s: %s\n", p, strerror(errno));
		return 1;
	}

	fd = open(dir, O_RDONLY | O_DIRECTORY);
	if (fd < 0) {
		printf("FAIL: open %s: %s\n", dir, strerror(errno));
		return 1;
	}

	/* flags 0: what a strictly conforming program passes.  */
	reg = dr = lnk = unknown = entries = 0;
	n = read_all(fd, buf, sizeof(buf), 0, &reg, &dr, &lnk, &unknown,
		     &entries);
	if (n > 0) {
		/* ".", "..", file, sub, link */
		if (entries != 5) {
			printf("FAIL: flags 0: %d entries, expected 5\n",
			       entries);
			fails++;
		} else {
			printf("PASS: flags 0: 5 entries, %d reg %d dir "
			       "%d lnk %d unknown\n", reg, dr, lnk, unknown);
		}
	}

	/* With DT_FORCE_TYPE nothing may stay DT_UNKNOWN, and the types
	   have to be the ones the files were made with.  */
	reg = dr = lnk = unknown = entries = 0;
	n = read_all(fd, buf, sizeof(buf), DT_FORCE_TYPE, &reg, &dr, &lnk,
		     &unknown, &entries);
	if (n > 0) {
		if (unknown != 0) {
			printf("FAIL: DT_FORCE_TYPE left %d entries "
			       "DT_UNKNOWN\n", unknown);
			fails++;
		} else if (reg != 1 || dr != 3 || lnk != 1) {
			printf("FAIL: DT_FORCE_TYPE: %d reg %d dir %d lnk, "
			       "expected 1/3/1\n", reg, dr, lnk);
			fails++;
		} else {
			printf("PASS: DT_FORCE_TYPE: 1 reg, 3 dir, 1 lnk\n");
		}
	}

	/* An unknown flag has to be refused, not ignored.  */
	errno = 0;
	n = posix_getdents(fd, buf, sizeof(buf), 0x4000);
	if (n != -1 || errno != EINVAL) {
		printf("FAIL: unknown flag: returned %zd errno %d (%s), "
		       "expected -1/EINVAL\n", n, errno, strerror(errno));
		fails++;
	} else {
		printf("PASS: unknown flag -> EINVAL\n");
	}

	/* A buffer too small for one entry is EINVAL.  */
	errno = 0;
	if (lseek(fd, 0, SEEK_SET) != (off_t) -1) {
		n = posix_getdents(fd, buf, 1, 0);
		if (n != -1 || errno != EINVAL) {
			printf("FAIL: 1-byte buffer: returned %zd errno %d "
			       "(%s), expected -1/EINVAL\n",
			       n, errno, strerror(errno));
			fails++;
		} else {
			printf("PASS: 1-byte buffer -> EINVAL\n");
		}
	}

	close(fd);

	/* A descriptor that is not a directory.  */
	snprintf(p, sizeof(p), "%s/file", dir);
	fd = open(p, O_RDONLY);
	if (fd >= 0) {
		errno = 0;
		n = posix_getdents(fd, buf, sizeof(buf), 0);
		if (n != -1 || errno != ENOTDIR) {
			printf("FAIL: plain file: returned %zd errno %d "
			       "(%s), expected -1/ENOTDIR\n",
			       n, errno, strerror(errno));
			fails++;
		} else {
			printf("PASS: plain file -> ENOTDIR\n");
		}
		close(fd);
	}

	if (fails) {
		printf("%d failure(s)\n", fails);
		return 1;
	}
	printf("all ok\n");
	return 0;
}

#endif
