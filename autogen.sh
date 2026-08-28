#!/bin/sh

# Stale caches can confuse autoconf.
rm -fr autom4te.cache || exit

# Let autoreconf figure out what, if anything, needs doing.
# Use autoreconf's -f option in case autoreconf itself has changed.
autoreconf --verbose --force --install

# Create a timestamp, so that './autogen.sh; make' doesn't
# cause 'make' to needlessly run 'autoheader'.
echo > stamp-h.in
echo > stamp-mak.in
