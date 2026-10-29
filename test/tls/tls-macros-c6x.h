/* TLS access macros for C6X, after the sequences of the C6000 EABI
   (SPRAB89, chapter 7).  __c6xabi_get_tp returns the thread pointer in
   A4 and, called through the PLT, may change B0, B1, B2, B30 and B31;
   __tls_get_addr takes the GOT pair in A5:A4.  B14 holds the data
   pointer of the module.  LD is done as GD, as gcc does.  */

#define __C6X_GET_TP_CLOBBERS "A4", "B0", "B1", "B2", "B3", "B30", "B31"

#define __C6X_CALL_CLOBBERS \
  "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7", "A8", "A9",		\
  "A16", "A17", "A18", "A19", "A20", "A21", "A22", "A23",		\
  "A24", "A25", "A26", "A27", "A28", "A29", "A30", "A31",		\
  "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7", "B8", "B9",		\
  "B16", "B17", "B18", "B19", "B20", "B21", "B22", "B23",		\
  "B24", "B25", "B26", "B27", "B28", "B29", "B30", "B31"

#define TLS_LE(x) \
  ({ int *__result;							\
     __asm__ ("callp .s2 (__c6xabi_get_tp), B3\n\t"			\
	      "mvk .s1 $tpr_byte(" #x "), %0\n\t"			\
	      "add .l1 %0, A4, %0"					\
	      : "=a" (__result) : : __C6X_GET_TP_CLOBBERS, "memory");	\
     __result; })

#define TLS_IE(x) \
  ({ int *__result;							\
     __asm__ ("ldw .d2t1 *+B14($got_tpr_byte(" #x ")), %0\n\t"	\
	      "nop 4\n\t"						\
	      "callp .s2 (__c6xabi_get_tp), B3\n\t"			\
	      "add .l1 %0, A4, %0"					\
	      : "=a" (__result) : : __C6X_GET_TP_CLOBBERS, "memory");	\
     __result; })

#define TLS_GD(x) \
  ({ int *__result;							\
     __asm__ ("ldw .d2t1 *+B14($got_tlsmod(" #x ")), A4\n\t"		\
	      "ldw .d2t1 *+B14($got_tbr(" #x ")), A5\n\t"		\
	      "nop 4\n\t"						\
	      "callp .s2 (__tls_get_addr), B3\n\t"			\
	      "mv .l1 A4, %0"						\
	      : "=a" (__result) : : __C6X_CALL_CLOBBERS, "memory");	\
     __result; })

#define TLS_LD(x) TLS_GD (x)
