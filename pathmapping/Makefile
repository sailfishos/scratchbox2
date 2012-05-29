
objs := $(D)/pathresolution.o \
	$(D)/pathlistutils.o $(D)/pathmapping_interf.o \
	$(D)/paths_ruletree_mapping.o \
	$(D)/paths_ruletree_maint.o

pathmapping/libpaths.a: $(objs)
pathmapping/libpaths.a: override CFLAGS := $(CFLAGS) -O2 -g -fPIC -Wall -W -I$(SRCDIR)/$(LUASRC) -I$(OBJDIR)/preload -I$(SRCDIR)/preload -I$(SRCDIR)/pathmapping \
	-Werror -Wmissing-prototypes -Wstrict-prototypes
pathmapping/libpaths.a: override LDFLAGS := $(LDFLAGS)
pathmapping/libpaths.a: override LIBS := 

targets := $(targets) $(D)/libpaths.a

