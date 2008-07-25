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


$(D)/sb2-monitor: CFLAGS := $(CFLAGS) -Wall -W -Werror \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include

$(D)/sb2-monitor: $(D)/sb2-monitor.o 
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(D)/sb2-interp-wrapper: CFLAGS := $(CFLAGS) -Wall -W -Werror

$(D)/sb2-interp-wrapper: $(D)/sb2-interp-wrapper.o
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -ldl


targets := $(targets) $(D)/sb2-show $(D)/sb2-monitor $(D)/sb2-interp-wrapper
