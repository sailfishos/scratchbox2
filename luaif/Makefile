# Not needed anymore, because Lua was dropped from libsb2:
#	$(D)/luaif.o $(D)/argvenvp.o

objs := $(D)/sblib_luaif.o

$(D)/sblib_luaif.o: preload/exported.h

luaif/libluaif.a: $(objs)
luaif/libluaif.a: override CFLAGS := $(CFLAGS) $(LUA_CFLAGS) -O2 -g -fPIC -Wall -W -I$(OBJDIR)/preload -I$(SRCDIR)/preload \
	$(WERROR) -Wmissing-prototypes -Wstrict-prototypes
luaif/libluaif.a: override LDFLAGS := $(LDFLAGS)
luaif/libluaif.a: override LIBS := $(LUA_LIBS)

lua_modules := $(D)/sb_pathmap.so \

$(lua_modules): override LIBS := -lsb2
$(lua_modules): override CFLAGS := $(CFLAGS) $(LUA_CFLAGS) -fPIC
$(lua_modules): override LDFLAGS := $(LDFLAGS) -L $(OBJDIR)/preload
$(lua_modules): CFLAGS += -Wno-error=missing-prototypes -Wno-missing-prototypes
$(lua_modules): $(OBJDIR)/preload/libsb2.$(SHLIBEXT)

$(D)/sb_pathmap.so: $(D)/lua_sb2_pathmap.o $(D)/lua_helpers.o

targets := $(targets) $(D)/libluaif.a $(D)/sb_pathmap.so
