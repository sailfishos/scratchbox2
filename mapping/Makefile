objs := $(D)/mapping.o

mapping/libmapping.a: $(objs)

mapping/libmapping.a: override CFLAGS := -O2 -g -fPIC -Wall -W \
			$(LUA51_CFLAGS) $(CFLAGS)
mapping/libmapping.a: override LDFLAGS := $(LDFLAGS)
mapping/libmapping.a: override LIBS := 

targets := $(targets) $(D)/libmapping.a

