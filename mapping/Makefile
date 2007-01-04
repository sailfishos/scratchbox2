source_list := $(shell find $(LLPWD)/lua-5.1.1/src -name "*.c" -not -name "lua.c" -not -name "luac.c" -not -name "print.c")

objs := $(source_list:.c=.o) $(LLPWD)/mapping.o

mapping/libmapping.a: $(objs)
	$(llbuild_link_archive)

mapping/libmapping.a: override CFLAGS := -fPIC -Wall -W -I$(LLPWD)/lua-5.1.1/src $(CFLAGS)
mapping/libmapping.a: override LDFLAGS := $(LDFLAGS)
mapping/libmapping.a: override LIBS := 

targets += mapping/libmapping.a

