# beware of namespace collisions, all subdir makefiles are included
# into the top-level Makefile

$(D)/fakeroot: CFLAGS := $(CFLAGS) -Wall -W $(WERROR) \
		-I$(SRCDIR)/preload -I$(OBJDIR)/preload $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include

$(D)/fakeroot: $(D)/fakeroot.o
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -ldl

targets := $(targets) $(D)/fakeroot
