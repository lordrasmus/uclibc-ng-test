/* A DT_NEEDED library found only through $ORIGIN in the program's
   DT_RUNPATH: it lies in neededlib/ next to the program, which neither
   LD_LIBRARY_PATH nor the default paths cover.  Before the fix the loader
   skipped $ORIGIN in DT_RUNPATH and the program did not start.  */
#include <stdio.h>

int tstneeded(void);

int main(void)
{
	int r = tstneeded();

	if (r != 42) {
		printf("tstneeded() = %d, want 42\n", r);
		return 1;
	}
	return 0;
}
