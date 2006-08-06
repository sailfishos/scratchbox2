CC = gcc-4.1.1
LD = ld
PACKAGE_VERSION = "1.99b"
PACKAGE = "SB2"
CFLAGS = -I$(TOPDIR)/include -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE

TOPDIR = $(CURDIR)

MAKEFILES = $(TOPDIR)/Makefile.include

export CC CFLAGS MAKEFILES

subdirs = lua preload utils
targets = utils/sb2init utils/chroot-uid

submodules:
	@set -e; \
	for d in $(subdirs); do $(MAKE) -C $$d ll_subdir; done


install: submodules
	install -d -m 755 $(prefix)/scratchbox/bin
	install -d -m 755 $(prefix)/scratchbox/lib
	install -d -m 755 $(prefix)/scratchbox/redir_scripts
	install -c -m 755 preload/libsb2.so $(prefix)/scratchbox/lib/libsb2.so
	install -c -m 755 utils/sb2init $(prefix)/scratchbox/bin/sb2init
	install -c -m 755 utils/chroot-uid $(prefix)/scratchbox/bin/chroot-uid
	install -c -m 755 scripts/login.sh $(prefix)/login.sh
	install -c -m 755 scripts/sb2rc $(prefix)/scratchbox/sb2rc
	install -c -m 644 redir_scripts/main.lua $(prefix)/scratchbox/redir_scripts/main.lua


CLEAN_FILES = $(targets)

clean:
	rm -rf $(CLEAN_FILES)
	find . -name "*.[oasd]" -o -name ".*.d" -o -name "*.*~" -o -name "*~" | xargs rm -rf

-include .config
include Makefile.include


