/* TLS access macros for Blackfin, after the code gcc emits.  The thread
   pointer is B3.  gcc has local-exec only, so programs are static and
   the other models are local-exec as well.  */

#define TLS_LE(x) \
  ({ int *__result, *__off;						\
     __asm__ ("%0 = B3;\n\t"						\
	      "%1.L = " TLS_ASM_NAME (x) "@TPOFF;\n\t"				\
	      "%1.H = " TLS_ASM_NAME (x) "@TPOFF;\n\t"				\
	      "%0 = %0 + %1;"						\
	      : "=&a" (__result), "=&a" (__off));			\
     __result; })

#define TLS_IE(x) TLS_LE (x)
#define TLS_LD(x) TLS_LE (x)
#define TLS_GD(x) TLS_LE (x)
