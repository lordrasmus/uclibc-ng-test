/* Testsuite runner.
 *
 * Replaces uclibcng-testrunner.sh and its two variants, which differed only in
 * how they announce a test and whether they colour the result:
 *
 *   uclibcng-testrunner.sh        plain text, overwriting ".... name" progress
 *   uclibcng-testrunner_qemu.sh   plain text, a "RUN name" line per test
 *   uclibcng-testrunner_color.sh  ANSI colours, overwriting progress
 *
 * so this takes -r and -c and covers all three.  The output is byte for byte
 * what the scripts produced, plus one new line per test:
 *
 *   TIME <name> <milliseconds>
 *
 * The reason for C is not elegance.  The shell version spends up to three
 * busybox execs per test -- the subshell around the test, the "env" prefix on
 * 533 of 841 lines, and "expr" for the counters -- and on a noMMU target every
 * exec re-loads busybox, some 1.1 MB, which the allocator rounds up to 2 MB and
 * cannot reuse.  That is what made riscv32 noMMU die with signal 11 partway
 * through a run, from test 132 onwards, whatever the test was.  Here a test
 * costs one vfork and one exec of the test binary itself.
 *
 * vfork, not fork: fork() does not exist without an MMU.  Linux gives the vfork
 * child its own file descriptors and its own working directory (no CLONE_FILES,
 * no CLONE_FS), so chdir and dup2 in the child are safe -- but its memory is
 * the parent's, so argv and envp are built before the vfork and the child does
 * nothing but syscalls.
 *
 * Copyright (c) 2015 Thorsten "mirabilos" Glaser <tg@mirbsd.org> for the shell
 * version this follows.
 *
 * Provided that these terms and disclaimer and all copyright notices are
 * retained or reproduced in an accompanying document, permission is granted to
 * deal in this work without restriction, including unlimited rights to use,
 * publicly perform, distribute, sell, modify, merge, give away, or sublicence.
 *
 * This work is provided "AS IS" and WITHOUT WARRANTY of any kind.
 */

#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/stat.h>

#define MAXLINE   8192
#define MAXWORDS  64
#define MAXENV    16
#define MAXPATHL  1024

static int opt_color;		/* -c: ANSI colours and a padded subdir column */
static int opt_runlines;	/* -r: "RUN name" instead of the \r progress   */

static int nfail, nskip, npass;

/* ------------------------------------------------------------------ output */

/* The scripts padded the subdir to 15 columns with printf '%*s'; a negative
   width does the same and needs no second pass. */
static void
result(const char *what, const char *colour, const char *subdir,
       const char *name, const char *tail)
{
	if (opt_color)
		printf("\r\033[01;%sm%s\033[00m %-15s %s%s\n",
		       colour, what, subdir, name, tail);
	else
		printf("%s %s%s\n", what, name, tail);
}

/* Print a file with every line indented, the way "sed 's/^/\t/'" did.  Used
   for the output of a test that failed or skipped; silent if it is not there. */
static void
cat_indented(const char *path, const char *indent)
{
	FILE *f = fopen(path, "r");
	int c, bol = 1;

	if (!f)
		return;
	while ((c = getc(f)) != EOF) {
		if (bol) {
			fputs(indent, stdout);
			bol = 0;
		}
		putchar(c);
		if (c == '\n')
			bol = 1;
	}
	if (!bol)
		putchar('\n');
	fclose(f);
}

/* A test that passed may still have said it could not check something.  Its
   output goes nowhere otherwise -- only the failure paths print it -- so the
   one kind of line that has to survive a pass is surfaced, as the scripts did
   with grep -e '^SKIP' -e 'skipped'. */
static void
surface_skips(const char *path)
{
	char buf[MAXLINE];
	FILE *f = fopen(path, "r");

	if (!f)
		return;
	while (fgets(buf, sizeof buf, f)) {
		if (strncmp(buf, "SKIP", 4) == 0 || strstr(buf, "skipped")) {
			size_t n = strlen(buf);
			fputs("\t", stdout);
			fputs(buf, stdout);
			if (n == 0 || buf[n - 1] != '\n')
				putchar('\n');
		}
	}
	fclose(f);
}

/* ------------------------------------------------------------- the command */

/* The cmd field of uclibcng-testrunner.in needs no shell.  Measured over the
   841 lines of a sparc run: 296 are a bare ./prog, 10 add arguments, 533 carry
   an "env VAR=value" prefix (one of them env -i), exactly one redirects stdin
   and exactly one is prefixed with 'trap ":" ABRT ;'.  The only expansion that
   occurs at all is $PWD, on 529 lines, and the only quoting is a double quoted
   value.  So: honour double quotes, expand $PWD, and understand env, < and the
   trap prefix.
 *
 * Splits s into words in buf, writing pointers into w.  Not in place: $PWD is
 * four characters standing for a whole path, so the result is longer than the
 * input and would overwrite what has not been read yet.  Returns the count, or
 * -1 if it does not fit. */
static int
split(const char *s, char **w, int max, char *buf, size_t bufsz,
      const char *pwd)
{
	int n = 0, quoted;
	char *out = buf;
	char *end = buf + bufsz - 1;
	size_t pwdlen = strlen(pwd);

	while (*s) {
		while (*s == ' ' || *s == '\t')
			s++;
		if (!*s)
			break;
		if (n >= max)
			return -1;
		w[n++] = out;
		quoted = 0;
		/* The quote state has to toggle anywhere in the word, not just
		   at its start: the one quoted value in the file is
		   LD_LIBRARY_PATH="$PWD:.:", where the quote sits behind the
		   equals sign.  Treating it as a leading quote only splits the
		   word and the program name ends up wrong. */
		while (*s && (quoted || (*s != ' ' && *s != '\t'))) {
			if (*s == '"') {
				quoted = !quoted;
				s++;
			} else if (strncmp(s, "$PWD", 4) == 0) {
				/* expands inside quotes too, as in the shell */
				if (out + pwdlen >= end)
					return -1;
				memcpy(out, pwd, pwdlen);
				out += pwdlen;
				s += 4;
			} else {
				if (out >= end)
					return -1;
				*out++ = *s++;
			}
		}
		if (out >= end)
			return -1;
		*out++ = '\0';
	}
	return n;
}

/* ------------------------------------------------------------------- a run */

/* Runs one test and returns the status the shell would have reported in $?,
   that is 128 + signal when it died of one.  -1 means it could not be run. */
static int
run_one(char **w, int nw, const char *outfile, char **environ_in)
{
	char *argv[MAXWORDS];
	char *saved[MAXENV];
	/* volatile: the vfork child runs in this frame and reads it, so the
	   compiler must not keep it somewhere the child's execution clobbers.
	   gcc warns about exactly this without it. */
	const char *volatile redirect = NULL;
	int i = 0, na = 0, ne = 0, ns = 0, clear_env = 0;
	pid_t pid;
	int status;

	/* the trap prefix: 'trap ":" ABRT ;' -- the shell used it to keep quiet
	   about a test that aborts on purpose, and there is nothing here that
	   would announce it, so it just goes */
	if (i < nw && strcmp(w[i], "trap") == 0) {
		while (i < nw && strcmp(w[i], ";") != 0)
			i++;
		if (i < nw)
			i++;
	}

	if (i < nw && strcmp(w[i], "env") == 0) {
		i++;
		if (i < nw && strcmp(w[i], "-i") == 0) {
			clear_env = 1;
			i++;
		}
		while (i < nw && strchr(w[i], '=') != NULL) {
			if (ns >= MAXENV)
				return -1;
			saved[ns++] = w[i++];
		}
	}

	for (; i < nw; i++) {
		if (strcmp(w[i], "<") == 0 && i + 1 < nw) {
			redirect = w[++i];
			continue;
		}
		if (na >= MAXWORDS - 1)
			return -1;
		argv[na++] = w[i];
	}
	argv[na] = NULL;
	if (na == 0)
		return -1;

	/* Build the environment before the vfork: the child shares this memory
	   and may not write to it.  setenv() would be simpler but it edits the
	   parent's environment, and these assignments are meant for one test
	   only -- LD_LIBRARY_PATH above all, which must not leak into the next
	   one.  The array grows as needed and is kept for the next call; there
	   is one of these per test, so it settles after the first. */
	{
		static char **envbuf;
		static int envcap;
		int nold = 0, need;

		if (!clear_env)
			while (environ_in[nold])
				nold++;
		need = ns + nold + 1;
		if (need > envcap) {
			char **p = realloc(envbuf, need * sizeof *p);
			if (!p)
				return -1;
			envbuf = p;
			envcap = need;
		}
		for (i = 0; i < ns; i++)
			envbuf[ne++] = saved[i];
		for (i = 0; i < nold; i++)
			envbuf[ne++] = environ_in[i];
		envbuf[ne] = NULL;

		pid = vfork();
		if (pid == 0) {
			int fd;

			/* Syscalls only from here: this is the parent's
			   address space. */
			fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if (fd < 0)
				_exit(127);
			dup2(fd, 1);
			dup2(fd, 2);
			if (fd > 2)
				close(fd);
			fd = open(redirect ? redirect : "/dev/null", O_RDONLY);
			if (fd < 0)
				_exit(127);
			dup2(fd, 0);
			if (fd > 2)
				close(fd);
			execve(argv[0], argv, envbuf);
			_exit(127);
		}
	}
	if (pid < 0)
		return -1;
	while (waitpid(pid, &status, 0) < 0)
		if (errno != EINTR)
			return -1;
	if (WIFSIGNALED(status))
		return 128 + WTERMSIG(status);
	return WEXITSTATUS(status);
}

/* --------------------------------------------------------------- .good file */

/* Byte comparison; the scripts used diff -u and only looked at whether it
   reported a difference.  There are five .good files in a whole rootfs, so the
   external diff for the report costs nothing and keeps the output identical.

   Only <binary_name>.out.good is consulted.  The script also named a second
   candidate, "$test_src_name.out", but the variable it read is tst_src_name --
   the misspelt one is always empty, so that candidate never existed.  Kept as
   it was: this is a rewrite, not a change of what gets compared. */
static int
same_file(const char *a, const char *b)
{
	FILE *fa = fopen(a, "r"), *fb = fopen(b, "r");
	int ca, cb, eq = 1;

	if (!fa || !fb) {
		if (fa) fclose(fa);
		if (fb) fclose(fb);
		return 0;
	}
	do {
		ca = getc(fa);
		cb = getc(fb);
		if (ca != cb) {
			eq = 0;
			break;
		}
	} while (ca != EOF);
	fclose(fa);
	fclose(fb);
	return eq;
}

static void
show_diff(const char *out, const char *good)
{
	char cmd[2 * MAXPATHL + 64];

	/* only on the failure path, and only for the report */
	snprintf(cmd, sizeof cmd, "diff -u '%s' '%s' | sed 's/^/       /'",
		 out, good);
	fflush(stdout);
	/* the diff itself is the message; its exit status says only that the
	   files differ, which is why we are here */
	if (system(cmd) < 0)
		fputs("       (diff unavailable)\n", stdout);
}

/* ------------------------------------------------------------------- main */

extern char **environ;

int
main(int argc, char **argv)
{
	const char *infile = "uclibcng-testrunner.in";
	char line[MAXLINE], cwd[MAXPATHL], subcwd[MAXPATHL];
	char wordbuf[MAXLINE * 2];	/* $PWD makes the result longer */
	char *w[MAXWORDS];
	FILE *in;
	int i;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-c") == 0)
			opt_color = 1;
		else if (strcmp(argv[i], "-r") == 0)
			opt_runlines = 1;
		else if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
			infile = argv[++i];
		else {
			fprintf(stderr, "usage: %s [-c] [-r] [-f file]\n"
				"  -c  colour the result and pad the subdir column\n"
				"  -r  print \"RUN name\" instead of overwriting progress\n",
				argv[0]);
			return 1;
		}
	}

	if (!getcwd(cwd, sizeof cwd)) {
		fprintf(stderr, "E: getcwd: %s\n", strerror(errno));
		return 1;
	}
	if ((in = fopen(infile, "r")) == NULL) {
		fprintf(stderr, "E: %s not found\n", infile);
		return 1;
	}

	while (fgets(line, sizeof line, in)) {
		char *p = line, *expected_s, *binary, *subdir, *rest;
		char outfile[MAXPATHL], goodfile[MAXPATHL], outpath[MAXPATHL];
		int expected, ret, nw;
		struct timeval t0, t1;
		long ms;

		p[strcspn(p, "\r\n")] = '\0';

		/* expected_ret tst_src_name binary_name subdir cmd... */
		expected_s = strtok(p, " \t");
		if (!expected_s)
			continue;
		(void) strtok(NULL, " \t");		/* tst_src_name, unused */
		binary = strtok(NULL, " \t");
		subdir = strtok(NULL, " \t");
		if (!binary || !subdir)
			continue;
		rest = strtok(NULL, "");
		if (!rest)
			continue;
		expected = atoi(expected_s);

		if (opt_runlines)
			printf("RUN %s\n", binary);
		else
			printf(".... %s\r", binary);
		fflush(stdout);

		if (chdir(subdir) != 0) {
			result("FAIL", "31", subdir, binary, " cannot chdir");
			nfail++;
			continue;
		}
		if (!getcwd(subcwd, sizeof subcwd))
			subcwd[0] = '\0';

		snprintf(outfile, sizeof outfile, "%s.out", binary);
		nw = split(rest, w, MAXWORDS, wordbuf, sizeof wordbuf, subcwd);

		gettimeofday(&t0, NULL);
		ret = nw < 0 ? -1 : run_one(w, nw, outfile, environ);
		gettimeofday(&t1, NULL);
		ms = (t1.tv_sec - t0.tv_sec) * 1000L
		   + (t1.tv_usec - t0.tv_usec) / 1000L;

		snprintf(outpath, sizeof outpath, "%s/%s.out", subdir, binary);
		snprintf(goodfile, sizeof goodfile, "%s.out.good", binary);

		if (ret == 23) {
			if (chdir(cwd) != 0) return 1;
			result("SKIP", "33", subdir, binary, "");
			printf("TIME %s %ld\n", binary, ms);
			nskip++;
			cat_indented(outpath, "\t");
			continue;
		}
		if (ret != expected) {
			char tail[64];
			snprintf(tail, sizeof tail, " got %d expected %d",
				 ret, expected);
			if (chdir(cwd) != 0) return 1;
			result("FAIL", "31", subdir, binary, tail);
			printf("TIME %s %ld\n", binary, ms);
			nfail++;
			cat_indented(outpath, "\t");
			continue;
		}

		if (access(goodfile, F_OK) == 0) {
			int ok = same_file(outfile, goodfile);
			char gpath[MAXPATHL];

			snprintf(gpath, sizeof gpath, "%s/%s.out.good",
				 subdir, binary);
			if (chdir(cwd) != 0) return 1;
			if (ok) {
				result("PASS", "32", subdir, binary, "");
				printf("TIME %s %ld\n", binary, ms);
				npass++;
			} else {
				result("FAIL", "31", subdir, binary,
				       " expected output differs");
				printf("TIME %s %ld\n", binary, ms);
				nfail++;
				show_diff(outpath, gpath);
			}
			continue;
		}

		if (chdir(cwd) != 0) return 1;
		result("PASS", "32", subdir, binary, "");
		printf("TIME %s %ld\n", binary, ms);
		npass++;
		surface_skips(outpath);
	}
	fclose(in);

	if (opt_color) {
		printf("Total skipped :\033[01;33m %d \033[00m\n", nskip);
		printf("Total failed  :\033[01;31m %d \033[00m\n", nfail);
		printf("Total passed  :\033[01;32m %d \033[00m\n", npass);
	} else {
		printf("Total skipped: %d\n", nskip);
		printf("Total failed: %d\n", nfail);
		printf("Total passed: %d\n", npass);
	}
	return nfail == 0 ? 0 : 1;
}
