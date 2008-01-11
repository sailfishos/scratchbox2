# beware of namespace collisions, all subdir makefiles are included
# into the top-level Makefile

$(D)/sb2-show: CFLAGS := $(CFLAGS) -Wall -W -Werror \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include

$(D)/sb2-show.o: preload/exported.h
$(D)/sb2-show: preload/libsb2.so $(D)/sb2-show.o 
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

targets := $(targets) $(D)/sb2-show

