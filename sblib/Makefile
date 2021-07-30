
objs := $(D)/sb_log.o \
	$(D)/processclock.o \
	$(D)/sb2_utils.o \
	$(D)/sb2_pthread_if.o

$(D)/sb_log.o: preload/exported.h

sblib/libsblib.a: $(objs)
sblib/libsblib.a: override CFLAGS := $(CFLAGS) -O2 -g -fPIC -Wall -W -I$(OBJDIR)/preload -I$(SRCDIR)/preload \
	$(WERROR) -Wmissing-prototypes -Wstrict-prototypes
sblib/libsblib.a: override LDFLAGS := $(LDFLAGS)
sblib/libsblib.a: override LIBS := 

sblib/sb_log.o: include/scratchbox2_version.h

targets := $(targets) $(D)/libsblib.a

