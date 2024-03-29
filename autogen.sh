#!/bin/sh

# Stale caches can confuse autoconf.
rm -fr autom4te.cache || exit

# Let autoreconf figure out what, if anything, needs doing.
# Use autoreconf's -f option in case autoreconf itself has changed.
autoreconf --verbose --force --install
