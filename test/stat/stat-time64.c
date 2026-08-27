/* Test for stat() after 2038 year.  */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#define assert(x) \
  if (!(x)) \
    { \
      fputs ("test failed: " #x "\n", stderr); \
      retval = 1; \
      goto the_end; \
    }

int
main (int argc, char *argv[])
{
#if defined(__arc__) || defined(__arm__) || defined(__i386__) || defined(__microblaze__) || defined(__mips__) || defined(__or1k__) || defined(__powerpc__) || defined(__sh__) || defined(__sparc__) || defined(__xtensa__)
  char name[PATH_MAX];
  int retval = 0;
  int fd;
  struct stat st;
  struct timespec ts_init, ts_final;
  int clock_moved = 0;

  if (sizeof(time_t) == 8) {

    memset(name, 0, PATH_MAX);

    struct timespec y2090_ts = {
      .tv_sec = 3786962400,
      .tv_nsec = 0
    };

    assert(clock_gettime (CLOCK_REALTIME, &ts_init) == 0);
    assert(clock_settime (CLOCK_REALTIME, &y2090_ts) == 0);
    clock_moved = 1;

    /* hack to get a tempfile name w/out using tmpname()
     * as that func causes a link time warning */
    sprintf(name, "%s-uClibc-test.XXXXXX", __FILE__);
    fd = mkstemp(name);

    assert(stat (name, &st) == 0)
    assert(st.st_atime >= 3786962400);
    assert(st.st_mtime >= 3786962400);
    assert(st.st_ctime >= 3786962400);

    assert(fstat (fd, &st) == 0)
    assert(st.st_atime >= 3786962400);
    assert(st.st_mtime >= 3786962400);
    assert(st.st_ctime >= 3786962400);

    assert(lstat (name, &st) == 0)
    assert(st.st_atime >= 3786962400);
    assert(st.st_mtime >= 3786962400);
    assert(st.st_ctime >= 3786962400);

    close(fd);
    retval = 0;

the_end:
    /* Put the clock back.  Every test that runs after this one in the same
       boot saw the year 2090 otherwise, and the ones that measure a duration
       reported nonsense -- the runner timed this test at 48 hours. */
    if (clock_moved && clock_gettime (CLOCK_REALTIME, &ts_final) == 0) {
      ts_init.tv_sec += ts_final.tv_sec - y2090_ts.tv_sec;
      ts_init.tv_nsec = ts_final.tv_nsec;
      clock_settime (CLOCK_REALTIME, &ts_init);
    }
    unlink (name);

    return retval;
  }
  else {
    printf("Nothing to test.\n");
  }
#else
  printf("Nothing to test.\n");
  return 0;
#endif
}
