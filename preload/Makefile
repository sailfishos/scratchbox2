objs := wrappers.o privatewrappers.o \
	libsb2.o sb_l10n.o glob.o glob64.o \
	network.o \
	execgates.o \
	miscgates.o \
	tmpnamegates.o \
	vperm_filestatgates.o \
	vperm_uid_gid_gates.o \
	chrootgate.o \
	vperm_statfuncts.o \
	fdpathdb.o procfs.o mempcpy.o \
	union_dirs.o \
	system.o \
	sb2context.o

ifeq ($(shell uname -s),Linux)
LIBSB2_LDFLAGS = -Wl,-soname=$(LIBSB2_SONAME) \
		-Wl,--version-script=$(OBJDIR)preload/export.map

# DO NOT add any libraries that depend on libpthread to
# LIBSB2_LIBS or LIBS!! see the comments in luaif.c.

LIBSB2_LIBS = $(LUA_LIBS)
SHLIBEXT = so
else
LIBSB2_LIBS = $(LUA_LIBS)
SHLIBEXT = dylib
endif


$(D)/libsb2.$(SHLIBEXT): $(call O,$(objs))
# $(D)/libsb2.$(SHLIBEXT): luaif/libluaif.a luaif/liblua.a sblib/libsblib.a
$(D)/libsb2.$(SHLIBEXT): sblib/libsblib.a
$(D)/libsb2.$(SHLIBEXT): pathmapping/libpaths.a
$(D)/libsb2.$(SHLIBEXT): execs/libexecs.a
$(D)/libsb2.$(SHLIBEXT): network/libsb2net.a
$(D)/libsb2.$(SHLIBEXT): rule_tree/libruletree.a
$(D)/libsb2.$(SHLIBEXT): CFLAGS := $(CFLAGS) $(LUA_CFLAGS) -fPIC -Wall -W \
		-I$(SRCDIR)/preload -I$(OBJDIR)/preload $(PROTOTYPEWARNINGS)
$(D)/libsb2.$(SHLIBEXT): LDFLAGS := $(LDFLAGS) $(LIBSB2_LDFLAGS)
$(D)/libsb2.$(SHLIBEXT): LIBS := -ldl $(LIBSB2_LIBS)

targets := $(targets) $(D)/libsb2.$(SHLIBEXT)

$(D)/libsb2.o $(D)/sb_l10n.o: preload/exported.h
$(D)/exported.h $(D)/ldexportlist: preload/wrappers.c
.NOTPARALLEL: exported.h
$(D)/wrappers.c: preload/interface.master preload/gen-interface.pl
	$(MKOUTPUTDIR)
	$(P)PERL
	$(Q)$(SRCDIR)/preload/gen-interface.pl \
		-n public \
		-W preload/wrappers.c \
		-E preload/exported.h \
		-M preload/export.map \
		-V $(PACKAGE_VERSION) \
		-m preload/libsb2_interface.7 \
		<$(SRCDIR)/preload/interface.master

$(D)/privatewrappers.h: preload/privatewrappers.c
$(D)/privatewrappers.c: preload/interface.private preload/gen-interface.pl
	$(MKOUTPUTDIR)
	$(P)PERL
	$(Q)$(SRCDIR)/preload/gen-interface.pl \
		-n private \
		-W preload/privatewrappers.c \
		-E preload/privatewrappers.h \
		<$(SRCDIR)/preload/interface.private

generated := preload/wrappers.c preload/exported.h preload/ldexportlist
.PRECIOUS: $(generated)

CLEAN_FILES += $(generated)
