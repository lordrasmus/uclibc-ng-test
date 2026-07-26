/* vi: set sw=4 ts=4 sts=4: */
/*
 * timerfd test for uClibc
 * Copyright (C) 2012 by Kevin Cernekee <cernekee@gmail.com>
 *
 * Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <signal.h>
#include <stdint.h>
#include <inttypes.h>
#include <time.h>
#include <sys/timerfd.h>
#include <sys/fcntl.h>
#include <poll.h>

static int
do_test(void)
{
	int fd, ret, result = 0;
	struct itimerspec s;
	struct pollfd pfd;
	uint64_t val = 0;

	fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK);
	if (fd < 0) {
		perror("timerfd() failed");
		/* nothing below works without the descriptor */
		return 1;
	}
	s.it_value.tv_sec = 1;
	s.it_value.tv_nsec = 0;
	s.it_interval.tv_sec = 0;
	s.it_interval.tv_nsec = 0;
	if (timerfd_settime(fd, 0, &s, NULL) < 0) {
		perror("timerfd_settime() failed");
		return 1;
	}

	/* this should return immediately with EAGAIN due to TFD_NONBLOCK */
	ret = read(fd, &val, sizeof(val));
	if (ret != -1 || errno != EAGAIN) {
		error(0, 0, "first read() returned %d", ret);
		result = 1;
	}

	/* Wait for the expiration itself instead of for a wall-clock reading:
	   the timer runs on CLOCK_MONOTONIC, while time() is CLOCK_REALTIME with
	   one-second granularity -- and this test suite steps the realtime clock
	   (the sysvipc tests set it past 2038).  poll() sleeps until the timer
	   fires, so there is no margin to lose on a slow machine.  */
	pfd.fd = fd;
	pfd.events = POLLIN;
	ret = poll(&pfd, 1, 3000);
	if (ret <= 0) {
		error(0, 0, "timer did not fire within 3s: poll() returned %d", ret);
		return 1;
	}

	ret = read(fd, &val, sizeof(val));
	if (ret != sizeof(val)) {
		error(0, 0, "second read() returned %d", ret);
		return 1;
	}

	/* we are expecting a single expiration, since it_interval is 0 */
	if (val != 1) {
		error(0, 0, "wrong number of expirations: %" PRIx64, val);
		result = 1;
	}

	return result;
}

/* The poll() above may wait 3s, and on noMMU the skeleton re-execs the test
   (vfork+execve, there is no fork), so the startup cost is paid twice.  */
#define TIMEOUT 10
#define TEST_FUNCTION do_test ()
#include "../test-skeleton.c"
