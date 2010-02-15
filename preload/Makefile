objs := wrappers.o privatewrappers.o \
	libsb2.o sb_exec.o sb_l10n.o glob.o glob64.o \
	network.o \
	execgates.o \
	miscgates.o \
	fdpathdb.o procfs.o mempcpy.o \
	system.o

ifeq ($(shell uname -s),Linux)
LIBSB2_LDFLAGS = -Wl,-soname=$(LIBSB2_SONAME) \
		-Wl,--version-script=preload/export.map

# DO NOT add any libraries that depend on libpthread to
# LIBSB2_LIBS or LIBS!! see the comments in luaif.c.

LIBSB2_LIBS =
SHLIBEXT = so
else
LIBSB2_LIBS =
SHLIBEXT = dylib
endif


$(D)/libsb2.$(SHLIBEXT): $(call O,$(objs))
$(D)/libsb2.$(SHLIBEXT): luaif/libluaif.a luaif/liblua.a
$(D)/libsb2.$(SHLIBEXT): CFLAGS := $(CFLAGS) -fPIC -Wall -W \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS)
$(D)/libsb2.$(SHLIBEXT): LDFLAGS := $(LDFLAGS) $(LIBSB2_LDFLAGS)
$(D)/libsb2.$(SHLIBEXT): LIBS := -ldl -lm $(LIBSB2_LIBS)

targets := $(targets) $(D)/libsb2.$(SHLIBEXT)

$(D)/libsb2.o $(D)/sb_l10n.o $(D)/sb_exec.o: preload/exported.h
$(D)/exported.h $(D)/ldexportlist: preload/wrappers.c
$(D)/wrappers.c: preload/interface.master preload/gen-interface.pl
	$(MKOUTPUTDIR)
	$(P)PERL
	$(Q)$(SRCDIR)/preload/gen-interface.pl \
		-W preload/wrappers.c \
		-E preload/exported.h \
		-M preload/export.map \
		<$(SRCDIR)/preload/interface.master

$(D)/privatewrappers.h: preload/privatewrappers.c
$(D)/privatewrappers.c: preload/interface.private preload/gen-interface.pl
	$(MKOUTPUTDIR)
	$(P)PERL
	$(Q)$(SRCDIR)/preload/gen-interface.pl \
		-W preload/privatewrappers.c \
		-E preload/privatewrappers.h \
		<$(SRCDIR)/preload/interface.private

generated := preload/wrappers.c preload/exported.h preload/ldexportlist
.PRECIOUS: $(generated)

CLEAN_FILES += $(generated)
