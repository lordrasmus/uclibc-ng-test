/* A1LbP holds the GOT pointer only in PIC code, where the prologue sets it
   up.  The tests are also built without -fPIC: there the pointer comes from
   the prologue's own PC-relative pair (the assembler takes
   __GLOBAL_OFFSET_TABLE__ PC-relative), in a scratch register of the A0
   unit to go with CPC0; initial-exec takes the offset as an immediate
   (@TLSIENONPIC), as gcc does.  */
#ifdef __PIC__
# define TLS_GOT(reg) "MOV " reg ", A1LbP\n\t"
# define TLS_GOT_CLOBBER
#else
# define TLS_GOT(reg)						\
  "ADDT A0.2, CPC0, #HI(__GLOBAL_OFFSET_TABLE__)\n\t"		\
  "ADD A0.2, A0.2, #LO(__GLOBAL_OFFSET_TABLE__ + 4)\n\t"		\
  "MOV " reg ", A0.2\n\t"
# define TLS_GOT_CLOBBER : "A0.2"
#endif

#define TLS_GD(x)					\
  ({ void *__result;					\
     extern void *__tls_get_addr (void *);		\
     __asm__ (TLS_GOT ("%0")				\
	  "ADD %0, %0, #(" TLS_ASM_NAME (x) "@TLSGD)"			\
	  : "=d" (__result) : TLS_GOT_CLOBBER);		\
     (int *)__tls_get_addr (__result); })

#define TLS_LD(x)					\
  ({ void *__result;					\
     extern void *__tls_get_addr (void *);		\
     __asm__ (TLS_GOT ("%0")				\
	  "ADD %0, %0, #(" TLS_ASM_NAME (x) "@TLSLDM)"		\
	  : "=d" (__result) : TLS_GOT_CLOBBER);		\
     __result = __tls_get_addr (__result);		\
     __asm__ ("ADDT %0,%0,#HI(" TLS_ASM_NAME (x) "@TLSLDO)\n\t"	\
	  "ADD %0,%0,#LO(" TLS_ASM_NAME (x) "@TLSLDO)"		\
	  : "+d" (__result));				\
     __result; })

#ifdef __PIC__
# define TLS_IE(x)					\
  ({ void *__result;					\
     unsigned long __rel;				\
     extern void *__metag_load_tp (void);		\
     __asm__ ("GETD %0,[A1LbP+#(" TLS_ASM_NAME (x) "@TLSIE)]"		\
	  : "=d" (__rel));				\
     __result = __metag_load_tp();			\
     __result + __rel; })
#else
# define TLS_IE(x)					\
  ({ void *__result;					\
     unsigned long __rel;				\
     extern void *__metag_load_tp (void);		\
     __asm__ ("MOVT %0,#HI(" TLS_ASM_NAME (x) "@TLSIENONPIC)\n\t"	\
	  "ADD %0,%0,#LO(" TLS_ASM_NAME (x) "@TLSIENONPIC)"		\
	  : "=d" (__rel));				\
     __result = __metag_load_tp();			\
     __result + __rel; })
#endif

#define TLS_LE(x)					\
  ({ void *__result;					\
     extern void *__metag_load_tp (void);		\
     __result = __metag_load_tp();			\
     __asm__ ("ADDT %0,%0,#HI(" TLS_ASM_NAME (x) "@TLSLE)\n\t"	\
	  "ADD %0,%0,#LO(" TLS_ASM_NAME (x) "@TLSLE)"			\
	  : "+d" (__result));				\
     __result; })
