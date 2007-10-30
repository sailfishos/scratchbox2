objs := wrappers.o libsb2.o sb_exec.o

PROTOTYPEWARNINGS=-Wmissing-prototypes -Wstrict-prototypes

$(D)/libsb2.so: $(call O,$(objs))
$(D)/libsb2.so: mapping/libmapping.a mapping/liblua.a
$(D)/libsb2.so: CFLAGS := $(CFLAGS) -fPIC -Wall -W \
		$(PROTOTYPEWARNINGS)
$(D)/libsb2.so: LDFLAGS := $(LDFLAGS) -Wl,-soname=$(LIBSB2_SONAME) \
		-Wl,--retain-symbols-file=$(D)/ldexportlist
$(D)/libsb2.so: LIBS := -ldl -lm -lpthread -lrt

targets := $(targets) $(D)/libsb2.so

preload/wrappers.c: preload/interface.master preload/gen-interface.pl
	$(P)PERL
	$(Q)preload/gen-interface.pl \
		-W preload/wrappers.c \
		-E preload/exported.h \
		-L preload/ldexportlist \
		<preload/interface.master

generated = preload/wrappers.c preload/exported.h preload/ldexportlist
.PRECIOUS: $(generated)

CLEAN_FILES += $(generated)
