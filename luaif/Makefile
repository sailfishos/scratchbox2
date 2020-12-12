LUASRC = luaif/lua-5.1.4/src

# Not needed anymore, because Lua was dropped from libsb2:
#	$(D)/luaif.o $(D)/argvenvp.o

objs := $(D)/sblib_luaif.o

$(D)/sblib_luaif.o: preload/exported.h

luaif/libluaif.a: $(objs)
luaif/libluaif.a: override CFLAGS := $(CFLAGS) -O2 -g -fPIC -Wall -W -I$(OBJDIR)/preload -I$(SRCDIR)/preload \
	$(WERROR) -Wmissing-prototypes -Wstrict-prototypes $(LUA_CFLAGS)
luaif/libluaif.a: override LDFLAGS := $(LDFLAGS)
luaif/libluaif.a: override LIBS := $(LUA_LIBS)


lua_sources := loadlib.c lstate.c ldebug.c liolib.c llex.c lauxlib.c ldump.c \
	ltablib.c lstrlib.c ldo.c lmathlib.c lapi.c lvm.c ltable.c lgc.c \
	lbaselib.c lopcodes.c linit.c lfunc.c lparser.c lstring.c ldblib.c \
	lzio.c loslib.c ltm.c lundump.c lobject.c lmem.c lcode.c

lua_sources_expanded = $(foreach v,$(lua_sources),$(LUASRC)/$(v))

lua_objs := $(lua_sources_expanded:.c=.o)

luaif/sb_log.o: include/scratchbox2_version.h

luaif/liblua.a: $(lua_objs)
luaif/liblua.a: override CFLAGS := -O2 -Dfopen=fopen_nomap -g -fPIC -Wall -W $(CFLAGS)
luaif/liblua.a: override LDFLAGS := $(LDFLAGS)
luaif/liblua.a: override LIBS :=

targets := $(targets) $(D)/libluaif.a

