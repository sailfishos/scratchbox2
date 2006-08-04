CC = gcc-4.1.1
LD = ld
CFLAGS = -I$(TOPDIR)/include
TOPDIR = $(CURDIR)

MAKEFILES = $(TOPDIR)/Makefile.include

export CC CFLAGS MAKEFILES

subdirs = lua preload utils


submodules:
	@set -e; \
	for d in $(subdirs); do $(MAKE) -C $$d ll_subdir; done


CLEAN_FILES = $(targets)

clean:
	rm -rf $(CLEAN_FILES)
	find . -name "*.[oasd]" -o -name ".*.d" -o -name "*.*~" -o -name "*~" | xargs rm -rf

-include .config
include Makefile.include


