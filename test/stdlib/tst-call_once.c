/* call_once from <stdlib.h> (ISO C23), without <threads.h>, so it also
   runs where there are no threads.  Built with only _ISOC23_SOURCE (see
   Makefile.in).  The threaded case is nptl/tst-call-once.  */

#include <stdio.h>
#include <stdlib.h>

static int calls;

static void once(void)
{
	calls++;
}

int main(void)
{
	static once_flag flag = ONCE_FLAG_INIT;
	int i;

	for (i = 0; i < 3; i++)
		call_once(&flag, once);
	if (calls != 1) {
		printf("the function ran %d times, expected once\n", calls);
		return 1;
	}
	return 0;
}
