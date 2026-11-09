/* How a program reads PC, SP and the first argument register of the
   interrupted context from the ucontext_t of an SA_SIGINFO handler,
   written against uClibc-ng's <sys/ucontext.h> as a program would.

   UC_ARG0 is left undefined where the first argument is passed on the
   stack (i386, m68k) or in a register the signal frame does not hold
   (ia64 stacked registers).  */

#define UC_M(uc) ((uc)->uc_mcontext)

/* A trap instruction.  Where gcc has none, __builtin_trap calls abort and
   the signal would come from raise; take the instruction gdb uses as its
   breakpoint there, where it raises a signal.  */
#if defined __sh__ && (defined __SH2__ || defined __SH2E__ || defined __SH2A__)
/* trapa #0xc3 raises no signal on SH-2; a reserved opcode does.  */
# define TRAP() __asm__ __volatile__ (".word 0xfffd")
#elif defined __sh__
# define TRAP() __asm__ __volatile__ ("trapa #0xc3")
#elif defined __H8300H__ || defined __H8300S__ || defined __H8300__
# define TRAP() __asm__ __volatile__ ("trapa #3")
#elif defined __or1k__
# define TRAP() __asm__ __volatile__ ("l.trap 1")
#elif defined __frv__
/* Not gdb's tira gr0,#1: Linux takes every user tira through the system
   call entry.  Writing the PSR is privileged and raises SIGILL.  */
# define TRAP() __asm__ __volatile__ ("movgs gr0,psr")
#elif defined __TMS320C6X__
# define TRAP() __asm__ __volatile__ (".word 0x56454314")
#elif defined __microblaze__
/* gcc's __builtin_trap is bri 0, a branch to itself.  gdb's brki r16, 0x18
   reaches the handler only as a fatal SIGTRAP; writing the MSR is
   privileged and raises SIGILL.  */
# define TRAP() __asm__ __volatile__ ("mts rmsr, r0")
#elif defined __nds32__
/* gcc's trap #0 is no debug trap to Linux, which kills the process for
   it; gdb's break16 0 raises SIGTRAP.  */
# define TRAP() __asm__ __volatile__ ("break16 0")
#elif defined __avr32__
# define TRAP() __asm__ __volatile__ ("breakpoint")
#elif defined __kvx__
# define TRAP() __asm__ __volatile__ (";;\n\terrop\n\t;;")
#elif defined __metag__
/* gcc 4.2's __builtin_trap is a call to abort.  The kernel raises SIGTRAP
   for its compiled-in breakpoint, __METAG_SW_PERM_BREAK.  */
# define TRAP() __asm__ __volatile__ ("SWITCH #0x400002")
#else
# define TRAP() __builtin_trap ()
#endif

/* A broken mcontext_t can make a test loop on its trap or hang; fail
   instead of using up the time of the whole suite.  */
static void
timeout (int sig)
{
  static const char msg[] = "FAIL: timed out\n";

  (void) sig;
  write (1, msg, sizeof msg - 1);
  _exit (1);
}

static void
start_timeout (void)
{
  signal (SIGALRM, timeout);
  alarm (20);
}

/* Traps as its first instruction, with its argument still in the first
   argument register.  */
static void __attribute__ ((noinline, optimize ("O2")))
trap_fn (unsigned long arg)
{
  (void) arg;
  TRAP ();
}

#if defined __aarch64__
# define UC_PC(uc)   UC_M (uc).pc
# define UC_SP(uc)   UC_M (uc).sp
# define UC_ARG0(uc) UC_M (uc).regs[0]
#elif defined __alpha__
# define UC_PC(uc)   UC_M (uc).sc_pc
# define UC_SP(uc)   UC_M (uc).sc_regs[30]
# define UC_ARG0(uc) UC_M (uc).sc_regs[16]
#elif defined __arc__
# define UC_PC(uc)   UC_M (uc).regs.scratch.ret
# define UC_SP(uc)   UC_M (uc).regs.scratch.sp
# define UC_ARG0(uc) UC_M (uc).regs.scratch.r0
#elif defined __arm__
# define UC_PC(uc)   UC_M (uc).arm_pc
# define UC_SP(uc)   UC_M (uc).arm_sp
# define UC_ARG0(uc) UC_M (uc).arm_r0
#elif defined __avr32__
# define UC_PC(uc)   UC_M (uc).pc
# define UC_SP(uc)   UC_M (uc).sp
# define UC_ARG0(uc) UC_M (uc).r12
#elif defined __bfin__
/* The REG_* names are hidden behind #if 0 in <sys/ucontext.h>.  */
# define UC_PC(uc)   UC_M (uc).gregs[21]
# define UC_SP(uc)   UC_M (uc).gregs[14]
# define UC_ARG0(uc) UC_M (uc).gregs[0]
#elif defined __TMS320C6X__
# define UC_PC(uc)   UC_M (uc).sc_pc
# define UC_SP(uc)   UC_M (uc).sc_sp
# define UC_ARG0(uc) UC_M (uc).sc_a4
#elif defined __CRIS__
# if defined __arch_v32
#  define UC_PC(uc)  UC_M (uc).regs.erp
# else
#  define UC_PC(uc)  UC_M (uc).regs.irp
# endif
# define UC_SP(uc)   UC_M (uc).usp
# define UC_ARG0(uc) UC_M (uc).regs.r10
#elif defined __csky__
# define UC_PC(uc)   UC_M (uc).sc_pt_regs.pc
# define UC_SP(uc)   UC_M (uc).sc_pt_regs.usp
# define UC_ARG0(uc) UC_M (uc).sc_pt_regs.a0
#elif defined __frv__
# define UC_PC(uc)   UC_M (uc).gregs[REG_PC]
# define UC_SP(uc)   UC_M (uc).gregs[REG_GR1]
# define UC_ARG0(uc) UC_M (uc).gregs[REG_GR8]
#elif defined __H8300H__ || defined __H8300S__ || defined __H8300__
# define UC_PC(uc)   UC_M (uc).sc_pc
# define UC_SP(uc)   UC_M (uc).sc_usp
# define UC_ARG0(uc) UC_M (uc).sc_er0
#elif defined __hppa__
/* The low two bits of the instruction address are the privilege level.  */
# define UC_PC(uc)   UC_M (uc).sc_iaoq[0]
# define UC_SP(uc)   UC_M (uc).sc_gr[30]
# define UC_ARG0(uc) UC_M (uc).sc_gr[26]
# define PC_MASK     (~3UL)
#elif defined __i386__
# define UC_PC(uc)   UC_M (uc).gregs[REG_EIP]
# define UC_SP(uc)   UC_M (uc).gregs[REG_ESP]
#elif defined __ia64__
/* The low two bits are the slot within the bundle.  */
# define UC_PC(uc)   UC_M (uc).sc_ip
# define UC_SP(uc)   UC_M (uc).sc_gr[12]
# define PC_MASK     (~3UL)
#elif defined __kvx__
# define UC_PC(uc)   UC_M (uc).sc_regs.spc
# define UC_SP(uc)   UC_M (uc).sc_regs.sp
# define UC_ARG0(uc) UC_M (uc).sc_regs.r0
#elif defined __lm32__
# define UC_PC(uc)   UC_M (uc).regs.ea
# define UC_SP(uc)   UC_M (uc).regs.sp
# define UC_ARG0(uc) UC_M (uc).regs.r1
#elif defined __loongarch__
# define UC_PC(uc)   UC_M (uc).__pc
# define UC_SP(uc)   UC_M (uc).__gregs[LARCH_REG_SP]
# define UC_ARG0(uc) UC_M (uc).__gregs[LARCH_REG_A0]
#elif defined __m68k__
# define UC_PC(uc)   UC_M (uc).gregs[R_PC]
# define UC_SP(uc)   UC_M (uc).gregs[R_SP]
#elif defined __metag__
# define UC_PC(uc)   UC_M (uc).regs.pc
# define UC_SP(uc)   UC_M (uc).regs.ax[0][0]
# define UC_ARG0(uc) UC_M (uc).regs.dx[3][1]
#elif defined __microblaze__
# define UC_PC(uc)   UC_M (uc).regs.pc
# define UC_SP(uc)   UC_M (uc).regs.r1
# define UC_ARG0(uc) UC_M (uc).regs.r5
#elif defined __mips__
# define UC_PC(uc)   UC_M (uc).pc
# define UC_SP(uc)   UC_M (uc).gregs[29]
# define UC_ARG0(uc) UC_M (uc).gregs[4]
#elif defined __nds32__
# define UC_PC(uc)   UC_M (uc).nds32_ipc
# define UC_SP(uc)   UC_M (uc).nds32_sp
# define UC_ARG0(uc) UC_M (uc).nds32_r0
#elif defined __nios2__
/* regs[i] is r(i+1): ea (the PC) is 27, sp 28, r4 3.  */
# define UC_PC(uc)   UC_M (uc).regs[27]
# define UC_SP(uc)   UC_M (uc).regs[28]
# define UC_ARG0(uc) UC_M (uc).regs[3]
#elif defined __or1k__
# define UC_PC(uc)   UC_M (uc).regs.pc
# define UC_SP(uc)   UC_M (uc).regs.gpr[1]
# define UC_ARG0(uc) UC_M (uc).regs.gpr[3]
#elif defined __powerpc__ && !defined __powerpc64__
# define UC_PC(uc)   UC_M (uc).uc_regs->gregs[PT_NIP]
# define UC_SP(uc)   UC_M (uc).uc_regs->gregs[PT_R1]
# define UC_ARG0(uc) UC_M (uc).uc_regs->gregs[PT_R3]
#elif defined __riscv
# define UC_PC(uc)   UC_M (uc).__gregs[REG_PC]
# define UC_SP(uc)   UC_M (uc).__gregs[REG_SP]
# define UC_ARG0(uc) UC_M (uc).__gregs[REG_A0]
#elif defined __sh__
# define UC_PC(uc)   UC_M (uc).pc
# define UC_SP(uc)   UC_M (uc).gregs[15]
# define UC_ARG0(uc) UC_M (uc).gregs[4]
#elif defined __sparc__ && defined __arch64__
/* The kernel passes no ucontext on sparc: the third argument of an
   SA_SIGINFO handler is the siginfo, with the pt_regs behind it (as
   glibc's sparc64 sigcontextinfo.h has it).  %sp carries the stack bias.  */
# include <asm/ptrace.h>
# define UC_REGS(uc) ((struct pt_regs *) ((siginfo_t *) (uc) + 1))
# define UC_PC(uc)   UC_REGS (uc)->tpc
# define UC_SP(uc)   (UC_REGS (uc)->u_regs[14] + 2047)
# define UC_ARG0(uc) UC_REGS (uc)->u_regs[8]
#elif defined __sparc__
/* The third argument is the kernel's register set, struct sigcontext
   in uClibc's <bits/sigcontext.h> (glibc's sparc32 sigcontextinfo.h).  */
# define UC_SC(uc)   (&((struct sigcontext *) (uc))->si_regs)
# define UC_PC(uc)   UC_SC (uc)->pc
# define UC_SP(uc)   UC_SC (uc)->u_regs[14]
# define UC_ARG0(uc) UC_SC (uc)->u_regs[8]
#elif defined __tile__
# define UC_PC(uc)   UC_M (uc).pc
# define UC_SP(uc)   UC_M (uc).sp
# define UC_ARG0(uc) UC_M (uc).gregs[0]
#elif defined __x86_64__
# define UC_PC(uc)   UC_M (uc).gregs[REG_RIP]
# define UC_SP(uc)   UC_M (uc).gregs[REG_RSP]
# define UC_ARG0(uc) UC_M (uc).gregs[REG_RDI]
#elif defined __xtensa__
# define UC_PC(uc)   UC_M (uc).sc_pc
# define UC_SP(uc)   UC_M (uc).sc_a[1]
# define UC_ARG0(uc) UC_M (uc).sc_a[2]
#else
# error "no ucontext register access for this architecture"
#endif

/* UC_ENTER (uc, a): make the context resume at the entry of the function
   whose code starts at A, the way a call would have entered it.  Besides
   the PC, some ABIs need a second program counter, the function address
   in a register the prologue derives its data pointer from, a call's
   stack alignment or the instruction set bit.  */
#if defined __alpha__
# define UC_ENTER(uc, a) \
  (UC_M (uc).sc_pc = (a), UC_M (uc).sc_regs[27] = (a))
#elif defined __arm__
# define UC_ENTER(uc, a) \
  (UC_M (uc).arm_pc = (a) & ~1UL,					\
   UC_M (uc).arm_cpsr = ((a) & 1) ? (UC_M (uc).arm_cpsr | 0x20)	\
				  : (UC_M (uc).arm_cpsr & ~0x20UL))
#elif defined __hppa__
# define UC_ENTER(uc, a) \
  (UC_M (uc).sc_iaoq[0] = (a) | 3, UC_M (uc).sc_iaoq[1] = ((a) + 4) | 3)
#elif defined __i386__
# define UC_ENTER(uc, a) \
  (UC_M (uc).gregs[REG_EIP] = (a),					\
   UC_M (uc).gregs[REG_ESP] = (UC_M (uc).gregs[REG_ESP] & ~15UL) - 4)
#elif defined __x86_64__
# define UC_ENTER(uc, a) \
  (UC_M (uc).gregs[REG_RIP] = (a),					\
   UC_M (uc).gregs[REG_RSP] = (UC_M (uc).gregs[REG_RSP] & ~15UL) - 8)
#elif defined __mips__
# define UC_ENTER(uc, a) \
  (UC_M (uc).pc = (a), UC_M (uc).gregs[25] = (a))
#elif defined __sparc__ && defined __arch64__
# define UC_ENTER(uc, a) \
  (UC_REGS (uc)->tpc = (a), UC_REGS (uc)->tnpc = (a) + 4)
#elif defined __sparc__
# define UC_ENTER(uc, a) \
  (UC_SC (uc)->pc = (a), UC_SC (uc)->npc = (a) + 4)
#elif defined __xtensa__ && !defined __XTENSA_CALL0_ABI__
/* As a call4 from the interrupted function: the return address in a4
   with the window increment in its top bits, the stack pointer in a5,
   the argument in a6 and PS.CALLINC 1.  The callee's entry then rotates
   the window by four and finds its caller's frame intact, which a later
   window overflow needs.  */
# define UC_ENTER(uc, a) \
  (UC_M (uc).sc_a[4] = (UC_M (uc).sc_pc & 0x3fffffffUL) | 0x40000000UL, \
   UC_M (uc).sc_a[5] = UC_M (uc).sc_a[1],				\
   UC_M (uc).sc_ps = (UC_M (uc).sc_ps & ~(3UL << 16)) | (1UL << 16),	\
   UC_M (uc).sc_pc = (a))
# define UC_ENTER_ARG0(uc) UC_M (uc).sc_a[6]
#elif !defined UC_NO_PC
# define UC_ENTER(uc, a) (UC_PC (uc) = (a))
#endif
