CFLAGS += -Wall -W -fPIC
LIBS = -ldl -lm
export CFLAGS

subdirs = ../lua
obj-default = libsb2.o libsb_env.o sb_alien.o

extra_targets = libsb2.so

libsb2.so: built-in.o
	$(ll_pretty_link_shared_library)

