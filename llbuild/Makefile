HOSTCC ?= gcc

targets = llflock

all: $(targets)

llflock: llflock.c
	@$(HOSTCC) -o llflock llflock.c

clean:
	@rm -f *.o $(targets)

