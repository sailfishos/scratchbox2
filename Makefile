# Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
# Licensed under LGPL 2.1

TOPDIR = $(CURDIR)
OBJDIR = $(TOPDIR)
SRCDIR = $(TOPDIR)
VPATH = $(SRCDIR)

MAKEFILEDIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
ifneq ($(MAKEFILEDIR),$(OBJDIR))
SRCDIR:=$(MAKEFILEDIR)
endif

prefix = /usr/local
libdir = $(prefix)/lib
bindir = $(prefix)/bin
datarootdir = $(prefix)/share
datadir = $(datarootdir)

MACH := $(shell uname -m)
OS := $(shell uname -s)

ifeq ($(OS),Linux)
LIBSB2_LDFLAGS = -Wl,-soname=$(LIBSB2_SONAME) \
		-Wl,--version-script=$(OBJDIR)/preload/export.map

SHLIBEXT = so
else
SHLIBEXT = dylib
endif

CC = gcc
CXX = g++
LD = ld
PACKAGE_VERSION = 2.3.90

ifeq ($(shell if [ -d $(SRCDIR)/.git ]; then echo y; fi),y)
GIT_PV_COMMIT := $(shell git --git-dir=$(SRCDIR)/.git log -1 --pretty="format:%h" $(PACKAGE_VERSION) -- 2>/dev/null)
GIT_CUR_COMMIT := $(shell git --git-dir=$(SRCDIR)/.git log -1 --pretty="format:%h" HEAD -- 2>/dev/null)
GIT_MODIFIED := $(shell cd $(SRCDIR); git ls-files -m)
GIT_TAG_EXISTS := $(strip $(shell git --git-dir=$(SRCDIR)/.git tag -l $(PACKAGE_VERSION) 2>/dev/null))
ifeq ("$(GIT_TAG_EXISTS)","")
# Add -rc to version to signal that the PACKAGE_VERSION release has NOT
# been yet tagged
PACKAGE_VERSION := $(PACKAGE_VERSION)-rc
endif
ifneq ($(GIT_PV_COMMIT),$(GIT_CUR_COMMIT))
PACKAGE_VERSION := $(PACKAGE_VERSION)-$(GIT_CUR_COMMIT)
endif
ifneq ($(strip "$(GIT_MODIFIED)"),"")
PACKAGE_VERSION := $(PACKAGE_VERSION)-dirty
endif
endif
PACKAGE = "SB2"
LIBSB2_SONAME = "libsb2.so.1"
LLBUILD ?= $(SRCDIR)/llbuild
PROTOTYPEWARNINGS=-Wmissing-prototypes -Wstrict-prototypes

ifdef E
WERROR = -Wno-error
else
WERROR = -Werror
endif

# targets variable will be filled by llbuild
targets = 
subdirs = preload luaif sblib pathmapping execs network rule_tree utils sb2d wrappers

-include config.mak

CFLAGS += -O2 -g -Wall -W
CFLAGS += -I$(OBJDIR)/include -I$(SRCDIR)/include
CFLAGS += -D_GNU_SOURCE=1 -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1
CFLAGS += $(MACH_CFLAG)
LDFLAGS += $(MACH_CFLAG)
CXXFLAGS = 

# Uncomment following two lines to activate the "processclock" reports:
#CFLAGS += -DUSE_PROCESSCLOCK
#LDFLAGS += -lrt

include $(LLBUILD)/Makefile.include

ifdef prefix
CONFIGURE_ARGS = --prefix=$(prefix)
else
CONFIGURE_ARGS = 
endif

all: $(OBJDIR)/config.status .WAIT .version do-all

do-all: $(targets)

# Don't erase these files if make is interrupted while refreshing them.
.PRECIOUS: $(OBJDIR)/config.status
$(OBJDIR)/config.status: $(SRCDIR)/configure $(SRCDIR)/config.mak.in
	if [ -x $(OBJDIR)/config.status ]; then		\
	    $(OBJDIR)/config.status --recheck;	\
	else					\
	    $(SRCDIR)/configure; \
	fi

$(SRCDIR)/configure: $(SRCDIR)/configure.ac
	cd $(SRCDIR); \
	./autogen.sh

.PHONY: .version
.version:
	@(set -e; \
	if [ -f .version ]; then \
		version=$$(cat .version); \
		if [ "$(PACKAGE_VERSION)" != "$$version" ]; then \
			echo $(PACKAGE_VERSION) > .version; \
		fi \
	else \
		echo $(PACKAGE_VERSION) > .version; \
	fi)

$(OBJDIR)/include/scratchbox2_version.h: .version Makefile
	mkdir -p $(OBJDIR)/include
	echo "/* Automatically generated file. Do not edit. */" >$(OBJDIR)/include/scratchbox2_version.h
	echo '#define SCRATCHBOX2_VERSION "'`cat .version`'"' >>$(OBJDIR)/include/scratchbox2_version.h
	echo '#define LIBSB2_SONAME "'$(LIBSB2_SONAME)'"' >>$(OBJDIR)/include/scratchbox2_version.h

gcc_bins = addr2line ar as cc c++ c++filt cpp g++ gcc gcov gdb gdbtui gprof ld nm objcopy objdump ranlib rdi-stub readelf run size strings strip
host_prefixed_gcc_bins = $(foreach v,$(gcc_bins),host-$(v))

sb2_modes = emulate tools simple accel nomap emulate+toolchain emulate+toolchain+utils \
	    	obs-deb-install obs-deb-build \
	    	obs-rpm-install obs-rpm-build obs-rpm-build+pp
sb2_net_modes = localhost offline online online_privatenets

tarball:
	@git archive --format=tar --prefix=sbox2-$(PACKAGE_VERSION)/ $(PACKAGE_VERSION) | bzip2 >sbox2-$(PACKAGE_VERSION).tar.bz2


install-noarch: all
	$(P)INSTALL
	$(Q)install -d -m 755 $(DESTDIR)$(bindir)
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/scratchbox2/lua_scripts
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/scratchbox2/modes
	$(Q)(set -e; for d in $(sb2_modes); do \
		install -d -m 755 $(DESTDIR)$(datadir)/scratchbox2/modes/$$d; \
		for f in $(SRCDIR)/modes/$$d/*; do \
			install -m 644 $$f $(DESTDIR)$(datadir)/scratchbox2/modes/$$d; \
		done; \
	done)
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/scratchbox2/net_rules
	$(Q)(set -e; for d in $(sb2_net_modes); do \
		install -d -m 755 $(DESTDIR)$(datadir)/scratchbox2/net_rules/$$d; \
		for f in $(SRCDIR)/net_rules/$$d/*; do \
			install -m 644 $$f $(DESTDIR)$(datadir)/scratchbox2/net_rules/$$d; \
		done; \
	done)
	# "accel" == "devel" mode in 2.3.x:
	$(Q)ln -sf accel $(DESTDIR)$(datadir)/scratchbox2/modes/devel
	# Rule libraries
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/scratchbox2/rule_lib
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/scratchbox2/rule_lib/fs_rules
	$(Q)(set -e; for f in $(SRCDIR)/rule_lib/fs_rules/*; do \
		install -m 644 $$f $(DESTDIR)$(datadir)/scratchbox2/rule_lib/fs_rules; \
	done)
	# "scripts" and "wrappers" are visible to the user in some 
	# mapping modes, "lib" is for sb2's internal use
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/scratchbox2/lib
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/scratchbox2/scripts
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/scratchbox2/wrappers
	$(Q)install -d -m 755 $(DESTDIR)$(datadir)/scratchbox2/tests
	@if [ -d $(DESTDIR)$(datadir)/man/man1 ] ; \
	then echo "$(DESTDIR)$(datadir)/man/man1 present" ; \
	else install -d -m 755 $(DESTDIR)$(datadir)/man/man1 ; \
	fi
	@if [ -d $(DESTDIR)$(datadir)/man/man7 ] ; \
	then echo "$(DESTDIR)$(datadir)/man/man7 present" ; \
	else install -d -m 755 $(DESTDIR)$(datadir)/man/man7 ; \
	fi
	$(Q)echo "$(PACKAGE_VERSION)" > $(DESTDIR)$(datadir)/scratchbox2/version
	$(Q)install -m 755 $(SRCDIR)/utils/sb2 $(DESTDIR)$(bindir)/sb2
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-init $(DESTDIR)$(bindir)/sb2-init
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-config $(DESTDIR)$(bindir)/sb2-config
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-session $(DESTDIR)$(bindir)/sb2-session
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-build-libtool $(DESTDIR)$(bindir)/sb2-build-libtool
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-start-qemuserver $(DESTDIR)$(bindir)/sb2-start-qemuserver
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-qemu-gdbserver-prepare $(DESTDIR)$(bindir)/sb2-qemu-gdbserver-prepare

	$(Q)install -m 755 $(SRCDIR)/utils/sb2-cmp-checkbuilddeps-output.pl $(DESTDIR)$(datadir)/scratchbox2/lib/sb2-cmp-checkbuilddeps-output.pl

	$(Q)install -m 755 $(SRCDIR)/utils/sb2-upgrade-config $(DESTDIR)$(datadir)/scratchbox2/scripts/sb2-upgrade-config
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-parse-sb2-init-args $(DESTDIR)$(datadir)/scratchbox2/scripts/sb2-parse-sb2-init-args
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-config-gcc-toolchain $(DESTDIR)$(datadir)/scratchbox2/scripts/sb2-config-gcc-toolchain
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-config-debian $(DESTDIR)$(datadir)/scratchbox2/scripts/sb2-config-debian
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-check-pkg-mappings $(DESTDIR)$(datadir)/scratchbox2/scripts/sb2-check-pkg-mappings
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-exitreport $(DESTDIR)$(datadir)/scratchbox2/scripts/sb2-exitreport
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-generate-locales $(DESTDIR)$(datadir)/scratchbox2/scripts/sb2-generate-locales
	$(Q)install -m 755 $(SRCDIR)/utils/sb2-logz $(DESTDIR)$(bindir)/sb2-logz
	$(Q)install -m 644 $(SRCDIR)/lua_scripts/init*.lua $(DESTDIR)$(datadir)/scratchbox2/lua_scripts/
	$(Q)install -m 644 $(SRCDIR)/lua_scripts/rule_constants.lua $(DESTDIR)$(datadir)/scratchbox2/lua_scripts/
	$(Q)install -m 644 $(SRCDIR)/lua_scripts/exec_constants.lua $(DESTDIR)$(datadir)/scratchbox2/lua_scripts/
	$(Q)install -m 644 $(SRCDIR)/lua_scripts/argvenvp_gcc.lua $(DESTDIR)$(datadir)/scratchbox2/lua_scripts/argvenvp_gcc.lua
	$(Q)install -m 644 $(SRCDIR)/lua_scripts/argvenvp_misc.lua $(DESTDIR)$(datadir)/scratchbox2/lua_scripts/argvenvp_misc.lua
	$(Q)install -m 644 $(SRCDIR)/lua_scripts/create_reverse_rules.lua $(DESTDIR)$(datadir)/scratchbox2/lua_scripts/create_reverse_rules.lua
	$(Q)install -m 644 $(SRCDIR)/lua_scripts/argvmods_loader.lua $(DESTDIR)$(datadir)/scratchbox2/lua_scripts/argvmods_loader.lua
	$(Q)install -m 644 $(SRCDIR)/lua_scripts/add_rules_to_rule_tree.lua $(DESTDIR)$(datadir)/scratchbox2/lua_scripts/add_rules_to_rule_tree.lua

	$(Q)install -m 644 $(SRCDIR)/tests/* $(DESTDIR)$(datadir)/scratchbox2/tests
	$(Q)chmod a+x $(DESTDIR)$(datadir)/scratchbox2/tests/run.sh

	$(Q)install -m 644 $(SRCDIR)/docs/*.1 $(DESTDIR)$(datadir)/man/man1
	$(Q)install -m 644 $(OBJDIR)/preload/libsb2_interface.7 $(DESTDIR)$(datadir)/man/man7
	$(Q)rm -f $(DESTDIR)$(datadir)/scratchbox2/host_usr
	$(Q)ln -sf /usr $(DESTDIR)$(datadir)/scratchbox2/host_usr
	@# Wrappers:
	$(Q)install -m 755 $(SRCDIR)/wrappers/deb-pkg-tools-wrapper $(DESTDIR)$(datadir)/scratchbox2/wrappers/dpkg
	$(Q)install -m 755 $(SRCDIR)/wrappers/deb-pkg-tools-wrapper $(DESTDIR)$(datadir)/scratchbox2/wrappers/apt-get
	$(Q)install -m 755 $(SRCDIR)/wrappers/ldconfig $(DESTDIR)$(datadir)/scratchbox2/wrappers/ldconfig
	$(Q)install -m 755 $(SRCDIR)/wrappers/texi2html $(DESTDIR)$(datadir)/scratchbox2/wrappers/texi2html
	$(Q)install -m 755 $(SRCDIR)/wrappers/dpkg-checkbuilddeps $(DESTDIR)$(datadir)/scratchbox2/wrappers/dpkg-checkbuilddeps
	$(Q)install -m 755 $(SRCDIR)/wrappers/debconf2po-update $(DESTDIR)$(datadir)/scratchbox2/wrappers/debconf2po-update
	$(Q)install -m 755 $(SRCDIR)/wrappers/host-gcc-tools-wrapper $(DESTDIR)$(datadir)/scratchbox2/wrappers/host-gcc-tools-wrapper
	$(Q)install -m 755 $(SRCDIR)/wrappers/gdb $(DESTDIR)$(datadir)/scratchbox2/wrappers/gdb
	$(Q)install -m 755 $(SRCDIR)/wrappers/ldd $(DESTDIR)$(datadir)/scratchbox2/wrappers/ldd
	$(Q)install -m 755 $(SRCDIR)/wrappers/pwd $(DESTDIR)$(datadir)/scratchbox2/wrappers/pwd
	$(Q)(set -e; cd $(DESTDIR)$(datadir)/scratchbox2/wrappers; \
	for f in $(host_prefixed_gcc_bins); do \
		ln -sf host-gcc-tools-wrapper $$f; \
	done)

install: install-noarch
	$(P)INSTALL
	$(Q)install -d -m 755 $(DESTDIR)$(libdir)/libsb2
	$(Q)install -d -m 755 $(DESTDIR)$(libdir)/libsb2/wrappers
	$(Q)install -m 755 $(OBJDIR)/wrappers/fakeroot $(DESTDIR)$(libdir)/libsb2/wrappers/fakeroot
	$(Q)install -m 755 $(OBJDIR)/preload/libsb2.$(SHLIBEXT) $(DESTDIR)$(libdir)/libsb2/libsb2.so.$(PACKAGE_VERSION)
	$(Q)install -m 755 $(OBJDIR)/utils/sb2dctl $(DESTDIR)$(libdir)/libsb2/sb2dctl
	$(Q)install -m 755 $(OBJDIR)/utils/sb2-show $(DESTDIR)$(bindir)/sb2-show
	$(Q)install -m 755 $(OBJDIR)/utils/sb2-monitor $(DESTDIR)$(bindir)/sb2-monitor
	$(Q)install -m 755 $(OBJDIR)/sb2d/sb2d $(DESTDIR)$(bindir)/sb2d
ifeq ($(OS),Linux)
	$(Q)/sbin/ldconfig -n $(DESTDIR)$(libdir)/libsb2
endif

CLEAN_FILES += $(targets) config.status config.log

superclean: clean
	$(P)CLEAN
	$(Q)rm -rf include/config.h config.mak

clean:
	$(P)CLEAN
	$(Q)$(ll_clean)

