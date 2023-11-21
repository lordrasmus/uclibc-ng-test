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
disable_if_is_set(  "UCLIBC_HAS_LINUXTHREADS", ["NO_THREADS", "NO_NPTL", "NO_TLS" ] )
disable_if_is_set(  "ARCH_HAS_NO_SHARED",      ["NO_DL"] );


disable_if_is_set(  "CONFIG_SH2",      ["NO_MATH" ] )
disable_if_is_set(  "TARGET_alpha",    ["NO_MATH" ] )
disable_if_is_set(  "TARGET_sparc",    ["NO_MISC", "NO_NPTL" ] )
disable_if_is_set(  "TARGET_kvx",      ["NO_TLS" ] )

if "TARGET_riscv64" in values:
    
    disable_if_not_set( "HAVE_SHARED",                    [ "NO_DL", "NO_ICONV", "NO_LOCALE", "NO_MISC", "NO_PTHREAD", "NO_TLS", "NO_MATH"] )

disabled.append("NO_MATH")

disabled = list(set(disabled))          

text=""
for d in disabled:
    text+=d+"=1 "

print( text )
