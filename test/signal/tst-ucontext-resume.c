/* A signal handler that changes uc_mcontext changes where the program
   goes on.

   Runtimes do this to step over a faulting instruction or to divert a
   thread into their own code.  The handler of a trap points the context
   at the entry of landing, sets the first argument register and returns;
   the program must then go on in landing with that argument.  */

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include <unistd.h>

#define ARG_VALUE    0x5a5a1234UL
#define RESUME_VALUE 0x600dcafeUL

#include "tst-ucontext-regs.h"

static sigjmp_buf jb;
static volatile int trapped, landed;
static volatile unsigned long landed_arg;

static unsigned long
code_addr (void (*fn) (unsigned long))
{
  unsigned long a = (unsigned long) fn;

#if defined __ia64__ || defined __FDPIC__ || defined __FRV_FDPIC__ \
    || defined __BFIN_FDPIC__
  a = *(unsigned long *) a;		/* function descriptor */
#elif defined __hppa__
  if (a & 2)
    a = *(unsigned long *) (a & ~3UL);	/* plabel */
#endif
  return a;
}

/* Entered from the signal frame, not called: it must not return.  */
static void __attribute__ ((noinline, noreturn))
landing (unsigned long arg)
{
  landed = 1;
  landed_arg = arg;
  siglongjmp (jb, 1);
}

static void
handler (int sig, siginfo_t *si, void *ctx)
{
  ucontext_t *uc = ctx;

  (void) si;
  if (sig == SIGABRT)
    /* gcc has no trap instruction here and called abort.  */
    siglongjmp (jb, 2);
  if (trapped)
    {
      /* The trap again: the context change did not take, and returning
	 would trap forever.  */
      static const char msg[] = "FAIL: the handler's context change did not take effect\n";
      write (1, msg, sizeof msg - 1);
      _exit (1);
    }
  trapped = 1;
#ifdef UC_ENTER
  UC_ENTER (uc, code_addr (landing));
# if defined UC_ENTER_ARG0
  UC_ENTER_ARG0 (uc) = RESUME_VALUE;
# elif defined UC_ARG0
  UC_ARG0 (uc) = RESUME_VALUE;
# endif
#else
  (void) uc;
  siglongjmp (jb, 1);
#endif
}

int
main (void)
{
  static const int sigs[] = { SIGILL, SIGTRAP, SIGSEGV, SIGBUS, SIGFPE, SIGABRT };
  void (*volatile fn) (unsigned long) = trap_fn;
  struct sigaction sa;
  unsigned int i;
  int ret = 0;

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

  switch (sigsetjmp (jb, 1))
    {
    case 0:
      fn (ARG_VALUE);
      puts ("FAIL: the trap returned to trap_fn's caller");
      return 1;
    case 2:
      puts ("SKIP: __builtin_trap is abort here");
      return 0;
    }

#ifndef UC_ENTER
  puts ("FAIL: <sys/ucontext.h> names no PC to change");
  return 1;
#endif

  if (!landed)
    {
      puts ("FAIL: the handler's context change did not take effect");
      return 1;
    }
#ifdef UC_ARG0
  if (landed_arg != RESUME_VALUE)
    {
      printf ("FAIL: landing got %#lx, expected %#lx\n",
	      landed_arg, RESUME_VALUE);
      ret = 1;
    }
#else
  puts ("first argument is passed on the stack, not checked");
#endif
  if (ret == 0)
    puts ("resumed in landing");
  return ret;
}
