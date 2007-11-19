objs := wrappers.o libsb2.o sb_exec.o

PROTOTYPEWARNINGS=-Wmissing-prototypes -Wstrict-prototypes


ifeq ($(shell uname -s),Linux)
LIBSB2_LDFLAGS = -Wl,-soname=$(LIBSB2_SONAME) \
		-Wl,--retain-symbols-file=preload/ldexportlist
LIBSB2_LIBS = -lrt
SHLIBEXT = dylib
else
#LIBSB2_LDFLAGS = -dynamiclib
SHLIBEXT = so
endif


$(D)/libsb2.$(SHLIBEXT): $(call O,$(objs))
$(D)/libsb2.$(SHLIBEXT): luaif/libluaif.a luaif/liblua.a
$(D)/libsb2.$(SHLIBEXT): CFLAGS := $(CFLAGS) -fPIC -Wall -W \
		$(PROTOTYPEWARNINGS)
$(D)/libsb2.$(SHLIBEXT): LDFLAGS := $(LDFLAGS) $(LIBSB2_LDFLAGS)
$(D)/libsb2.$(SHLIBEXT): LIBS := -ldl -lm -lpthread $(LIBSB2_LIBS)

targets := $(targets) $(D)/libsb2.$(SHLIBEXT)

$(D)/libsb2.o $(D)/sb_exec.o: preload/exported.h
$(D)/exported.h $(D)/ldexportlist: preload/wrappers.c
$(D)/wrappers.c: preload/interface.master preload/gen-interface.pl
	$(P)PERL
	$(Q)preload/gen-interface.pl \
		-W preload/wrappers.c \
		-E preload/exported.h \
		-L preload/ldexportlist \
		<preload/interface.master

generated := preload/wrappers.c preload/exported.h preload/ldexportlist
.PRECIOUS: $(generated)

CLEAN_FILES += $(generated)
