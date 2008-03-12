objs := wrappers.o libsb2.o sb_exec.o

ifeq ($(shell uname -s),Linux)
LIBSB2_LDFLAGS = -Wl,-soname=$(LIBSB2_SONAME) \
		-Wl,--retain-symbols-file=preload/ldexportlist

# DO NOT add any libraries that depend on libpthread to
# LIBSB2_LIBS or LIBS!! see the comments in luaif.c.

LIBSB2_LIBS =
SHLIBEXT = so
else
#LIBSB2_LDFLAGS = -dynamiclib
SHLIBEXT = dylib
endif


$(D)/libsb2.$(SHLIBEXT): $(call O,$(objs))
$(D)/libsb2.$(SHLIBEXT): luaif/libluaif.a luaif/liblua.a
$(D)/libsb2.$(SHLIBEXT): CFLAGS := $(CFLAGS) -fPIC -Wall -W \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS)
$(D)/libsb2.$(SHLIBEXT): LDFLAGS := $(LDFLAGS) $(LIBSB2_LDFLAGS)
$(D)/libsb2.$(SHLIBEXT): LIBS := -ldl -lm $(LIBSB2_LIBS)

targets := $(targets) $(D)/libsb2.$(SHLIBEXT)

$(D)/libsb2.o $(D)/sb_exec.o: preload/exported.h
$(D)/exported.h $(D)/ldexportlist: preload/wrappers.c
$(D)/wrappers.c: preload/interface.master preload/gen-interface.pl
	$(MKOUTPUTDIR)
	$(P)PERL
	$(Q)$(SRCDIR)/preload/gen-interface.pl \
		-W preload/wrappers.c \
		-E preload/exported.h \
		-L preload/ldexportlist \
		<$(SRCDIR)/preload/interface.master

generated := preload/wrappers.c preload/exported.h preload/ldexportlist
.PRECIOUS: $(generated)

CLEAN_FILES += $(generated)
