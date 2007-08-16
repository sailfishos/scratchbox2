objs := $(D)/mapping.o

mapping/libmapping.a: $(objs)

mapping/libmapping.a: override CFLAGS := -O2 -g -fPIC -Wall -W \
			-Imapping/lua-5.1.2/src $(CFLAGS)
mapping/libmapping.a: override LDFLAGS := $(LDFLAGS)
mapping/libmapping.a: override LIBS := 

lua_src := $(shell find $(D)/lua-5.1.2/src -name "*.c" -not -name "lua.c" \
	-not -name "luac.c" -not -name "print.c")
lua_objs := $(lua_src:.c=.o)

mapping/liblua.a: $(lua_objs)
mapping/liblua.a: override CFLAGS := -O2 -g -fPIC -Wall -W
mapping/liblua.a: override LDFLAGS := $(LDFLAGS)
mapping/liblua.a: override LIBS :=

targets := $(targets) $(D)/libmapping.a $(D)/liblua.a

