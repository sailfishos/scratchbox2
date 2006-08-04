CFLAGS += -fPIC -Wall -W
export CFLAGS

source_list := $(shell find ./lua-5.1/src -name "*.c" -not -name "lua.c" -not -name "luac.c" -not -name "print.c")

obj-default = $(source_list:.c=.o)

