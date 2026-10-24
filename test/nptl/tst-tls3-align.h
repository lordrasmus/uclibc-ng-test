/* The alignment of struct pthread, which is what pthread_self () returns.
   uClibc-ng gives it TCB_ALIGNMENT, set per architecture to the stack
   alignment in libpthread/nptl/sysdeps/<arch>/pthreaddef.h, because the
   stack top of a new thread is derived from the descriptor's address.  It
   is not visible outside the library, hence the copy here; glibc's version
   of the test reads the internal constant instead.  */
#if defined __kvx__
# define TCB_ALIGNMENT 32
#elif defined __arc__ || defined __metag__ || defined __sh__
# define TCB_ALIGNMENT 8
#elif defined __nios2__
# define TCB_ALIGNMENT 4
#else
# define TCB_ALIGNMENT 16
#endif
