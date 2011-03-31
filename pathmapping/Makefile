
objs := $(D)/paths.o

pathmapping/libpaths.a: $(objs)
pathmapping/libpaths.a: override CFLAGS := $(CFLAGS) -O2 -g -fPIC -Wall -W -I$(SRCDIR)/$(LUASRC) -I$(OBJDIR)/preload -I$(SRCDIR)/preload
pathmapping/libpaths.a: override LDFLAGS := $(LDFLAGS)
pathmapping/libpaths.a: override LIBS := 

targets := $(targets) $(D)/libpaths.a

