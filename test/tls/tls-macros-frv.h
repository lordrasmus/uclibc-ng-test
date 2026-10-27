/* TLS access macros for FR-V FDPIC, after the code gcc emits for each
   model.  gr29 is the thread pointer, gr15 the FDPIC GOT pointer.  The
   32-bit hi/lo forms work for a TLS segment of any size.  */

#define TLS_LE(x) \
  ({ int *__result;							\
     __asm__ ("sethi.p #tlsmoffhi(" #x "), %0\n\t"			\
	      "setlo #tlsmofflo(" #x "), %0\n\t"			\
	      "add %0, gr29, %0"					\
	      : "=r" (__result));					\
     __result; })

#define TLS_IE(x) \
  ({ int *__result;							\
     register void *__gr15 __asm__ ("gr15");				\
     __asm__ ("sethi.p #gottlsoffhi(" #x "), %0\n\t"			\
	      "setlo #gottlsofflo(" #x "), %0\n\t"			\
	      "ld #tlsoff(" #x ")@(%1, %0), %0\n\t"			\
	      "add %0, gr29, %0"					\
	      : "=&r" (__result) : "r" (__gr15));			\
     __result; })

/* The descriptor's function returns the offset from gr29 in gr9.  */
#define TLS_GD(x) \
  ({ int *__result;							\
     register void *__gr15 __asm__ ("gr15");				\
     __asm__ ("sethi.p #gottlsdeschi(" #x "), gr8\n\t"			\
	      "setlo #gottlsdesclo(" #x "), gr8\n\t"			\
	      "ldd #tlsdesc(" #x ")@(%1, gr8), gr8\n\t"			\
	      "calll #gettlsoff(" #x ")@(gr8, gr0)\n\t"			\
	      "add gr9, gr29, %0"					\
	      : "=r" (__result) : "r" (__gr15)				\
	      : "gr8", "gr9", "lr", "memory");				\
     __result; })

#define TLS_LD TLS_GD
