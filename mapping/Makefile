source_list := $(shell find $(D)/lua-5.1.1/src -name "*.c" -not -name "lua.c" \
		-not -name "luac.c" -not -name "print.c")

objs := $(source_list:.c=.o) $(D)/mapping.o

mapping/libmapping.a: $(objs)

mapping/libmapping.a: override CFLAGS := -O2 -g -fPIC -Wall -W \
			-I$(D)/lua-5.1.1/src $(CFLAGS)
mapping/libmapping.a: override LDFLAGS := $(LDFLAGS)
mapping/libmapping.a: override LIBS := 

targets := $(targets) $(D)/libmapping.a

