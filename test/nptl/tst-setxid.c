/* setuid and its relatives change the IDs of the whole process (POSIX),
   but Linux keeps them per thread: the C library has to apply each call
   to every thread, as glibc does with its setxid broadcast.  Like glibc's
   tst-setuid1, this changes the IDs in the main thread and has the other
   threads read their own after every step.

   The steps keep root's saved user ID until the last two, so that each
   call is allowed.  Without root it says SKIP.  */

#define _GNU_SOURCE
#include <grp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NTHREADS 3

static pthread_barrier_t b1, b2;
static volatile int done;

struct ids {
	uid_t ru, eu, su;
	gid_t rg, eg, sg;
	int ngroups;
	gid_t group;
};

static void get_ids(struct ids *p)
{
	gid_t g[4];

	memset(p, 0, sizeof *p);
	getresuid(&p->ru, &p->eu, &p->su);
	getresgid(&p->rg, &p->eg, &p->sg);
	p->ngroups = getgroups(4, g);
	p->group = p->ngroups > 0 ? g[0] : (gid_t) -1;
}

static struct ids want;
static int failed;

static void *thread(void *arg)
{
	long n = (long) arg;

	for (;;) {
		struct ids have;

		pthread_barrier_wait(&b1);
		if (done)
			return NULL;
		get_ids(&have);
		if (memcmp(&have, &want, sizeof have) != 0) {
			printf("thread %ld: uid %d/%d/%d gid %d/%d/%d groups %d:%d,"
			       " main thread has uid %d/%d/%d gid %d/%d/%d groups %d:%d\n",
			       n, (int) have.ru, (int) have.eu, (int) have.su,
			       (int) have.rg, (int) have.eg, (int) have.sg,
			       have.ngroups, (int) have.group,
			       (int) want.ru, (int) want.eu, (int) want.su,
			       (int) want.rg, (int) want.eg, (int) want.sg,
			       want.ngroups, (int) want.group);
			failed = 1;
		}
		pthread_barrier_wait(&b2);
	}
}

/* Make the change in the main thread, then let the others compare.  */
static int step(const char *what, int ret)
{
	int bad = 0;

	if (ret != 0) {
		printf("%s failed: %m\n", what);
		bad = 1;
	}
	get_ids(&want);
	failed = 0;
	pthread_barrier_wait(&b1);
	pthread_barrier_wait(&b2);
	if (failed) {
		printf("  after %s\n", what);
		bad = 1;
	}
	return bad;
}

int main(void)
{
	pthread_t t[NTHREADS];
	gid_t g = 1005;
	long i;
	int result = 0;

	if (geteuid() != 0) {
		puts("SKIP: needs root");
		return 0;
	}
	pthread_barrier_init(&b1, NULL, NTHREADS + 1);
	pthread_barrier_init(&b2, NULL, NTHREADS + 1);
	for (i = 0; i < NTHREADS; ++i)
		if (pthread_create(&t[i], NULL, thread, (void *) i) != 0) {
			puts("pthread_create failed");
			return 1;
		}

#define STEP(call) result |= step(#call, call)
	STEP(setgroups(1, &g));
	STEP(setgid(1001));
	STEP(setegid(1002));
	STEP(setregid(1003, 1004));
	STEP(setresgid(1006, 1007, 1008));
	STEP(seteuid(2001));
	STEP(seteuid(0));
	STEP(setreuid(2002, 2003));
	STEP(setresuid(2003, 2002, 2002));
	STEP(setuid(2002));

	done = 1;
	pthread_barrier_wait(&b1);
	for (i = 0; i < NTHREADS; ++i)
		pthread_join(t[i], NULL);
	return result;
}
