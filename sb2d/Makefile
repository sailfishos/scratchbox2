# beware of namespace collisions, all subdir makefiles are included
# into the top-level Makefile

$(D)/sb2d: CFLAGS := $(CFLAGS) -Wall -W $(WERROR) \
		$(PROTOTYPEWARNINGS) \
		-I$(SRCDIR)/include \
		-I$(SRCDIR)/preload -Ipreload/

$(D)/sb2d: $(D)/sb2d.o \
		$(D)/server_socket.o \
		$(D)/libsupport.o \
		$(D)/ruletree_server.o \
		$(D)/rule_tree_luaif.o \
		sblib/sb_log.o \
		sblib/sb2_utils.o \
		rule_tree/rule_tree.o \
		rule_tree/rule_tree_utils.o \
		pathmapping/paths_ruletree_maint.o \
		execs/exec_ruletree_maint.o \
		luaif/sblib_luaif.o \
		luaif/liblua.a
	$(MKOUTPUTDIR)
	$(P)LD
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm -ldl

targets := $(targets) $(D)/sb2d
