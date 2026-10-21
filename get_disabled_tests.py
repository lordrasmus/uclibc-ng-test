#!/usr/bin/python3
#
# What of the test suite does not run against a given uClibc-ng build.
#
#   get_disabled_tests.py <uClibc-ng .config>
#
# prints make variables for test/Makefile and test/Test.mak:
#
#   NO_TLS=1 NO_DL=1 ...                  whole directories (test/Makefile)
#   DISABLED_TESTS=nptl/tst-foo,misc/bar  single tests, dir/name, comma
#                                         separated so "export $(...)" keeps
#                                         it in one word (test/Test.mak)
#
# This is the one place that decides it.  The makefiles used to carry their
# own ifeq blocks on TARGET_ARCH and on uClibc options -- and since the suite
# never reads the .config, the latter were always empty and fired on every
# target.  A test that can find out for itself whether it applies (features.h,
# sysconf, a probe) skips itself instead; what is here cannot.

import sys

values = {}
with open(sys.argv[1], 'r') as config_file:
    for l in config_file:
        if l.startswith("#") or "=" not in l:
            continue
        k, v = l.rstrip("\n").split("=", 1)
        values[k] = v.replace("\"", "")

def is_set(option):
    return values.get(option) == "y"

def arch(name):
    return is_set("TARGET_" + name)

no = set()
tests = []

def disable(*names):
    tests.extend(names)


# --- whole directories ----------------------------------------------------

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
    disable("nptl/tst-tls3", "nptl/tst-tls4", "nptl/tst-tls5",
            "nptl/tst-atfork2", "nptl/tst-cancelx4-pie", "nptl/tst-thread-db")
    disable(*["tls/" + t for t in (
        "tst-tls3", "tst-tls4", "tst-tls5", "tst-tls6", "tst-tls7", "tst-tls8",
        "tst-tls9", "tst-tls10", "tst-tls11", "tst-tls12", "tst-tls13",
        "tst-tls14", "tst-tls15", "tst-tls17", "tst-tls18", "tst-tls-at-ctor",
        "tst-tls1-pie", "tst-tls2-pie", "tst-tls3-pie")])

# kvx: binutils keeps one "GOT entry initialised" mark per symbol, not per
# TLS model (bfd/elfnn-kvx.c, symbol_got_offset_mark).  When a symbol is
# reached through IE first and GD or LD later in the same link, the GD/LD
# entry is reserved but never written -- no DTPMOD/DTPOFF, module 0 --
# so __tls_get_addr returns NULL.  tst-tls1..3 mix the models on purpose.
# Measured 2026-10-21 with binutils 2.46 / kvx gcc 9.4.1: IE then GD on one
# variable links to a lone R_KVX_64_TPOFF, GD alone to DTPMOD + DTPOFF.
# Off until the linker is fixed.
if arch("kvx"):
    disable("tls/tst-tls1", "tls/tst-tls1-pie", "tls/tst-tls2",
            "tls/tst-tls2-pie", "tls/tst-tls3", "tls/tst-tls3-pie")


out = [n + "=1" for n in sorted(no)]
if tests:
    out.append("DISABLED_TESTS=" + ",".join(tests))
print(" ".join(out))
