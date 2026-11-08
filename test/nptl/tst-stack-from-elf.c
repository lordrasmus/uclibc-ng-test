/* With PTHREADS_STACK_SIZE_FROM_ELF, a program linked with
   -z stack-size=SIZE (here 384 KiB, see Makefile.in) gets threads with a
   default stack of that size; without the option, the size is ignored.  */
#define _GNU_SOURCE
#include <link.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/auxv.h>

#define LINKED	(384 * 1024)
/* The descriptor, static TLS and a guard come on top of the size, or out
   of it for the default without an MMU.  */
#define SLACK	(64 * 1024)

static void *report(void *arg)
{
	pthread_attr_t a;
	size_t *size = arg;

	if (pthread_getattr_np(pthread_self(), &a) != 0)
		return (void *) 1;
	pthread_attr_getstacksize(&a, size);
	pthread_attr_destroy(&a);
	return NULL;
}

int main(void)
{
	const ElfW(Phdr) *ph = (const ElfW(Phdr) *) getauxval(AT_PHDR);
	unsigned long n = getauxval(AT_PHNUM);
	size_t linked = 0, size = 0;
	pthread_t t;
	void *res;
	int ret;

	for (; ph != NULL && n > 0; ph++, n--)
		if (ph->p_type == PT_GNU_STACK)
			linked = ph->p_memsz;
	if (linked != LINKED) {
		/* FLAT has no program headers, some linkers ignore -z stack-size. */
		printf("PT_GNU_STACK says %zu, not %d: skipped\n", linked, LINKED);
		return 23;
	}

	ret = pthread_create(&t, NULL, report, &size);
	if (ret != 0) {
		printf("pthread_create: %s\n", strerror(ret));
		return 1;
	}
	pthread_join(t, &res);
	if (res != NULL) {
		puts("pthread_getattr_np failed");
		return 1;
	}
	printf("default thread stack %zu\n", size);

#if defined __UCLIBC__ && defined __PTHREADS_STACK_SIZE_FROM_ELF__
	if (size + SLACK < LINKED || size > LINKED + SLACK) {
		printf("want about %d from PT_GNU_STACK\n", LINKED);
		return 1;
	}
#else
	/* The option is off (or another libc): the linked size must not be
	   what decides, unless the default happens to be the same.  */
	if (size + SLACK >= LINKED && size <= LINKED + SLACK)
		puts("default is about the linked size anyway");
#endif
	return 0;
}
