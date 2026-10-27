/* TLS access macros for CRIS, after the code gcc emits for each model.
   The thread pointer is $pid with its low 8 bits (the MMU context)
   cleared on v32, and r6 on v10.  gas takes TPOFF and IE only without
   --pic and TPOFFGOT only with it, so PIC code goes through the GOT.
   There is no general-dynamic model on CRIS, so GD and LD are
   initial-exec, which is what gcc does for them as well.  */

#ifdef __arch_v32
# define __CRIS_TP(tp) \
  "move $pid," tp "\n\t"						\
  "lsrq 8," tp "\n\t"							\
  "lslq 8," tp "\n\t"
#else
# define __CRIS_TP(tp) \
  "move.d $r6," tp "\n\t"
#endif

#ifndef __PIC__
# define TLS_LE(x) \
  ({ int *__result;							\
     __asm__ (__CRIS_TP ("%0")						\
	      "add.d " #x ":TPOFF,%0"					\
	      : "=r" (__result));					\
     __result; })

# define TLS_IE(x) \
  ({ int *__result, *__off;						\
     __asm__ ("move.d " #x ":IE,%1\n\t"					\
	      "move.d [%1],%1\n\t"					\
	      __CRIS_TP ("%0")						\
	      "add.d %1,%0"						\
	      : "=&r" (__result), "=&r" (__off));			\
     __result; })
#else
# define TLS_IE(x) \
  ({ int *__result, *__off, *__got;					\
     __asm__ ("move.d " #x ":TPOFFGOT,%1\n\t"				\
	      "lapc _GLOBAL_OFFSET_TABLE_,%2\n\t"			\
	      "add.d %2,%1\n\t"						\
	      "move.d [%1],%1\n\t"					\
	      __CRIS_TP ("%0")						\
	      "add.d %1,%0"						\
	      : "=&r" (__result), "=&r" (__off), "=&r" (__got));	\
     __result; })

# define TLS_LE(x) TLS_IE (x)
#endif

#define TLS_LD(x) TLS_IE (x)
#define TLS_GD(x) TLS_IE (x)
