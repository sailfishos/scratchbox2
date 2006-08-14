CFLAGS += -Wall -W -fPIC
LIBS = -ldl -lm
export CFLAGS

subdirs = ../lua
obj-default = libsb2.o libsb_env.o sb_alien.o

extra_targets = libsb2.so

libsb2.so: built-in.o
#	$(LD) --export-dynamic -Bsymbolic -shared -o libsb2.so built-in.o ../lua/built-in.o $(LIBS)
	$(CC) -shared -o libsb2.so built-in.o ../lua/built-in.o $(LIBS)

