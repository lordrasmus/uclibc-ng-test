#!/bin/sh
#-
# Copyright (c) 2015
#	Thorsten "mirabilos" Glaser <tg@mirbsd.org>
#
# Provided that these terms and disclaimer and all copyright notices
# are retained or reproduced in an accompanying document, permission
# is granted to deal in this work without restriction, including un-
# limited rights to use, publicly perform, distribute, sell, modify,
# merge, give away, or sublicence.
#
# This work is provided "AS IS" and WITHOUT WARRANTY of any kind, to
# the utmost extent permitted by applicable law, neither express nor
# implied; without malicious intent or gross negligence. In no event
# may a licensor, author or contributor be held liable for indirect,
# direct, other damage, loss, or other issues arising in any way out
# of dealing in the work, even if advised of the possibility of such
# damage or existence of a defect, except proven that it results out
# of said person's immediate fault when using the work as intended.
#-
# Testsuite runner


die() {
	echo >&2 E: "$*"
	exit 1
}



test -s uclibcng-testrunner.in || die uclibcng-testrunner.in not found

nfail=0
nskip=0
npass=0
while read expected_ret tst_src_name binary_name subdir cmd; do
	printf '.... %s\r' "$binary_name"
	(cd $subdir && eval "$cmd" >$binary_name.out 2>&1) </dev/null
	ret=$?
	
	current_length="${#subdir}"
	diff=$((15 - current_length))
	padding=$(printf '%*s' "$diff" ' ')
	subdir_pad=${subdir}${padding}
	
	test $ret = "23" && {
		echo -e "\r\033[01;33mSKIP\033[00m $subdir_pad $binary_name"
		nskip=`expr $nskip + 1`
		sed 's/^/	/' <$subdir/$binary_name.out
		continue
	}
	test $ret = "$expected_ret" || {
		echo -e "\r\033[01;31mFAIL\033[00m $subdir_pad $binary_name got $ret expected $expected_ret"
		nfail=`expr $nfail + 1`
		sed 's/^/	/' <$subdir/$binary_name.out
		continue
	}
	for x in $binary_name.out $test_src_name.out -; do
		if test x"$x" = x"-"; then
			echo -e "\r\033[01;32mPASS\033[00m $subdir_pad $binary_name"
			npass=`expr $npass + 1`
			break
		fi
		test -e "$subdir/$x.good" || continue
		if d=`diff -u "$subdir/$binary_name.out" "$subdir/$x.good"`; then
			echo -e "\r\033[01;32mPASS\033[00m $subdir_pad $binary_name"
			npass=`expr $npass + 1`
		else
			echo -e "\r\033[01;31mFAIL\033[00m $subdir_pad $binary_name expected output differs"
			nfail=`expr $nfail + 1`
			echo "$d" | sed 's/^/       /'
		fi
		break
	done
done <uclibcng-testrunner.in
echo -e "Total skipped :\033[01;33m $nskip \033[00m"
echo -e "Total failed  :\033[01;31m $nfail \033[00m"
echo -e "Total passed  :\033[01;32m $npass \033[00m"
test $nfail = 0
