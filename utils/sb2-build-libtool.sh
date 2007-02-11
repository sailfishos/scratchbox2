#!/bin/bash

set -e

NAME=libtool
VERSION=1.5.22

DLHOST="http://ftp.funet.fi/pub/mirrors/ftp.gnu.org/pub/gnu/libtool"
SOURCEFILE=$NAME-$VERSION.tar.gz
COMPILERDIR=
WORKDIR=libtool_build
mkdir -p $WORKDIR
cd $WORKDIR
wget $DLHOST/$SOURCEFILE
tar zxf $SOURCEFILE


./configure --prefix=$SBOX_TARGET_ROOT

# The system libtool script in Debian must be able to support
# invoking gcc as cc (Debian specific)
echo '# ### BEGIN LIBTOOL TAG CONFIG: BINCC' >> libtool
sed -n -e '/^# ### BEGIN LIBTOOL CONFIG/,/^# ### END LIBTOOL CONFIG/p' < libtool \
	| grep -B 2 -A 1 -e '^LTCC=' -e '^CC=' \
	| sed -e 's/gcc/cc/g' >> libtool
echo '# ### END LIBTOOL TAG CONFIG: BINCC' >> libtool
echo >> libtool

# The system libtool script in Debian must be able to support
# invoking g++ both by the g++ and c++ names. (Debian specific)
sed -n -e '/^# ### BEGIN LIBTOOL TAG CONFIG: CXX$$/,/^# ### END LIBTOOL TAG CONFIG: CXX$$/p' < libtool \
	| sed -e 's/CONFIG: CXX/CONFIG: BINCXX/g' \
		-e 's/g++/c++/g' >> libtool
echo >> libtool

# Add our BINCC and BINCXX tags (Debian specific)
sed -e 's/^\(available_tags\)=\"\(.*\)\"/\1=\"\2 BINCC BINCXX\"/' \
	< libtool > libtool.tags
mv libtool.tags libtool

# Make libtool executable again
chmod 755 libtool


make
make install

