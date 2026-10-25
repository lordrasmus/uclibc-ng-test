/* Built twenty times with N = 0..19 for tst-tls-ie-many: one variable
   with initial-exec TLS, which dlopen has to place in the static TLS
   block, and the address its own code reaches it at.  */

__thread int ie_var __attribute__ ((tls_model ("initial-exec"))) = 100 + N;

int *
ie_addr (void)
{
  int *p = &ie_var;

  /* GCC assumes &var is never NULL, add optimization barrier.  */
  __asm__ __volatile__ ("" : "+r" (p));
  return p;
}
