/* Copyright (C) 2009 Mikael Lund Jepsen <mlj@iccc.dk>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

/* A POSIX shared memory object, shared between two processes.  The second
 * one is the same program started with vfork and exec, so it runs without
 * an MMU too.  The parent writes the data, the child checks it and writes
 * back, the parent checks that.  */

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static const char shared_name[] = "/sharetest";
static const int test_data[11] = {0,1,2,3,4,5,6,7,8,9,10};
#define CHILD_MARK 4711

static int *map_object(int flags)
{
	int fd = shm_open(shared_name, flags, DEFFILEMODE);
	int *p;

	if (fd == -1) {
		perror("shm_open");
		return NULL;
	}
	if ((flags & O_CREAT) && ftruncate(fd, sizeof(test_data)) == -1) {
		perror("ftruncate");
		close(fd);
		return NULL;
	}
	p = mmap(NULL, sizeof(test_data), PROT_READ | PROT_WRITE, MAP_SHARED,
		 fd, 0);
	close(fd);
	if (p == MAP_FAILED) {
		perror("mmap");
		return NULL;
	}
	return p;
}

static int child(void)
{
	int *p = map_object(O_RDWR);
	unsigned int i;
	int err = 0;

	if (p == NULL)
		return 1;
	for (i = 0; i < ARRAY_SIZE(test_data); i++)
		if (p[i] != test_data[i]) {
			printf("child: element %u is %d, expected %d\n",
			       i, p[i], test_data[i]);
			err = 1;
		}
	p[0] = CHILD_MARK;
	munmap(p, sizeof(test_data));
	return err;
}

int main(int argc, char *argv[])
{
	int *p;
	pid_t pid;
	int status, err = 0;

	if (argc > 1 && strcmp(argv[1], "child") == 0)
		return child();

	shm_unlink(shared_name);
	p = map_object(O_RDWR | O_CREAT | O_EXCL);
	if (p == NULL)
		return 1;
	memcpy(p, test_data, sizeof(test_data));

	if (shm_open(shared_name, O_RDWR | O_CREAT | O_EXCL, DEFFILEMODE) != -1
	    || errno != EEXIST) {
		puts("shm_open(O_EXCL) on an existing object did not fail with EEXIST");
		err = 1;
	}

	pid = vfork();
	if (pid == 0) {
		execl(argv[0], argv[0], "child", (char *)NULL);
		_exit(127);
	}
	if (pid == -1) {
		perror("vfork");
		err = 1;
	} else if (waitpid(pid, &status, 0) != pid
		   || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		printf("child failed, status 0x%x\n", status);
		err = 1;
	} else if (p[0] != CHILD_MARK) {
		printf("child's write not seen: element 0 is %d\n", p[0]);
		err = 1;
	}

	munmap(p, sizeof(test_data));
	if (shm_unlink(shared_name) == -1) {
		perror("shm_unlink");
		err = 1;
	}
	if (shm_open(shared_name, O_RDWR, DEFFILEMODE) != -1 || errno != ENOENT) {
		puts("shm_open after shm_unlink did not fail with ENOENT");
		err = 1;
	}
	return err;
}
