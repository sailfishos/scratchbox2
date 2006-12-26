CC = gcc
CXX = g++
LD = ld
PACKAGE_VERSION = "1.99b"
PACKAGE = "SB2"
CFLAGS = -Wall -W -I$(TOPDIR)/include -D_LARGEFILE_SOURCE=1 -D_LARGEFILE64_SOURCE=1
CFLAGS += -DSCRATCHBOX_ROOT="$(prefix)"
TOPDIR = $(CURDIR)
LLBUILD = $(TOPDIR)/llbuild

MAKEFILES = Makefile $(LLBUILD)/Makefile.include

export CC CFLAGS MAKEFILES TOPDIR LLBUILD

subdirs = lua preload utils
targets = utils/sb_gcc_wrapper


all: build

configure: configure.ac
	./autogen.sh
	./configure

build: configure
	$(ll_toplevel_build)
	@echo Build completed successfully!

gcc_bins = addr2line,ar,as,cc,c++,c++filt,cpp,g++,gcc,gcov,gdb,gdbtui,gprof,ld,nm,objcopy,objdump,ranlib,rdi-stub,readelf,run,size,strings,strip

install: build
	install -d -m 755 $(prefix)/bin
	install -d -m 755 $(prefix)/lib
	install -d -m 755 $(prefix)/share/scratchbox2/redir_scripts
	install -d -m 755 $(prefix)/share/scratchbox2/redir_scripts/parts
	install -c -m 755 preload/libsb2.so $(prefix)/lib/libsb2.so
	install -c -m 755 utils/sb2 $(prefix)/bin/sb2
	install -c -m 755 utils/sb_gcc_wrapper $(prefix)/bin/sb_gcc_wrapper
	install -c -m 755 scripts/sb2rc $(prefix)/share/scratchbox2/sb2rc
	install -c -m 644 redir_scripts/main.lua $(prefix)/share/scratchbox2/redir_scripts/main.lua
	install -c -m 644 redir_scripts/parts/default.lua $(prefix)/share/scratchbox2/redir_scripts/parts/default.lua
	install -c -m 644 etc/sb2.config.sample $(prefix)/share/scratchbox2/sb2.config.sample
	install -c -m 644 etc/host-gcc.specs $(prefix)/share/scratchbox2/host-gcc.specs
	@for f in $(prefix)/bin/host-{$(gcc_bins)}; do \
		ln -sf sb_gcc_wrapper $$f; \
	done

CLEAN_FILES = $(targets)

clean:
	$(ll_clean)

-include .config
include $(LLBUILD)/Makefile.include


