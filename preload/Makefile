objs := libsb2.o libsb_env.o sb_exec.o

$(D)/libsb2.so: $(call O,$(objs))

$(D)/libsb2.so: mapping/libmapping.a

$(D)/libsb2.so: CFLAGS := $(CFLAGS) -fPIC -Wall -W
$(D)/libsb2.so: LDFLAGS := $(LDFLAGS) -Wl,-soname=libsb2.so.1
$(D)/libsb2.so: LIBS := -ldl -lm -lpthread -lrt 

targets := $(targets) $(D)/libsb2.so


