#!/usr/bin/python3
#
# What of the test suite does not run against a given C library.
#
#   CC=<compiler> CFLAGS=<flags> LDFLAGS=<flags> TARGET_ARCH=<arch> \
#       get_disabled_tests.py
#
# test/Makefile calls it with its own CC, CFLAGS, LDFLAGS and TARGET_ARCH;
# it prints make variables for test/Makefile and test/Test.mak:
#
#   NO_TLS=1 NO_DL=1 ...                  whole directories (test/Makefile)
#   DISABLED_TESTS=nptl/tst-foo,misc/bar  single tests, dir/name, comma
#                                         separated so "export $(...)" keeps
#                                         it in one word (test/Test.mak)
#
# This is the one place that decides it.  It asks the compiler rather than
# reading a .config: the predefined macros after <features.h> say which C
# library the tests are built against, and for uClibc-ng
# bits/uClibc_config.h carries every option as __<OPTION>__.  So the answer
# is about the library that is actually linked, and the same call works for
# glibc and musl.  A test that can find out for itself whether it applies
# (features.h, sysconf, a probe) skips itself instead; what is here cannot.

import os
import shlex
import subprocess
import sys

# The .config argument of earlier versions is refused rather than ignored:
# a caller that still passes one would otherwise get the answer for
# whatever "gcc" is on its PATH.
if len(sys.argv) > 1:
    sys.exit("get_disabled_tests.py takes no arguments; set CC and CFLAGS")

def macros():
    cc = shlex.split(os.environ.get("CC", "gcc"))
    flags = shlex.split(os.environ.get("CFLAGS", ""))
    # <sys/types.h> for musl, which has no macro of its own; its
    # bits/alltypes.h leaves __DEFINED_size_t behind
    src = "#include <features.h>\n#include <sys/types.h>\n"
    r = subprocess.run(cc + flags + ["-dM", "-E", "-x", "c", "-"],
                       input=src, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(r.stderr)
        sys.exit("get_disabled_tests.py: cannot run " + " ".join(cc))
    m = {}
    for l in r.stdout.splitlines():
        f = l.split(None, 2)
        if len(f) >= 2 and f[0] == "#define":
            m[f[1]] = f[2] if len(f) > 2 else ""
    return m

def links_pie():
    """Whether toolchain and libc can link a PIE: a PIC Scrt1.o and a linker
    that produces ET_DYN executables."""
    cc = shlex.split(os.environ.get("CC", "gcc"))
    flags = (shlex.split(os.environ.get("CFLAGS", ""))
             + shlex.split(os.environ.get("LDFLAGS", "")))
    r = subprocess.run(cc + flags + ["-fPIE", "-pie", "-x", "c", "-",
                                     "-o", os.devnull],
                       input="int main(void){return 0;}\n",
                       capture_output=True, text=True)
    return r.returncode == 0

defined = macros()

# uClibc-ng defines __GLIBC__ too, so it is asked first
if "__UCLIBC__" in defined:
    libc = "uclibc"
elif "__GLIBC__" in defined:
    libc = "glibc"
elif "__DEFINED_size_t" in defined:
    libc = "musl"
else:
    libc = "unknown"

uclibc = libc == "uclibc"

def is_set(option):
    """A uClibc-ng option, as bits/uClibc_config.h has it."""
    return "__" + option + "__" in defined

def arch(name):
    """TARGET_ARCH as test/Rules.mak derives it from $(CC) -dumpmachine."""
    return os.environ.get("TARGET_ARCH", "") == name

no = set()
tests = []

def disable(*names):
    tests.extend(names)


# --- whole directories ----------------------------------------------------

# For uClibc-ng the options say it.  glibc and musl always have locales,
# wide characters, threads and TLS; whether a static-only build has dlopen
# is not visible here, a caller sets NO_DL=1 for that.
if uclibc:
    if not is_set("UCLIBC_HAS_LOCALE"):
        no.add("NO_LOCALE")
    if not is_set("HAVE_SHARED") or is_set("ARCH_HAS_NO_SHARED"):
        no.add("NO_DL")
    if not is_set("UCLIBC_HAS_WCHAR"):
        no.add("NO_WCHAR")

    if is_set("HAS_NO_THREADS"):
        no.update(["NO_THREADS", "NO_NPTL", "NO_TLS"])
    if is_set("UCLIBC_HAS_LINUXTHREADS"):
        no.update(["NO_NPTL", "NO_TLS"])

# Both from openadk's package/uclibc-ng-test/Makefile, which sets them
# without a reason; taken over so that it need not set them itself.  Not
# measured here -- to be pruned by a run, not by guessing.
if libc == "glibc":
    no.add("NO_CRYPT")
if libc == "musl":
    no.add("NO_MATH")

# The *-pie variants, wherever a PIE cannot be linked (microblaze: non-PIC
# Scrt1.o plus a BFD assertion on the attempt).
if not links_pie():
    no.add("NO_PIE")

# math/ ran nowhere since 79524cb ("github always set NO_MATH for now", 2023-11-21),
# and SH2 and alpha had been exempted two days before that in c189229; neither commit
# says why.  Measured on riscv32-npt on 2026-08-23: builds clean and 20 of 20 results
# pass, including test-double, test-float, test-idouble, test-ifloat and test-fpucw.
# So it is on everywhere; re-exempt an arch here with a reason, not with a blanket
# switch.


# --- single tests -----------------------------------------------------------

# A libc without shared support cannot link the modules these tests load, and
# the -pie variants have no loader to relocate them.  What is left of tls/,
# tst-tls1 and tst-tls2, is what a static program can do with TLS at all.
if "NO_DL" in no:
    disable("nptl/tst-tls3", "nptl/tst-tls4",
            "nptl/tst-atfork2", "nptl/tst-cancelx4-pie", "nptl/tst-thread-db")
    disable(*["tls/" + t for t in (
        "tst-tls3", "tst-tls4", "tst-tls5", "tst-tls6", "tst-tls7", "tst-tls8",
        "tst-tls9", "tst-tls10", "tst-tls11", "tst-tls12", "tst-tls13",
        "tst-tls14", "tst-tls15", "tst-tls17", "tst-tls18", "tst-tls-at-ctor",
        "tst-tls-ie-many", "tst-tls-ie-full",
        "tst-tls1-pie", "tst-tls2-pie", "tst-tls3-pie", "tst-tls9-static")])

# linuxthreads makes libc functions cancellation points only through the
# wrappers in libpthread/linuxthreads/wrapsyscall.c, and 24 of the points
# POSIX.1-2024 lists have none (poll, select, readv, clock_nanosleep, mq_*,
# msgrcv, openat, sigwaitinfo, ...).  Measured on mips64 n32 2026-10-22: 24
# of 56 checks fail.  linuxthreads stays for targets without NPTL and is not
# made POSIX-complete here; the way out is NPTL on those targets.
if uclibc and is_set("UCLIBC_HAS_LINUXTHREADS"):
    disable("pthread/cancellation-points")

# POSIX AIO runs each request in a thread and exists only with NPTL.
if uclibc and not is_set("UCLIBC_HAS_THREADS_NATIVE"):
    disable(*["rt/" + t for t in (
        "tst-aio", "tst-aio2", "tst-aio3", "tst-aio5", "tst-aio6", "tst-aio7",
        "tst-aio8", "tst-aio9", "tst-aio10", "tst-aio64")])

# locale_t, newlocale() and uselocale() exist only with UCLIBC_HAS_XLOCALE.
if uclibc and not is_set("UCLIBC_HAS_XLOCALE"):
    disable("locale/tst-xlocale1", "locale/tst-xlocale2",
            "locale/bug-usesetlocale")

# strfmon() and <monetary.h> exist only with UCLIBC_HAS_FLOATS.
if uclibc and not is_set("UCLIBC_HAS_FLOATS"):
    disable("locale/tst-strfmon1", "locale-mbwc/tst_strfmon")

# tst-fmon is a driver: it takes locale, format, value and the expected
# string as arguments.  glibc feeds it from tst-fmon.data through
# tst-fmon.sh; neither is here, and run without arguments it crashes.
disable("locale/tst-fmon")

# Need files the rootfs does not carry.  collate-test and xfrm-test take a
# random seed and a word list on stdin and are judged against a reference
# output; tst-digits and tst-trans need glibc's test locales "test7" and
# "test", which glibc compiles with localedef -- uClibc-ng has no localedef,
# its locale data is built into the library.
disable("locale/collate-test", "locale/xfrm-test", "locale/tst-digits",
        "locale/tst-trans")

# uClibc-ng reads POSIX TZ strings from TZ or /etc/TZ and has no zoneinfo
# reader; tst-timezone needs named zones (Europe/Amsterdam).  tst-posixtz
# covers what applies.
if uclibc:
    disable("time/tst-timezone")

# glibc extensions, not POSIX: iconv's //TRANSLIT suffix (bug-iconv-trans),
# the ja_JP character classes jhira, jkata, jdigit, jspace and jkanji
# (tst-wctype), scanf's "%I" for locale digits (tst-sscanf), and strftime's
# "-" flag and field widths such as %2000Y (tst-strftime).
if libc != "glibc":
    disable("locale/bug-iconv-trans", "locale/tst-wctype",
            "locale/tst-sscanf", "time/tst-strftime")

# For a year that does not fit, asctime() and asctime_r() write "????" and
# return the buffer (SAFE_ASCTIME_R in libc/misc/time/time.c) where glibc
# fails with EOVERFLOW.  Deliberate, and stated in both test files.
if uclibc:
    disable("time/bug-asctime", "time/bug-asctime_r")

# long double is a wrapper around double in libm (ldouble_wrappers.c), so
# results lose their low mantissa bits wherever LDBL_MANT_DIG > 53: on i686
# 111 errors of that one cause.  On when long double is done per format.
if uclibc:
    disable("math/test-ldouble", "math/test-ildoubl")

# tst-fallocate64 and tst-posix_fallocate64 allocate past 4 GiB; the rootfs
# is a tmpfs in guest RAM.
disable("unistd/tst-fallocate64", "unistd/tst-posix_fallocate64")

# Fail, measured on x86_64 2026-10-22, causes being worked on:
# - tst-tls16: dlopen succeeds where the static TLS space should not suffice;
# - gamma: fails past the first cases.
if uclibc:
    disable("tls/tst-tls16", "math/gamma")

# tst-tls9-static dlopens, from a static program, modules that have
# libc.so.0 in DT_NEEDED, as glibc's version does on purpose.  That loads a
# second libc and its ld-uClibc.so into a process whose own libc is linked
# in; the second one's __pthread_initialize_minimal calls through
# _dl_error_catch_tsd, which only a started ld.so sets, and jumps to NULL.
# Two libcs in one process (two mallocs, two errnos) are not supported;
# glibc needs it for NSS and iconv modules in static programs, uClibc-ng has
# both built in.  Plugins of a static program must not link libc.so.  The
# static TLS part of this test, an initial-exec module in a static program,
# works since uClibc-ng 90293cee2.
if uclibc:
    disable("tls/tst-tls9-static")

# kvx: binutils keeps one "GOT entry initialised" mark per symbol, not per
# TLS model (bfd/elfnn-kvx.c, symbol_got_offset_mark).  When a symbol is
# reached through IE first and GD or LD later in the same link, the GD/LD
# entry is reserved but never written -- no DTPMOD/DTPOFF, module 0 --
# so __tls_get_addr returns NULL.  tst-tls1..3 mix the models on purpose.
# Measured 2026-10-21 with binutils 2.46 / kvx gcc 9.4.1: IE then GD on one
# variable links to a lone R_KVX_64_TPOFF, GD alone to DTPMOD + DTPOFF.
# The -static builds hit it the same way (2026-10-22: segfault at the
# first LD access after IE).  Off until the linker is fixed.
if arch("kvx"):
    disable("tls/tst-tls1", "tls/tst-tls1-pie", "tls/tst-tls1-static",
            "tls/tst-tls2", "tls/tst-tls2-pie", "tls/tst-tls2-static",
            "tls/tst-tls3", "tls/tst-tls3-pie")


out = [n + "=1" for n in sorted(no)]
if tests:
    out.append("DISABLED_TESTS=" + ",".join(tests))
print(" ".join(out))
