# Makefile for uClibc-ng-test
# Licensed under the LGPL v2.1, see the file COPYING.LIB in this tarball.

top_srcdir=../
top_builddir=../
include Rules.mak

ALL_SUBDIRS := $(patsubst %/Makefile,%,$(wildcard */Makefile))

DIRS := $(ALL_SUBDIRS)

ifeq ($(NO_THREADS),1)
DIRS := $(filter-out nptl,$(DIRS))
DIRS := $(filter-out pthread,$(DIRS))
DIRS := $(filter-out dlopen,$(DIRS))
endif

ifeq ($(NO_NPTL),1)
DIRS := $(filter-out nptl,$(DIRS))
endif

ifeq ($(NO_TLS),1)
DIRS := $(filter-out tls,$(DIRS))
endif

ifeq ($(NO_DL),1)
DIRS := $(filter-out dlopen,$(DIRS))
endif

ifeq ($(NO_WCHAR),1)
DIRS := $(filter-out locale-mbwc,$(DIRS))
endif

ifeq ($(NO_LOCALE),1)
DIRS := $(filter-out locale,$(DIRS))
endif

ifeq ($(NO_MATH),1)
DIRS := $(filter-out math,$(DIRS))
endif

ifeq ($(NO_CRYPT),1)
DIRS := $(filter-out crypt,$(DIRS))
endif

ifeq ($(NO_MISC),1)
DIRS := $(filter-out misc,$(DIRS))
endif

ifeq ($(NO_ICONV),1)
DIRS := $(filter-out iconv,$(DIRS))
endif

ifeq ($(NO_PTHREAD),1)
DIRS := $(filter-out pthread,$(DIRS))
endif

test check all: run

run: subdirs_run

# A parallel make appends the runner's lines in whatever order the subdirs
# happen to finish, so the guest ran the suite in a different order every time
# -- which makes a run that dies partway impossible to compare with the one
# before it.  Sorting by subdir and then test name is stable across machines
# and jobserver settings, and groups the log the way it reads best.
# Fields are: expected_ret src binary subdir cmd.
gen:
	-rm -f $(top_builddir)/test/uclibcng-testrunner.in
	$(MAKE) run UCLIBCNG_GENERATE_TESTRUNNER=1
	LC_ALL=C sort -k4,4 -k3,3 \
		-o $(top_builddir)/test/uclibcng-testrunner.in \
		$(top_builddir)/test/uclibcng-testrunner.in

# The runner itself, built for the target because it runs in the guest.  It
# replaces uclibcng-testrunner{,_qemu,_color}.sh, which differed only in -r and
# -c; see the comment at the top of the source.  Built with the same CC and
# flags as the tests, so it links against the uClibc-ng under test -- the same
# exposure busybox already has in that rootfs.
#
# "make install" tars test/ excluding *.[ochmd], so this binary is carried into
# the rootfs and its source is not.
uclibcng-testrunner: uclibcng-testrunner.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $< $(LDFLAGS) $(EXTRA_LDFLAGS)

compile: uclibcng-testrunner subdirs_compile

clean: subdirs_clean
	-rm -f $(top_builddir)/test/uclibcng-testrunner

subdirs: $(patsubst %, _dir_%, $(DIRS))
subdirs_compile: $(patsubst %, _dircompile_%, $(DIRS))
subdirs_run: $(patsubst %, _dirrun_%, $(DIRS))
subdirs_clean: $(patsubst %, _dirclean_%, $(ALL_SUBDIRS))

$(patsubst %, _dir_%, $(DIRS)) : dummy
	$(Q)$(MAKE) -C $(patsubst _dir_%, %, $@) \
		KCONFIG_CONFIG=$(KCONFIG_CONFIG)

$(patsubst %, _dirrun_%, $(DIRS)) : dummy
	$(Q)$(MAKE) -C $(patsubst _dirrun_%, %, $@) run \
		UCLIBCNG_TEST_SUBDIR=$(strip $(patsubst _dirrun_%, %, $@))

$(patsubst %, _dircompile_%, $(DIRS)) : dummy
	$(Q)$(MAKE) -C $(patsubst _dircompile_%, %, $@) compile

$(patsubst %, _dirclean_%, $(ALL_SUBDIRS)) : dummy
	$(Q)$(MAKE) -C $(patsubst _dirclean_%, %, $@) clean

.PHONY: all check clean dummy subdirs_compile subdirs_run subdirs subdirs_clean test run compile
