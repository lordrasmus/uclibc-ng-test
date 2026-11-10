/* With LDSO_RUNPATH_OF_EXECUTABLE the loader also searches the
   executable's RPATH and RUNPATH for the dependencies of libraries that
   have no paths of their own.  $ORIGIN there is the executable's
   directory.  The program dlopens LIB1 from its own path (DIR next to it);
   LIB1 needs LIB2 from the same directory but carries no path, so LIB2 can
   only come from that fallback.  Neither LD_LIBRARY_PATH="$PWD" nor the
   default paths reach DIR.  */
#include <dlfcn.h>
#include <stdio.h>

int main(void)
{
	void *h;
	int (*fb1)(void);

#if !defined __UCLIBC__ || !defined __LDSO_RUNPATH_OF_EXECUTABLE__
	puts("SKIP: no LDSO_RUNPATH_OF_EXECUTABLE");
	return 0;
#endif
	h = dlopen(LIB1, RTLD_NOW);
	if (!h) {
		printf("dlopen %s: %s\n", LIB1, dlerror());
		return 1;
	}
	fb1 = (int (*)(void)) dlsym(h, "fb1");
	if (!fb1) {
		printf("dlsym fb1: %s\n", dlerror());
		return 1;
	}
	if (fb1() != 42) {
		printf("fb1() = %d, want 42\n", fb1());
		return 1;
	}
	dlclose(h);
	return 0;
}
