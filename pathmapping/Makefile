
objs := $(D)/paths.o $(D)/paths_luaif.o $(D)/pathlistutils.o $(D)/pathmapping_interf.o

pathmapping/libpaths.a: $(objs)
pathmapping/libpaths.a: override CFLAGS := $(CFLAGS) -O2 -g -fPIC -Wall -W -I$(SRCDIR)/$(LUASRC) -I$(OBJDIR)/preload -I$(SRCDIR)/preload
pathmapping/libpaths.a: override LDFLAGS := $(LDFLAGS)
pathmapping/libpaths.a: override LIBS := 

targets := $(targets) $(D)/libpaths.a

