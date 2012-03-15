
objs := $(D)/exec_ruletree_maint.o \
	$(D)/exec_preprocess.o \
	$(D)/exec_policy_selection.o \
	$(D)/exec_map_script_interp.o \
	$(D)/exec_policy_ruletree.o \
	$(D)/sb_exec.o

$(D)/sb_exec.o: preload/exported.h

execs/libexecs.a: $(objs)
execs/libexecs.a: override CFLAGS := $(CFLAGS) -O2 -g -fPIC -Wall -W -I$(SRCDIR)/$(LUASRC) -I$(OBJDIR)/preload -I$(SRCDIR)/preload -I$(SRCDIR)/execs
execs/libexecs.a: override LDFLAGS := $(LDFLAGS)
execs/libexecs.a: override LIBS := 

targets := $(targets) $(D)/libexecs.a

