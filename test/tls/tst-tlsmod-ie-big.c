/* For tst-tls-ie-full: more initial-exec TLS than the static TLS block
   keeps for dlopen by default.  */

__thread char ie_big[65536] __attribute__ ((tls_model ("initial-exec")));
__thread int ie_big_mark __attribute__ ((tls_model ("initial-exec"))) = 42;

int
ie_big_check (void)
{
  ie_big[65535] = 1;
  return ie_big_mark == 42 && ie_big[0] == 0 && ie_big[65535] == 1;
}
