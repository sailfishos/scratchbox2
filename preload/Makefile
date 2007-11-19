objs := wrappers.o libsb2.o sb_exec.o

PROTOTYPEWARNINGS=-Wmissing-prototypes -Wstrict-prototypes

$(D)/libsb2.so: $(call O,$(objs))
$(D)/libsb2.so: luaif/libluaif.a luaif/liblua.a
$(D)/libsb2.so: CFLAGS := $(CFLAGS) -fPIC -Wall -W \
		$(PROTOTYPEWARNINGS)
$(D)/libsb2.so: LDFLAGS := $(LDFLAGS) -Wl,-soname=$(LIBSB2_SONAME) \
		-Wl,--retain-symbols-file=preload/ldexportlist
$(D)/libsb2.so: LIBS := -ldl -lm -lpthread -lrt

targets := $(targets) $(D)/libsb2.so

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
