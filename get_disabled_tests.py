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
# LinuxThreads has no TLS and is not NPTL, but it does have threads: test/pthread
# holds ex1..ex8, the original LinuxThreads examples.  Running them is what the
# NO_THREADS entry used to prevent.  sparc goes first -- extend the exception to
# the other linuxthreads targets once we know what they do with those tests.
if is_set("UCLIBC_HAS_LINUXTHREADS"):
    no.update(["NO_NPTL", "NO_TLS"])
    if not arch("sparc"):
        no.add("NO_THREADS")

if arch("kvx"):
    no.add("NO_TLS")
# no tls-macros-nds32.h in test/tls -> tst-tls* hit the
# "No support for this architecture" #error and fail to compile
if arch("nds32"):
    no.add("NO_TLS")

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

# Only with the option: neither the function nor its declaration exist without
# it, so these do not even build.
if not is_set("UCLIBC_HAS_REENTRANT_RPC"):
    disable("rpc/getrpcent_r")
if not is_set("UCLIBC_HAS_ARC4RANDOM"):
    disable("stdlib/testarc4random")

# Inherited from the makefiles as they were, without a reason recorded.  Most
# of these targets have no qemu in the CI, so nobody has checked whether they
# still fail; prune with a run, not by guessing.
if arch("sh"):
    # "deadlock"
    disable(*["nptl/" + t for t in (
        "tst-cond1", "tst-cond4", "tst-cond6", "tst-cond7", "tst-cond10",
        "tst-cond13", "tst-cond14", "tst-cond15", "tst-cond18", "tst-cond20",
        "tst-cond21", "tst-cond22", "tst-cputimer1", "tst-cputimer2",
        "tst-cputimer3", "tst-mutex8", "tst-timer4", "tst-timer5", "tst-spin3")])
    disable("pthread/ex2", "pthread/ex7")
if arch("metag"):
    disable("nptl/tst-cleanup0", "nptl/tst-cleanupx0")
if arch("riscv32"):
    disable("nptl/tst-rwlock9")
if arch("riscv64"):
    disable("inet/tst-ifaddrs")
if arch("nds32"):
    disable("pthread/ex3")
if arch("cris"):
    disable("pthread/ex2", "pthread/ex5")
if arch("avr32"):
    disable("misc/tst-inotify", "time/tst-timerfd", "unistd/tst-fallocate",
            "unistd/tst-posix_fallocate", "unistd/tst-fallocate64")
if arch("bfin"):
    disable("misc/tst-rlimit", "misc/tst-rlimit64")
if arch("h8300"):
    disable("string/stratcliff")
if arch("or1k"):
    disable("unistd/tst-pselect")


out = [n + "=1" for n in sorted(no)]
if tests:
    out.append("DISABLED_TESTS=" + ",".join(tests))
print(" ".join(out))
