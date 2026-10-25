/* longjmp out of a region with a buffer on the thread's old-style
   cleanup chain (_pthread_cleanup_push, which the C library also uses
   around its internal locks) runs the handlers it jumps over and takes
   them off the chain, as glibc does through __pthread_cleanup_upto.
   Statically linked, uClibc-ng used to leave both undone: the handler
   did not run, and pthread_exit later found the buffer of a dead stack
   frame on the chain.  */

#include <pthread.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

/* The old API, declared here as in tst-cleanup4.c.  */
extern void _pthread_cleanup_push (struct _pthread_cleanup_buffer *__buffer,
				   void (*__routine) (void *), void *__arg);
extern void _pthread_cleanup_pop (struct _pthread_cleanup_buffer *__buffer,
				  int __execute);

static jmp_buf jb;
static volatile int ran;

static void handler(void *arg)
{
	(void) arg;
	++ran;
}

static void __attribute__ ((noinline)) jump_out(void)
{
	struct _pthread_cleanup_buffer b;

	_pthread_cleanup_push(&b, handler, NULL);
	longjmp(jb, 1);
	_pthread_cleanup_pop(&b, 0);
}

static void *thread(void *arg)
{
	if (setjmp(jb) == 0)
		jump_out();
	if (ran != 1) {
		printf("handler ran %d times during longjmp, expected 1\n", ran);
		exit(1);
	}
	pthread_exit(arg);
}

int main(void)
{
	pthread_t t;

	if (pthread_create(&t, NULL, thread, NULL) != 0) {
		puts("pthread_create failed");
		return 1;
	}
	if (pthread_join(t, NULL) != 0) {
		puts("pthread_join failed");
		return 1;
	}
	if (ran != 1) {
		printf("handler ran %d times in all, expected 1:"
		       " pthread_exit found it still on the chain\n", ran);
		return 1;
	}
	return 0;
}
