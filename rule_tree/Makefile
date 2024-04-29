
objs := $(D)/rule_tree.o \
	$(D)/rule_tree_utils.o \
	$(D)/rule_tree_rpc_client.o

rule_tree/libruletree.a: $(objs)
rule_tree/libruletree.a: override CFLAGS := $(CFLAGS) $(LUA_CFLAGS) -O2 -g -fPIC -Wall -W -I$(SRCDIR)/$(LUASRC) -I$(OBJDIR)/preload -I$(SRCDIR)/preload \
	$(WERROR) -Wmissing-prototypes -Wstrict-prototypes
rule_tree/libruletree.a: override LDFLAGS := $(LDFLAGS)
rule_tree/libruletree.a: override LIBS := 

targets := $(targets) $(D)/libruletree.a

