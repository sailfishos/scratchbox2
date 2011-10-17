#!/bin/sh
#
# Copyright (C) 2011 Nokia Corporation. All rights reserved.
# Author: Lauri T. Aarnio

# This script builds Scratchbox2 for OpenSUSE; creates an RPM package to
# <SB2_source_directory>/packages/RPMS/*

# check CWD
if [ ! -d debian -o ! -f opensuse_rpm/build_rpms_for_opensuse.sh ]
then
	echo "Please run 'opensuse_rpm/build_rpms_for_opensuse.sh' in"
	echo "the top-level source directory of Scratchbox2"
	exit 1
fi

rm -rf rpm.tmp
mkdir -p rpm.builddir rpm.tmp packages

(cd packages; mkdir -p BUILD RPMS SOURCES SPECS SRPMS)

# Get version number
VRS=`awk '$1 ~ "^PACKAGE_VERSION$" && $2 ~ "^=$" {print $3}' <Makefile`

# Create a source tarball, exluding some directories.
# Note that 'debian' is included, because changelog lives there.
src_tarball=`pwd`/packages/SOURCES/scratchbox2-$VRS.tar.gz
echo "Packaging sources to $src_tarball"
REALSRCDIR=`pwd`
make clean
tar czf - \
	--exclude "packages" \
	--exclude "rpm.builddir" \
	--exclude "rpm.tmp" \
	--exclude ".git" \
	--exclude "obj-32" \
	--exclude "obj-64" \
	--exclude "old-ruledb-versions" \
	. | (cd rpm.tmp; mkdir scratchbox2-$VRS; cd scratchbox2-$VRS;
		tar xzf -; cd ..; tar czf $src_tarball scratchbox2-$VRS)
 
echo "Creating scratchbox2.spec"
BD=`pwd`/rpm.builddir
TOPDIR=`pwd`/packages
sed -e "s/@@VRS@@/$VRS/g" -e "s;@@BUILDDIR@@;$BD;" -e "s;@@TOPDIR@@;$TOPDIR;" <opensuse_rpm/spec_for_opensuse.src >packages/SPECS/scratchbox2.spec

echo building
set -x
(cd packages; rpmbuild -v -bb --clean SPECS/scratchbox2.spec)
set +x

echo "Done. Results:"
ls `pwd`/packages/RPMS/*
