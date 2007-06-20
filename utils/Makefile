$(D)/sb_gcc_wrapper: $(call O,sb_gcc_wrapper.o sb_config.o)
	$(P)CXX
	$(Q)$(CXX) $(CFLAGS) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

targets := $(targets) $(D)/sb_gcc_wrapper

