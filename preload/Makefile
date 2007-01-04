objs := libsb2.o libsb_env.o sb_exec.o

preload/libsb2.so: $(call llep,$(objs))
	$(llbuild_link_shlib)

preload/libsb2.so: mapping/libmapping.a

preload/libsb2.so: CFLAGS := $(CFLAGS) -fPIC -Wall -W
preload/libsb2.so: LDFLAGS := $(LDFLAGS)
preload/libsb2.so: LIBS := -ldl -lm -lpthread -lrt 

targets += preload/libsb2.so


