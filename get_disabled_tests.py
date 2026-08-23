#!/usr/bin/python

import sys

from pprint import pprint


def disable_if_not_set( value, disable ):
    global values,disabled
    
    if not value in values:
        disabled += disable

def disable_if_is_set( value, disable ):
    global values,disabled
    
    if value in values and values[value] == "y":
        disabled += disable
       
    
    

with open(sys.argv[1], 'r') as config_file:
    config_lines = config_file.readlines()

values = {}
disabled=[]

#print( values["HAS_NO_THREADS"] )

for l in config_lines:
    
    if l.startswith("#"): continue
    
    
    tmp = l.split("=")
        
    if tmp[0] == '\n': continue
    tmp[1] = tmp[1].replace("\n","").replace("\"","")
        
    values[tmp[0]] = tmp[1]

#pprint( values )


disable_if_not_set( "UCLIBC_HAS_LOCALE",              [ "NO_LOCALE" ] )
disable_if_not_set( "UCLIBC_HAS_OBSOLETE_BSD_SIGNAL", [ "NO_NPTL"] )
disable_if_not_set( "HAVE_SHARED",                    [ "NO_DL"] )
disable_if_not_set( "UCLIBC_HAS_WCHAR",               [ "NO_WCHAR"] )

disable_if_is_set(  "HAS_NO_THREADS",          ["NO_THREADS", "NO_NPTL", "NO_TLS" ] )
# LinuxThreads has no TLS and is not NPTL, but it does have threads: test/pthread
# holds ex1..ex8, the original LinuxThreads examples.  Running them is what the
# NO_THREADS entry used to prevent.  sparc goes first -- extend the exception to
# the other linuxthreads targets once we know what they do with those tests.
if not ("TARGET_sparc" in values and values["TARGET_sparc"] == "y"):
    disable_if_is_set(  "UCLIBC_HAS_LINUXTHREADS", ["NO_THREADS" ] )
disable_if_is_set(  "UCLIBC_HAS_LINUXTHREADS", ["NO_NPTL", "NO_TLS" ] )
disable_if_is_set(  "ARCH_HAS_NO_SHARED",      ["NO_DL"] );


disable_if_is_set(  "TARGET_kvx",      ["NO_TLS" ] )
# no tls-macros-nds32.h in test/tls -> tst-tls* hit the
# "No support for this architecture" #error and fail to compile
disable_if_is_set(  "TARGET_nds32",    ["NO_TLS" ] )

# math/ ran nowhere since 79524cb ("github always set NO_MATH for now", 2023-11-21),
# and SH2 and alpha had been exempted two days before that in c189229; neither commit
# says why.  Measured on riscv32-npt on 2026-08-23: builds clean and 20 of 20 results
# pass, including test-double, test-float, test-idouble, test-ifloat and test-fpucw.
# So turn it on everywhere and let the matrix say where it does not hold -- a target
# that needs a flag shows up here, the way alpha needed -mieee for the hexadecimal
# float printf.  Re-exempt an arch with disable_if_is_set() and a reason, not with a
# blanket switch.

disabled = list(set(disabled))          

text=""
for d in disabled:
    text+=d+"=1 "

print( text )
