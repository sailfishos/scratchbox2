objs := libsb2.o sb_exec.o

$(D)/libsb2.so: $(call O,$(objs))

$(D)/libsb2.so: mapping/libmapping.a mapping/liblua.a

$(D)/libsb2.so: CFLAGS := $(CFLAGS) -fPIC -Wall -W
$(D)/libsb2.so: LDFLAGS := $(LDFLAGS) -Wl,-soname=$(LIBSB2_SONAME)
$(D)/libsb2.so: LIBS := -ldl -lm -lpthread -lrt

targets := $(targets) $(D)/libsb2.so


