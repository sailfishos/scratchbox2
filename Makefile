CC = gcc
CXX = g++
LD = ld
PACKAGE_VERSION = "1.99.0.10"
PACKAGE = "SB2"
LIBSB2_SONAME = "libsb2.so.1"
CFLAGS = -O2 -g -Wall -W -I./include -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1
CFLAGS += -DSCRATCHBOX_ROOT="$(prefix)"
CXXFLAGS = 
TOPDIR = $(CURDIR)
LLBUILD ?= $(TOPDIR)/llbuild

export CC CFLAGS CXX CXXFLAGS TOPDIR LLBUILD

# targets variable will be filled by llbuild
targets = 
subdirs = mapping preload utils

include config.mak
include $(LLBUILD)/Makefile.include

ifdef prefix
CONFIGURE_ARGS = --prefix=$(prefix)
else
CONFIGURE_ARGS = 
endif

all: $(targets)

gcc_bins = addr2line ar as cc c++ c++filt cpp g++ gcc gcov gdb gdbtui gprof ld nm objcopy objdump ranlib rdi-stub readelf run size strings strip
gcc_bins_expanded = $(foreach v,$(gcc_bins),$(prefix)/bin/host-$(v))

config.mak: configure
	./configure $(CONFIGURE_ARGS)

configure: configure.ac autogen.sh
	./autogen.sh

sources-release:
	git archive --format=tar --prefix=sbox2-$(PACKAGE_VERSION)/ $(PACKAGE_VERSION) | bzip2 >sbox2-$(PACKAGE_VERSION).tar.bz2

install: $(targets)
	install -d -m 755 $(prefix)/bin
	install -d -m 755 $(prefix)/lib
	install -d -m 755 $(prefix)/lib/libsb2
	install -d -m 755 $(prefix)/share/scratchbox2/redir_scripts
	install -d -m 755 $(prefix)/share/scratchbox2/redir_scripts/preload
	install -d -m 755 $(prefix)/share/scratchbox2/redir_scripts/preload/default
	install -d -m 755 $(prefix)/share/scratchbox2/redir_scripts/preload/emulate
	install -d -m 755 $(prefix)/share/scratchbox2/scripts
	install -d -m 755 $(prefix)/share/man/man1
	install -c -m 755 preload/libsb2.so $(prefix)/lib/libsb2/libsb2.so.$(PACKAGE_VERSION)
	install -c -m 755 utils/sb2 $(prefix)/bin/sb2
	install -c -m 755 utils/sb2-init $(prefix)/bin/sb2-init
	install -c -m 755 utils/sb2-build-libtool $(prefix)/bin/sb2-build-libtool
	install -c -m 755 utils/dpkg-checkbuilddeps $(prefix)/share/scratchbox2/scripts/dpkg-checkbuilddeps
	install -c -m 755 utils/sb_gcc_wrapper $(prefix)/bin/sb_gcc_wrapper
	install -c -m 644 redir_scripts/main.lua $(prefix)/share/scratchbox2/redir_scripts/main.lua
	install -c -m 644 redir_scripts/preload/default/*.lua $(prefix)/share/scratchbox2/redir_scripts/preload/default/
	install -c -m 644 redir_scripts/preload/emulate/*.lua $(prefix)/share/scratchbox2/redir_scripts/preload/emulate/
	install -c -m 644 docs/sb2.1 $(prefix)/share/man/man1/sb2.1
	@for f in $(gcc_bins_expanded); do \
		ln -sf sb_gcc_wrapper $$f; \
	done
	rm -f $(prefix)/share/scratchbox2/host_usr
	ln -sf /usr $(prefix)/share/scratchbox2/host_usr
	/sbin/ldconfig -n $(prefix)/lib/libsb2

CLEAN_FILES = $(targets) config.status config.log

# make all object files depend on include/config.h

clean:
	$(ll_clean)

