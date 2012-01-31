# Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
# Licensed under LGPL 2.1

TOPDIR = $(CURDIR)
OBJDIR = $(TOPDIR)
SRCDIR = $(TOPDIR)
VPATH = $(SRCDIR)

MACH := $(shell uname -m)
OS := $(shell uname -s)

ifeq ($(OS),Linux)
LIBSB2_LDFLAGS = -Wl,-soname=$(LIBSB2_SONAME) \
		-Wl,--version-script=preload/export.map

SHLIBEXT = so
else
SHLIBEXT = dylib
endif

# pick main build bitness
ifeq ($(MACH),x86_64)
PRI_OBJDIR = obj-64
else
PRI_OBJDIR = obj-32
endif

CC = gcc
CXX = g++
LD = ld
PACKAGE_VERSION = 2.3.25

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


# targets variable will be filled by llbuild
targets = 
subdirs = luaif sblib pathmapping execs rule_tree preload utils sb2d

-include config.mak

CFLAGS += -O2 -g -Wall -W
CFLAGS += -I$(OBJDIR)/include -I$(SRCDIR)/include
CFLAGS += -I$(SRCDIR)/luaif/lua-5.1.4/src
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

ifeq ($(MACH),x86_64)
all: multilib
else
all: regular
endif

do-all: $(targets)

.configure:
	$(SRCDIR)/configure $(CONFIGURE_ARGS)
	@touch .configure

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

include/scratchbox2_version.h: .version Makefile
	echo "/* Automatically generated file. Do not edit. */" >include/scratchbox2_version.h
	echo '#define SCRATCHBOX2_VERSION "'`cat .version`'"' >>include/scratchbox2_version.h
	echo '#define LIBSB2_SONAME "'$(LIBSB2_SONAME)'"' >>include/scratchbox2_version.h

regular: .configure .version
	@$(MAKE) -f $(SRCDIR)/Makefile --include-dir=$(SRCDIR) SRCDIR=$(SRCDIR) do-all

multilib:
	@mkdir -p obj-32
	@mkdir -p obj-64
	@$(MAKE) MACH_CFLAG=-m32 -C obj-32 --include-dir=.. -f ../Makefile SRCDIR=.. regular
	@$(MAKE) MACH_CFLAG=-m64 -C obj-64 --include-dir=.. -f ../Makefile SRCDIR=.. regular


gcc_bins = addr2line ar as cc c++ c++filt cpp g++ gcc gcov gdb gdbtui gprof ld nm objcopy objdump ranlib rdi-stub readelf run size strings strip
host_prefixed_gcc_bins = $(foreach v,$(gcc_bins),host-$(v))

sb2_modes = emulate tools simple accel nomap emulate+toolchain emulate+toolchain+utils \
	    	obs-rpm-install obs-rpm-build obs-rpm-build+pp
sb2_net_modes = localhost offline online online_privatenets

tarball:
	@git archive --format=tar --prefix=sbox2-$(PACKAGE_VERSION)/ $(PACKAGE_VERSION) | bzip2 >sbox2-$(PACKAGE_VERSION).tar.bz2


install-noarch: regular
	$(P)INSTALL
	@if [ -d $(prefix)/bin ] ; \
	then echo "$(prefix)/bin present" ; \
	else install -d -m 755 $(prefix)/bin ; \
	fi
	$(Q)install -d -m 755 $(prefix)/share/scratchbox2/lua_scripts
	$(Q)install -d -m 755 $(prefix)/share/scratchbox2/modes
	$(Q)(set -e; for d in $(sb2_modes); do \
		install -d -m 755 $(prefix)/share/scratchbox2/modes/$$d; \
		for f in $(SRCDIR)/modes/$$d/*; do \
			install -c -m 644 $$f $(prefix)/share/scratchbox2/modes/$$d; \
		done; \
	done)
	$(Q)install -d -m 755 $(prefix)/share/scratchbox2/net_rules
	$(Q)(set -e; for d in $(sb2_net_modes); do \
		install -d -m 755 $(prefix)/share/scratchbox2/net_rules/$$d; \
		for f in $(SRCDIR)/net_rules/$$d/*; do \
			install -c -m 644 $$f $(prefix)/share/scratchbox2/net_rules/$$d; \
		done; \
	done)
	# "accel" == "devel" mode in 2.3.x:
	$(Q)ln -sf accel $(prefix)/share/scratchbox2/modes/devel

	# "scripts" and "wrappers" are visible to the user in some 
	# mapping modes, "lib" is for sb2's internal use
	$(Q)install -d -m 755 $(prefix)/share/scratchbox2/lib
	$(Q)install -d -m 755 $(prefix)/share/scratchbox2/scripts
	$(Q)install -d -m 755 $(prefix)/share/scratchbox2/wrappers
	$(Q)install -d -m 755 $(prefix)/share/scratchbox2/tests
	@if [ -d $(prefix)/share/man/man1 ] ; \
	then echo "$(prefix)/share/man/man1 present" ; \
	else install -d -m 755 $(prefix)/share/man/man1 ; \
	fi
	$(Q)echo "$(PACKAGE_VERSION)" > $(prefix)/share/scratchbox2/version
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2 $(prefix)/bin/sb2
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-init $(prefix)/bin/sb2-init
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-config $(prefix)/bin/sb2-config
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-session $(prefix)/bin/sb2-session
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-build-libtool $(prefix)/bin/sb2-build-libtool
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-build-qemuserver $(prefix)/bin/sb2-build-qemuserver
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-mkinitramfs $(prefix)/bin/sb2-mkinitramfs
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-start-qemuserver $(prefix)/bin/sb2-start-qemuserver
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-qemu-gdbserver-prepare $(prefix)/bin/sb2-qemu-gdbserver-prepare

	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-cmp-checkbuilddeps-output.pl $(prefix)/share/scratchbox2/lib/sb2-cmp-checkbuilddeps-output.pl

	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-upgrade-config $(prefix)/share/scratchbox2/scripts/sb2-upgrade-config
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-parse-sb2-init-args $(prefix)/share/scratchbox2/scripts/sb2-parse-sb2-init-args
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-config-gcc-toolchain $(prefix)/share/scratchbox2/scripts/sb2-config-gcc-toolchain
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-config-debian $(prefix)/share/scratchbox2/scripts/sb2-config-debian
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-check-pkg-mappings $(prefix)/share/scratchbox2/scripts/sb2-check-pkg-mappings
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-exitreport $(prefix)/share/scratchbox2/scripts/sb2-exitreport
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-generate-locales $(prefix)/share/scratchbox2/scripts/sb2-generate-locales
	$(Q)install -c -m 755 $(SRCDIR)/utils/sb2-logz $(prefix)/bin/sb2-logz
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/init.lua $(prefix)/share/scratchbox2/lua_scripts/init.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/init_argvmods_rules.lua $(prefix)/share/scratchbox2/lua_scripts/init_argvmods_rules.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/main.lua $(prefix)/share/scratchbox2/lua_scripts/main.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/mapping.lua $(prefix)/share/scratchbox2/lua_scripts/mapping.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/network.lua $(prefix)/share/scratchbox2/lua_scripts/network.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/argvenvp.lua $(prefix)/share/scratchbox2/lua_scripts/argvenvp.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/argvenvp_gcc.lua $(prefix)/share/scratchbox2/lua_scripts/argvenvp_gcc.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/argvenvp_misc.lua $(prefix)/share/scratchbox2/lua_scripts/argvenvp_misc.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/create_reverse_rules.lua $(prefix)/share/scratchbox2/lua_scripts/create_reverse_rules.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/create_argvmods_rules.lua $(prefix)/share/scratchbox2/lua_scripts/create_argvmods_rules.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/argvmods_loader.lua $(prefix)/share/scratchbox2/lua_scripts/argvmods_loader.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/create_argvmods_usr_bin_rules.lua $(prefix)/share/scratchbox2/lua_scripts/create_argvmods_usr_bin_rules.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/add_rules_to_rule_tree.lua $(prefix)/share/scratchbox2/lua_scripts/add_rules_to_rule_tree.lua
	$(Q)install -c -m 644 $(SRCDIR)/lua_scripts/add_config_to_rule_tree.lua $(prefix)/share/scratchbox2/lua_scripts/add_config_to_rule_tree.lua

	$(Q)install -c -m 644 $(SRCDIR)/tests/* $(prefix)/share/scratchbox2/tests
	$(Q)chmod a+x $(prefix)/share/scratchbox2/tests/run.sh

	$(Q)install -c -m 644 $(SRCDIR)/docs/*.1 $(prefix)/share/man/man1
	$(Q)rm -f $(prefix)/share/scratchbox2/host_usr
	$(Q)ln -sf /usr $(prefix)/share/scratchbox2/host_usr
	@# Wrappers:
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/deb-pkg-tools-wrapper $(prefix)/share/scratchbox2/wrappers/dpkg
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/deb-pkg-tools-wrapper $(prefix)/share/scratchbox2/wrappers/apt-get
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/ldconfig $(prefix)/share/scratchbox2/wrappers/ldconfig
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/texi2html $(prefix)/share/scratchbox2/wrappers/texi2html
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/dpkg-checkbuilddeps $(prefix)/share/scratchbox2/wrappers/dpkg-checkbuilddeps
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/debconf2po-update $(prefix)/share/scratchbox2/wrappers/debconf2po-update
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/host-gcc-tools-wrapper $(prefix)/share/scratchbox2/wrappers/host-gcc-tools-wrapper
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/gdb $(prefix)/share/scratchbox2/wrappers/gdb
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/ldd $(prefix)/share/scratchbox2/wrappers/ldd
	$(Q)install -c -m 755 $(SRCDIR)/wrappers/pwd $(prefix)/share/scratchbox2/wrappers/pwd
	$(Q)(set -e; cd $(prefix)/share/scratchbox2/wrappers; \
	for f in $(host_prefixed_gcc_bins); do \
		ln -sf host-gcc-tools-wrapper $$f; \
	done)

ifeq ($(MACH),x86_64)
install: install-multilib
else
install: do-install
endif

do-install: install-noarch
	$(P)INSTALL
	@if [ -d $(prefix)/lib ] ; \
	then echo "$(prefix)/lib present" ; \
	else install -d -m 755 $(prefix)/lib ; \
	fi
	$(Q)install -d -m 755 $(prefix)/lib/libsb2
	$(Q)install -c -m 755 $(OBJDIR)/preload/libsb2.$(SHLIBEXT) $(prefix)/lib/libsb2/libsb2.so.$(PACKAGE_VERSION)
	$(Q)install -c -m 755 $(OBJDIR)/utils/sb2-show $(prefix)/bin/sb2-show
	$(Q)install -c -m 755 $(OBJDIR)/utils/sb2-monitor $(prefix)/bin/sb2-monitor
	$(Q)install -c -m 755 $(OBJDIR)/sb2d/sb2d $(prefix)/bin/sb2d
ifeq ($(OS),Linux)
	$(Q)/sbin/ldconfig -n $(prefix)/lib/libsb2
endif

multilib_prefix=$(prefix)

install-multilib: multilib
	@$(MAKE) -C obj-32 --include-dir=.. -f ../Makefile SRCDIR=.. do-install-multilib bitness=32
	@$(MAKE) -C obj-64 --include-dir=.. -f ../Makefile SRCDIR=.. do-install

do-install-multilib:
	$(P)INSTALL
	@if [ -d $(multilib_prefix)/lib$(bitness) ] ; \
	then echo "$(prefix)/lib$(bitness) present" ; \
	else install -d -m 755 $(prefix)/lib$(bitness) ; \
	fi
	$(Q)install -d -m 755 $(multilib_prefix)/lib$(bitness)/libsb2
	$(Q)install -c -m 755 preload/libsb2.$(SHLIBEXT) $(multilib_prefix)/lib$(bitness)/libsb2/libsb2.so.$(PACKAGE_VERSION)
ifeq ($(OS),Linux)
	$(Q)/sbin/ldconfig -n $(multilib_prefix)/lib$(bitness)/libsb2
endif

CLEAN_FILES += $(targets) config.status config.log

superclean: clean
	$(P)CLEAN
	$(Q)rm -rf obj-32 obj-64 .configure-multilib .configure
	$(Q)rm -rf include/config.h config.mak

clean-multilib:
	$(P)CLEAN
	-$(Q)$(MAKE) -C obj-32 --include-dir=.. -f ../Makefile SRCDIR=.. do-clean
	-$(Q)$(MAKE) -C obj-64 --include-dir .. -f ../Makefile SRCDIR=.. do-clean

ifeq ($(MACH),x86_64)
clean: clean-multilib do-clean
else
clean: do-clean
endif

do-clean:
	$(P)CLEAN
	$(Q)$(ll_clean)

