#!/bin/bash

#automake_version=1.9

rm -rf configure autom4te.cache
autoreconf --verbose --force --install --make

