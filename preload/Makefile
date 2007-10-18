objs := wrappers.o libsb2.o sb_exec.o

PROTOTYPEWARNINGS=-Wmissing-prototypes -Wstrict-prototypes

$(D)/libsb2.so: $(call O,$(objs))

$(D)/libsb2.so: mapping/libmapping.a mapping/liblua.a

$(D)/libsb2.so: CFLAGS := $(CFLAGS) -fPIC -Wall -W $(PROTOTYPEWARNINGS)
$(D)/libsb2.so: LDFLAGS := $(LDFLAGS) -Wl,-soname=$(LIBSB2_SONAME) -Wl,--retain-symbols-file=preload/generated/ldexportlist
$(D)/libsb2.so: LIBS := -ldl -lm -lpthread -lrt

targets := $(targets) $(D)/libsb2.so


# "exported.h" will be created by the interface generator, too.
preload/generated/exported.h: preload/wrappers.o

preload/wrappers.o: preload/interface.master preload/gen-interface.pl
	if [ ! -d preload/generated ]; then mkdir preload/generated; fi
	echo "Expect to see one warning (about SCANDIR_TYPE_ARG3)"
	preload/gen-interface.pl \
		-W preload/generated/wrappers.c \
		-E preload/generated/exported.h \
		-L preload/generated/ldexportlist \
		<preload/interface.master
	$(CC) -I. -Ipreload -Iinclude -o preload/wrappers.o -c \
		$(PROTOTYPEWARNINGS) \
		preload/generated/wrappers.c
