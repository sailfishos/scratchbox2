
objs := $(D)/exec_ruletree_maint.o \
	$(D)/sb_exec.o

$(D)/sb_exec.o: preload/exported.h

execs/libexecs.a: $(objs)
execs/libexecs.a: override CFLAGS := $(CFLAGS) -O2 -g -fPIC -Wall -W -I$(SRCDIR)/$(LUASRC) -I$(OBJDIR)/preload -I$(SRCDIR)/preload -I$(SRCDIR)/execs
execs/libexecs.a: override LDFLAGS := $(LDFLAGS)
execs/libexecs.a: override LIBS := 

targets := $(targets) $(D)/libexecs.a

