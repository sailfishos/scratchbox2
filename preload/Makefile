objs := libsb2.o libsb_env.o sb_alien.o

preload/libsb2.so: $(call llep,$(objs))
	$(llbuild_link_shlib)

preload/libsb2.so: lua/liblua_bindings.a

preload/libsb2.so: CFLAGS := $(CFLAGS) -fPIC -Wall -W -I$(TOPDIR)/lua/lua-5.1.1/src
preload/libsb2.so: LDFLAGS := $(LDFLAGS)
preload/libsb2.so: LIBS := -ldl -lm -lpthread -lrt 

targets += preload/libsb2.so


