# Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
# Licensed under LGPL 2.1

TOPDIR = $(CURDIR)
OBJDIR = $(TOPDIR)
SRCDIR = $(TOPDIR)
VPATH = $(SRCDIR)


ifeq ($(shell uname -m),x86_64)
X86_64 = y
PRI_OBJDIR = obj-64
else
PRI_OBJDIR = obj-32
endif


CC = gcc
CXX = g++
LD = ld
PACKAGE_VERSION = 1.99.0.29

ifeq ($(shell if [ -d $(SRCDIR)/.git ]; then echo y; fi),y)
GIT_PV_COMMIT := $(shell git --git-dir=$(SRCDIR)/.git log -1 --pretty="format:%h" $(PACKAGE_VERSION) 2>/dev/null)
GIT_CUR_COMMIT := $(shell git --git-dir=$(SRCDIR)/.git log -1 --pretty="format:%h" HEAD 2>/dev/null)
ifneq ($(GIT_PV_COMMIT),$(GIT_CUR_COMMIT))
PACKAGE_VERSION := $(PACKAGE_VERSION)-$(GIT_CUR_COMMIT)
GIT_MODIFIED := $(shell cd $(SRCDIR); git ls-files -m)
ifneq ($(strip "$(GIT_MODIFIED)"),"")
PACKAGE_VERSION := $(PACKAGE_VERSION)-dirty
endif
endif
endif
PACKAGE = "SB2"
LIBSB2_SONAME = "libsb2.so.1"
LLBUILD ?= $(SRCDIR)/llbuild
PROTOTYPEWARNINGS=-Wmissing-prototypes -Wstrict-prototypes


# targets variable will be filled by llbuild
targets = 
subdirs = luaif preload utils

-include config.mak

CFLAGS += -O2 -g -Wall -W
CFLAGS += -I$(OBJDIR)/include -I$(SRCDIR)/include
CFLAGS += -I$(SRCDIR)/luaif/lua-5.1.4/src
CFLAGS += -D_GNU_SOURCE=1 -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1
CFLAGS += -DSCRATCHBOX_ROOT="$(prefix)"
CFLAGS += $(MACH_CFLAG)
LDFLAGS += $(MACH_CFLAG)
CXXFLAGS = 

include $(LLBUILD)/Makefile.include

ifdef prefix
CONFIGURE_ARGS = --prefix=$(prefix)
else
CONFIGURE_ARGS = 
endif

ifeq ($(X86_64),y)
all: multilib
else
all: regular
endif

do-all: $(targets)

.configure:
	$(SRCDIR)/configure $(CONFIGURE_ARGS)
	touch .configure

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

regular: .configure .version
	$(MAKE) -f $(SRCDIR)/Makefile --include-dir=$(SRCDIR) SRCDIR=$(SRCDIR) do-all

multilib:
	@mkdir -p obj-32
	@mkdir -p obj-64
	$(MAKE) MACH_CFLAG=-m32 -C obj-32 --include-dir=.. -f ../Makefile SRCDIR=.. regular
	$(MAKE) MACH_CFLAG=-m64 -C obj-64 --include-dir=.. -f ../Makefile SRCDIR=.. regular


gcc_bins = addr2line ar as cc c++ c++filt cpp g++ gcc gcov gdb gdbtui gprof ld nm objcopy objdump ranlib rdi-stub readelf run size strings strip
host_prefixed_gcc_bins = $(foreach v,$(gcc_bins),host-$(v))


tarball:
	git archive --format=tar --prefix=sbox2-$(PACKAGE_VERSION)/ $(PACKAGE_VERSION) | bzip2 >sbox2-$(PACKAGE_VERSION).tar.bz2


install-noarch: regular
	if [ -d $(prefix)/bin ] ; \
	then echo "$(prefix)/bin present" ; \
	else install -d -m 755 $(prefix)/bin ; \
	fi
	install -d -m 755 $(prefix)/share/scratchbox2/lua_scripts
	install -d -m 755 $(prefix)/share/scratchbox2/lua_scripts/pathmaps
	install -d -m 755 $(prefix)/share/scratchbox2/lua_scripts/pathmaps/emulate
	install -d -m 755 $(prefix)/share/scratchbox2/lua_scripts/pathmaps/tools
	install -d -m 755 $(prefix)/share/scratchbox2/lua_scripts/pathmaps/simple
	install -d -m 755 $(prefix)/share/scratchbox2/lua_scripts/pathmaps/devel
	install -d -m 755 $(prefix)/share/scratchbox2/lua_scripts/pathmaps/install

	install -d -m 755 $(prefix)/share/scratchbox2/scripts
	install -d -m 755 $(prefix)/share/scratchbox2/wrappers
	install -d -m 755 $(prefix)/share/scratchbox2/tests
	install -d -m 755 $(prefix)/share/scratchbox2/modeconf
	if [ -d $(prefix)/share/man/man1 ] ; \
	then echo "$(prefix)/share/man/man1 present" ; \
	else install -d -m 755 $(prefix)/share/man/man1 ; \
	fi
	echo "$(PACKAGE_VERSION)" > $(prefix)/share/scratchbox2/version
	install -c -m 755 $(SRCDIR)/utils/sb2 $(prefix)/bin/sb2
	install -c -m 755 $(SRCDIR)/utils/sb2-init $(prefix)/bin/sb2-init
	install -c -m 755 $(SRCDIR)/utils/sb2-config $(prefix)/bin/sb2-config
	install -c -m 755 $(SRCDIR)/utils/sb2-build-libtool $(prefix)/bin/sb2-build-libtool
	install -c -m 755 $(SRCDIR)/utils/sb2-build-qemuserver $(prefix)/bin/sb2-build-qemuserver
	install -c -m 755 $(SRCDIR)/utils/sb2-mkinitramfs $(prefix)/bin/sb2-mkinitramfs
	install -c -m 755 $(SRCDIR)/utils/sb2-start-qemuserver $(prefix)/bin/sb2-start-qemuserver
	install -c -m 755 $(SRCDIR)/utils/sb2-qemu-gdbserver-prepare $(prefix)/bin/sb2-qemu-gdbserver-prepare
	install -c -m 755 $(SRCDIR)/utils/sb2-check-pkg-mappings $(prefix)/share/scratchbox2/scripts/sb2-check-pkg-mappings
	install -c -m 755 $(SRCDIR)/utils/sb2-exitreport $(prefix)/share/scratchbox2/scripts/sb2-exitreport
	install -c -m 755 $(SRCDIR)/utils/sb2-generate-locales $(prefix)/share/scratchbox2/scripts/sb2-generate-locales
	install -c -m 755 $(SRCDIR)/utils/sb2-logz $(prefix)/bin/sb2-logz
	install -c -m 644 $(SRCDIR)/lua_scripts/main.lua $(prefix)/share/scratchbox2/lua_scripts/main.lua
	install -c -m 644 $(SRCDIR)/lua_scripts/mapping.lua $(prefix)/share/scratchbox2/lua_scripts/mapping.lua
	install -c -m 644 $(SRCDIR)/lua_scripts/argvenvp.lua $(prefix)/share/scratchbox2/lua_scripts/argvenvp.lua
	install -c -m 644 $(SRCDIR)/lua_scripts/argvenvp_gcc.lua $(prefix)/share/scratchbox2/lua_scripts/argvenvp_gcc.lua
	install -c -m 644 $(SRCDIR)/lua_scripts/create_reverse_rules.lua $(prefix)/share/scratchbox2/lua_scripts/create_reverse_rules.lua

	install -c -m 644 $(SRCDIR)/lua_scripts/pathmaps/emulate/*.lua $(prefix)/share/scratchbox2/lua_scripts/pathmaps/emulate/
	install -c -m 644 $(SRCDIR)/lua_scripts/pathmaps/tools/*.lua $(prefix)/share/scratchbox2/lua_scripts/pathmaps/tools/
	install -c -m 644 $(SRCDIR)/lua_scripts/pathmaps/simple/*.lua $(prefix)/share/scratchbox2/lua_scripts/pathmaps/simple/
	install -c -m 644 $(SRCDIR)/lua_scripts/pathmaps/devel/*.lua $(prefix)/share/scratchbox2/lua_scripts/pathmaps/devel/
	install -c -m 644 $(SRCDIR)/lua_scripts/pathmaps/install/*.lua $(prefix)/share/scratchbox2/lua_scripts/pathmaps/install/
	(cd $(prefix)/share/scratchbox2/lua_scripts/pathmaps; ln -sf devel maemo)

	install -c -m 644 $(SRCDIR)/modeconf/* $(prefix)/share/scratchbox2/modeconf/
	(cd $(prefix)/share/scratchbox2/modeconf; for f in *.devel; do \
		b=`basename $$f .devel`; ln -sf $$f $$b.maemo; \
	done)
	install -c -m 644 $(SRCDIR)/tests/* $(prefix)/share/scratchbox2/tests
	chmod a+x $(prefix)/share/scratchbox2/tests/run.sh

	install -c -m 644 $(SRCDIR)/docs/sb2.1 $(prefix)/share/man/man1/sb2.1
	install -c -m 644 $(SRCDIR)/docs/sb2-show.1 $(prefix)/share/man/man1/sb2-show.1
	install -c -m 644 $(SRCDIR)/docs/sb2-config.1 $(prefix)/share/man/man1/sb2-config.1
	rm -f $(prefix)/share/scratchbox2/host_usr
	ln -sf /usr $(prefix)/share/scratchbox2/host_usr
	# Wrappers:
	install -c -m 755 $(SRCDIR)/wrappers/deb-pkg-tools-wrapper $(prefix)/share/scratchbox2/wrappers/dpkg
	install -c -m 755 $(SRCDIR)/wrappers/deb-pkg-tools-wrapper $(prefix)/share/scratchbox2/wrappers/apt-get
	install -c -m 755 $(SRCDIR)/wrappers/ldconfig $(prefix)/share/scratchbox2/wrappers/ldconfig
	install -c -m 755 $(SRCDIR)/wrappers/texi2html $(prefix)/share/scratchbox2/wrappers/texi2html
	install -c -m 755 $(SRCDIR)/wrappers/dpkg-checkbuilddeps $(prefix)/share/scratchbox2/wrappers/dpkg-checkbuilddeps
	install -c -m 755 $(SRCDIR)/wrappers/debconf2po-update $(prefix)/share/scratchbox2/wrappers/debconf2po-update
	install -c -m 755 $(SRCDIR)/wrappers/host-gcc-tools-wrapper $(prefix)/share/scratchbox2/wrappers/host-gcc-tools-wrapper
	install -c -m 755 $(SRCDIR)/wrappers/gdb $(prefix)/share/scratchbox2/wrappers/gdb
	install -c -m 755 $(SRCDIR)/wrappers/ldd $(prefix)/share/scratchbox2/wrappers/ldd
	(cd $(prefix)/share/scratchbox2/wrappers; \
	 for f in $(host_prefixed_gcc_bins); do \
		ln -sf host-gcc-tools-wrapper $$f; \
	done)

ifeq ($(X86_64),y)
install: install-multilib
else
install: do-install
endif

do-install: install-noarch
	if [ -d $(prefix)/lib ] ; \
	then echo "$(prefix)/lib present" ; \
	else install -d -m 755 $(prefix)/lib ; \
	fi
	install -d -m 755 $(prefix)/lib/libsb2
	install -c -m 755 $(OBJDIR)/preload/libsb2.so $(prefix)/lib/libsb2/libsb2.so.$(PACKAGE_VERSION)
	install -c -m 755 $(OBJDIR)/utils/sb2-show $(prefix)/bin/sb2-show
	install -c -m 755 $(OBJDIR)/utils/sb2-monitor $(prefix)/bin/sb2-monitor
	install -c -m 755 $(OBJDIR)/utils/sb2-interp-wrapper $(prefix)/bin/sb2-interp-wrapper
	/sbin/ldconfig -n $(prefix)/lib/libsb2


multilib_prefix=$(prefix)

install-multilib: multilib
	$(MAKE) -C obj-32 --include-dir=.. -f ../Makefile SRCDIR=.. do-install-multilib bitness=32
	$(MAKE) -C obj-64 --include-dir=.. -f ../Makefile SRCDIR=.. do-install

do-install-multilib:
	if [ -d $(multilib_prefix)/lib$(bitness) ] ; \
	then echo "$(prefix)/lib$(bitness) present" ; \
	else install -d -m 755 $(prefix)/lib$(bitness) ; \
	fi
	install -d -m 755 $(multilib_prefix)/lib$(bitness)/libsb2
	install -c -m 755 preload/libsb2.so $(multilib_prefix)/lib$(bitness)/libsb2/libsb2.so.$(PACKAGE_VERSION)
	/sbin/ldconfig -n $(multilib_prefix)/lib$(bitness)/libsb2


CLEAN_FILES += $(targets) config.status config.log

superclean: clean
	rm -rf obj-32 obj-64 .configure-multilib .configure
	rm -rf include/config.h config.mak

clean-multilib:
	-$(MAKE) -C obj-32 --include-dir=.. -f ../Makefile SRCDIR=.. do-clean
	-$(MAKE) -C obj-64 --include-dir .. -f ../Makefile SRCDIR=.. do-clean

ifeq ($(X86_64),y)
clean: clean-multilib
else
clean: do-clean
endif

do-clean:
	$(ll_clean)

