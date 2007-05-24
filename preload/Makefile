objs := libsb2.o libsb_env.o sb_exec.o

preload/libsb2.so.$(PACKAGE_VERSION): $(call llep,$(objs))
	$(call llbuild_link_shlib,$(LIBSB2_SONAME))

preload/libsb2.so.$(PACKAGE_VERSION): mapping/libmapping.a

preload/libsb2.so.$(PACKAGE_VERSION): CFLAGS := $(CFLAGS) -fPIC -Wall -W
preload/libsb2.so.$(PACKAGE_VERSION): LDFLAGS := $(LDFLAGS)
preload/libsb2.so.$(PACKAGE_VERSION): LIBS := -ldl -lm -lpthread -lrt 

targets += preload/libsb2.so.$(PACKAGE_VERSION)


