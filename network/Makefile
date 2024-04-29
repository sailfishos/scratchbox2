
objs := $(D)/net_rules.o

network/libsb2net.a: $(objs)
network/libsb2net.a: override CFLAGS := $(CFLAGS) $(LUA_CFLAGS) -O2 -g -fPIC -Wall -W -I$(SRCDIR)/$(LUASRC) -I$(OBJDIR)/preload -I$(SRCDIR)/preload -I$(SRCDIR)/network \
	$(WERROR) -Wmissing-prototypes -Wstrict-prototypes
network/libsb2net.a: override LDFLAGS := $(LDFLAGS)
network/libsb2net.a: override LIBS := 

targets := $(targets) $(D)/libsb2net.a

