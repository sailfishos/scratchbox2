source_list := $(shell find $(LLPWD)/lua-5.1.1/src -name "*.c" -not -name "lua.c" -not -name "luac.c" -not -name "print.c")

objs := $(source_list:.c=.o) $(LLPWD)/lua_bindings.o

lua/liblua_bindings.a: $(objs)
	$(llbuild_link_archive)

lua/liblua_bindings.a: override CFLAGS := -fPIC -Wall -W -I./lua/lua-5.1.1/src $(CFLAGS)
lua/liblua_bindings.a: override LDFLAGS := $(LDFLAGS)
lua/liblua_bindings.a: override LIBS := 

targets = lua/liblua_bindings.a

