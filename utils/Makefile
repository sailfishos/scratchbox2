# beware of namespace collisions, all subdir makefiles are included
# into the top-level Makefile

$(D)/sb2-show: CFLAGS := $(CFLAGS) -Wall -W -Werror \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include

$(D)/sb2-show.o: preload/exported.h
$(D)/sb2-show: $(D)/sb2-show.o
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -ldl

#------------
# sb2-ruletree, a debugging tool
$(D)/sb2-ruletree: CFLAGS := $(CFLAGS) -Wall -W -Werror \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include -ldl

$(D)/sb2-ruletree.o: preload/exported.h
$(D)/sb2-ruletree: $(D)/sb2-ruletree.o $(D)/../rule_tree/rule_tree.o
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

#------------
# sb2dctl, a tool for communicating with sb2d
$(D)/sb2dctl: override CFLAGS := $(CFLAGS) -Wall -W -Werror \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include -ldl

$(D)/sb2dctl.o: preload/exported.h
$(D)/sb2dctl: $(D)/sb2dctl.o 
$(D)/sb2dctl: rule_tree/rule_tree_rpc_client.o
$(D)/sb2dctl: sblib/sb_log.o
$(D)/sb2dctl: sb2d/libsupport.o
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

targets := $(targets) $(D)/sb2dctl
#------------

$(D)/sb2-monitor: CFLAGS := $(CFLAGS) -Wall -W -Werror \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include

$(D)/sb2-monitor: $(D)/sb2-monitor.o 
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(D)/sb2-interp-wrapper: CFLAGS := $(CFLAGS) -Wall -W -Werror \
		-I$(SRCDIR)/preload -Ipreload/ $(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include

$(D)/sb2-interp-wrapper: $(D)/sb2-interp-wrapper.o preload/libsb2.$(SHLIBEXT)
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -ldl


targets := $(targets) $(D)/sb2-show $(D)/sb2-monitor $(D)/sb2-ruletree
