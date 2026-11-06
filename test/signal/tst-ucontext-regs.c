/* The registers a signal handler finds in uc_mcontext.

   Garbage collectors, profilers and sanitizers read the interrupted
   context's registers from the ucontext_t of an SA_SIGINFO handler.  If
   the libc's mcontext_t does not match the frame the kernel writes, they
   read the wrong registers without any error.

   trap_fn traps with a known value in its first argument register.  The
   handler must then find a PC inside trap_fn, that value in the first
   argument register, and a stack pointer next to the caller's frame.  A
   trap instruction rather than a jump to an unmapped address, because
   without an MMU there is no unmapped address.  */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#define ARG_VALUE 0x5a5a1234UL

/* Per architecture: UC_PC, UC_SP and, where the first argument is passed
   in a register, UC_ARG0, each reading from a ucontext_t *.  */
#include "tst-ucontext-regs.h"

#ifndef PC_MASK
# define PC_MASK (~0UL)
#endif

/* trap_fn is a few instructions; the trap must lie within this many bytes
   of its start.  */
#define TRAP_FN_SIZE 256

static sigjmp_buf jb;
static volatile int got_sig;
static volatile unsigned long got_pc, got_sp, got_arg;

static void
handler (int sig, siginfo_t *si, void *ctx)
{
  ucontext_t *uc = ctx;

  (void) si;
  got_sig = sig;
  got_pc = (unsigned long) UC_PC (uc);
  got_sp = (unsigned long) UC_SP (uc);
#ifdef UC_ARG0
  got_arg = (unsigned long) UC_ARG0 (uc);
#endif
  siglongjmp (jb, 1);
}

/* The address of trap_fn's code.  */
static unsigned long
code_addr (void (*fn) (unsigned long))
{
  unsigned long a = (unsigned long) fn;

#if defined __ia64__ || defined __FDPIC__
  a = *(unsigned long *) a;		/* function descriptor */
#elif defined __hppa__
  if (a & 2)
    a = *(unsigned long *) (a & ~3UL);	/* plabel */
#elif defined __arm__
  a &= ~1UL;				/* Thumb bit */
#endif
  return a;
}

int
main (void)
{
  static const int sigs[] = { SIGILL, SIGTRAP, SIGSEGV, SIGBUS, SIGFPE, SIGABRT };
  void (*volatile fn) (unsigned long) = trap_fn;
  volatile char here = 0;
  struct sigaction sa;
  int ret = 0;
  unsigned int i;

  start_timeout ();
  memset (&sa, 0, sizeof sa);
  sa.sa_sigaction = handler;
  sa.sa_flags = SA_SIGINFO;
  sigemptyset (&sa.sa_mask);
  for (i = 0; i < sizeof sigs / sizeof sigs[0]; i++)
    if (sigaction (sigs[i], &sa, NULL) != 0)
      {
	perror ("sigaction");
	return 1;
      }

  if (sigsetjmp (jb, 1) == 0)
    {
      fn (ARG_VALUE);
      puts ("FAIL: __builtin_trap returned");
      return 1;
    }

  unsigned long start = code_addr (trap_fn);

  printf ("signal %d: pc %#lx (trap_fn %#lx) sp %#lx arg %#lx\n",
	  got_sig, got_pc, start, got_sp, got_arg);

  if (got_sig == SIGABRT)
    {
      /* gcc has no trap instruction for this target and calls abort;
	 the signal then comes from raise, not from trap_fn.  */
      puts ("SKIP: __builtin_trap is abort here");
      return 0;
    }

#ifdef UC_NO_PC
  puts ("FAIL: <sys/ucontext.h> names no PC");
  ret = 1;
#else
  unsigned long pc = got_pc & PC_MASK;
  if (pc < start || pc >= start + TRAP_FN_SIZE)
    {
      printf ("FAIL: pc %#lx is not in trap_fn (%#lx)\n", got_pc, start);
      ret = 1;
    }
#endif

  unsigned long frame = (unsigned long) &here;
  unsigned long dist = got_sp > frame ? got_sp - frame : frame - got_sp;
  if (dist > 64 * 1024)
    {
      printf ("FAIL: sp %#lx is %#lx away from the caller's frame %#lx\n",
	      got_sp, dist, frame);
      ret = 1;
    }

#ifdef UC_ARG0
  if (got_arg != ARG_VALUE)
    {
      printf ("FAIL: first argument register %#lx, expected %#lx\n",
	      got_arg, ARG_VALUE);
      ret = 1;
    }
#else
  puts ("first argument is passed on the stack, not checked");
#endif

  return ret;
}
