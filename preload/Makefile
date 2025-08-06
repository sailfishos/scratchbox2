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
		-Wl,--version-script=$(OBJDIR)/preload/export.map

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

$(D)/libsb2.o $(D)/sb_l10n.o: $(OBJDIR)/preload/exported.h
preloader_generated_public := \
	$(OBJDIR)/$(D)/wrappers.c \
	$(OBJDIR)/$(D)/exported.h \
	$(OBJDIR)/$(D)/export.map \
	$(OBJDIR)/$(D)/libsb2_interface.7

$(preloader_generated_public): preload/interface.master \
			preload/gen-interface.pl
	$(MKOUTPUTDIR)
	$(P)PERL
	$(Q)$(SRCDIR)/preload/gen-interface.pl \
		-n public \
		-W $(OBJDIR)/preload/wrappers.c \
		-E $(OBJDIR)/preload/exported.h \
		-M $(OBJDIR)/preload/export.map \
		-V $(PACKAGE_VERSION) \
		-m $(OBJDIR)/preload/libsb2_interface.7 \
		<$(SRCDIR)/preload/interface.master

preloader_generated_private := \
	$(OBJDIR)/$(D)/privatewrappers.h \
	$(OBJDIR)/$(D)/privatewrappers.c

$(preloader_generated_private): preload/interface.private \
			preload/gen-interface.pl
	$(MKOUTPUTDIR)
	$(P)PERL
	$(Q)$(SRCDIR)/preload/gen-interface.pl \
		-n private \
		-W $(OBJDIR)/preload/privatewrappers.c \
		-E $(OBJDIR)/preload/privatewrappers.h \
		<$(SRCDIR)/preload/interface.private

.PRECIOUS: $(preloader_generated_public) $(preloader_generated_private)

CLEAN_FILES += $(preloader_generated_public) $(preloader_generated_private)
