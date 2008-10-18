LUASRC = luaif/lua-5.1.4/src

objs := $(D)/luaif.o $(D)/sb_log.o $(D)/paths.o $(D)/argvenvp.o

$(D)/sb_log.o: preload/exported.h

luaif/libluaif.a: $(objs)
luaif/libluaif.a: override CFLAGS := $(CFLAGS) -O2 -g -fPIC -Wall -W -I$(SRCDIR)/$(LUASRC) -I$(OBJDIR)/preload -I$(SRCDIR)/preload
luaif/libluaif.a: override LDFLAGS := $(LDFLAGS)
luaif/libluaif.a: override LIBS := 


lua_sources := loadlib.c lstate.c ldebug.c liolib.c llex.c lauxlib.c ldump.c \
	ltablib.c lstrlib.c ldo.c lmathlib.c lapi.c lvm.c ltable.c lgc.c \
	lbaselib.c lopcodes.c linit.c lfunc.c lparser.c lstring.c ldblib.c \
	lzio.c loslib.c ltm.c lundump.c lobject.c lmem.c lcode.c

lua_sources_expanded = $(foreach v,$(lua_sources),$(LUASRC)/$(v))

lua_objs := $(lua_sources_expanded:.c=.o)

luaif/liblua.a: $(lua_objs)
luaif/liblua.a: override CFLAGS := -O2 -g -fPIC -Wall -W $(CFLAGS)
luaif/liblua.a: override LDFLAGS := $(LDFLAGS)
luaif/liblua.a: override LIBS :=

targets := $(targets) $(D)/libluaif.a $(D)/liblua.a

