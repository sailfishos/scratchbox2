LUASRC = mapping/lua-5.1.2/src

objs := $(D)/mapping.o

mapping/libmapping.a: $(objs)
mapping/libmapping.a: override CFLAGS := -O2 -g -fPIC -Wall -W \
			-I$(SRCDIR)/$(LUASRC) $(CFLAGS)
mapping/libmapping.a: override LDFLAGS := $(LDFLAGS)
mapping/libmapping.a: override LIBS := 


lua_sources := loadlib.c lstate.c ldebug.c liolib.c llex.c lauxlib.c ldump.c \
	ltablib.c lstrlib.c ldo.c lmathlib.c lapi.c lvm.c ltable.c lgc.c \
	lbaselib.c lopcodes.c linit.c lfunc.c lparser.c lstring.c ldblib.c \
	lzio.c loslib.c ltm.c lundump.c lobject.c lmem.c lcode.c

lua_sources_expanded = $(foreach v,$(lua_sources),$(LUASRC)/$(v))

lua_objs := $(lua_sources_expanded:.c=.o)

mapping/liblua.a: $(lua_objs)
mapping/liblua.a: override CFLAGS := -O2 -g -fPIC -Wall -W $(CFLAGS)
mapping/liblua.a: override LDFLAGS := $(LDFLAGS)
mapping/liblua.a: override LIBS :=

targets := $(targets) $(D)/libmapping.a $(D)/liblua.a

