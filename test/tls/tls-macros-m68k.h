/* Macros for accessing thread-local storage.  m68k version.
   Copyright (C) 2010-2017 Free Software Foundation, Inc.
   Contributed by Maxim Kuvyrkov <maxim@codesourcery.com>, 2010.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library.  If not, see
   <http://www.gnu.org/licenses/>.  */

#define TLS_GD(x)							\
  ({									\
    void *__result;							\
    extern void *__tls_get_addr (void *);				\
									\
    asm ("movel #_GLOBAL_OFFSET_TABLE_@GOTPC, %0\n\t"			\
	 "lea (-6, %%pc, %0), %0\n\t"					\
	 "lea " #x "@TLSGD(%0), %0"					\
	 : "=&a" (__result));						\
    (int *) __tls_get_addr (__result); })

#define TLS_LD(x)							\
  ({									\
    char *__tp;								\
    int __offset;							\
    extern void *__tls_get_addr (void *);				\
									\
    asm ("movel #_GLOBAL_OFFSET_TABLE_@GOTPC, %0\n\t"			\
	 "lea (-6, %%pc, %0), %0\n\t"					\
	 "lea " #x "@TLSLDM(%0), %0"					\
	 : "=&a" (__tp));						\
    __tp = (char *) __tls_get_addr (__tp);				\
    asm ("movel #" #x "@TLSLDO, %0"					\
	 : "=a" (__offset));						\
    (int *) (__tp + __offset); })

#define TLS_IE(x)							\
  ({									\
    char *__tp;								\
    int __offset;							\
    extern void * __m68k_read_tp (void);				\
									\
    __tp = (char *) __m68k_read_tp ();					\
    asm ("movel #_GLOBAL_OFFSET_TABLE_@GOTPC, %0\n\t"			\
	 "lea (-6, %%pc, %0), %0\n\t"					\
	 "movel " #x "@TLSIE(%0), %0"					\
	 : "=&a" (__offset));						\
    (int *) (__tp + __offset); })

#define TLS_LE(x)							\
  ({									\
    char *__tp;								\
    int __offset;							\
    extern void * __m68k_read_tp (void);				\
									\
    __tp = (char *) __m68k_read_tp ();					\
    asm ("movel #" #x "@TLSLE, %0"					\
	 : "=a" (__offset));						\
    (int *) (__tp + __offset); })

#if defined __uClinux__ && !defined __PIC__
/* A uClinux program holds all its thread-local storage, and the FLAT
   loader relocates every GOT entry, which a thread-pointer offset must
   not be.  gcc uses local-exec there, and so do these.  */
# undef TLS_GD
# undef TLS_LD
# undef TLS_IE
# define TLS_GD(x) TLS_LE (x)
# define TLS_LD(x) TLS_LE (x)
# define TLS_IE(x) TLS_LE (x)
#endif
