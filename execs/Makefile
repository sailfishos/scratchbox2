
objs := $(D)/exec_ruletree_maint.o \
	$(D)/exec_preprocess.o \
	$(D)/exec_policy_selection.o \
	$(D)/exec_map_script_interp.o \
	$(D)/exec_policy_ruletree.o \
	$(D)/exec_postprocess.o \
	$(D)/sb_exec.o

$(D)/sb_exec.o: preload/exported.h

execs/libexecs.a: $(objs)
execs/libexecs.a: override CFLAGS := $(CFLAGS) $(LUA_CFLAGS) -O2 -g -fPIC -Wall -W -I$(SRCDIR)/$(LUASRC) -I$(OBJDIR)/preload -I$(SRCDIR)/preload -I$(SRCDIR)/execs \
	$(WERROR) -Wmissing-prototypes -Wstrict-prototypes
execs/libexecs.a: override LDFLAGS := $(LDFLAGS)
execs/libexecs.a: override LIBS := 

targets := $(targets) $(D)/libexecs.a

