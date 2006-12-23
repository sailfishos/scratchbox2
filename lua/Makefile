CFLAGS += -fPIC -Wall -W
CFLAGS += -I./lua-5.1.1/src

export CFLAGS

source_list := $(shell find ./lua-5.1.1/src -name "*.c" -not -name "lua.c" -not -name "luac.c" -not -name "print.c")

#$(warning in lua/Makefile, before defining obj-default)
obj-builtin := $(source_list:.c=.o) lua_bindings.o

