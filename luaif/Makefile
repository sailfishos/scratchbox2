# Not needed anymore, because Lua was dropped from libsb2:
#	$(D)/luaif.o $(D)/argvenvp.o

objs := $(D)/sblib_luaif.o

$(D)/sblib_luaif.o: preload/exported.h

luaif/libluaif.a: $(objs)
luaif/libluaif.a: override CFLAGS := $(CFLAGS) -O2 -g -fPIC -Wall -W -I$(OBJDIR)/preload -I$(SRCDIR)/preload \
	$(WERROR) -Wmissing-prototypes -Wstrict-prototypes $(LUA_CFLAGS)
luaif/libluaif.a: override LDFLAGS := $(LDFLAGS)
luaif/libluaif.a: override LIBS := $(LUA_LIBS)


luaif/sb_log.o: include/scratchbox2_version.h


targets := $(targets) $(D)/libluaif.a

