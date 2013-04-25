Summary: 	Scratchbox2 crosscompiling environment
License: 	LGPL
Name: 		scratchbox2
Version: 2.3.90
Release: 4
Source: 	%{name}-%{version}.tar.gz
Patch1:	0001-scratchbox2-2.3.27-usrsrc.patch
Patch2:	0002-scratchbox2-2.3.52-wrapperargs.patch
Patch3:	0003-accel-localedef.patch
Patch4:	0004-support-ccache.patch
Patch5:	0005-accel-ccache.patch
Patch6:	0006-accel-qtchooser-qmake.patch
Patch7:	0007-Quote-PWD-variables-that-may-have-spaces-in-them.patch
Prefix: 	/usr
Group: 		Development/Tools
ExclusiveArch:	%{ix86}
BuildRequires:	make
Requires:	fakeroot

%description
Scratchbox2 crosscompiling environment

%prep
# Adjusting %%setup since git-pkg unpacks to src/
# %%setup -q
%setup -q -n src
%patch1 -p1
%patch2 -p1
%patch3 -p1
%patch4 -p1
%patch5 -p1
%patch6 -p1
%patch7 -p1

%build
./autogen.sh
./configure
make

%install
make install prefix=$RPM_BUILD_ROOT/usr

%files
%defattr(-,root,root)
/usr/bin/sb2*
/usr/lib/libsb2/*
%ifarch x86_64
/usr/lib32/libsb2/*
%endif
/usr/share/scratchbox2/*

%doc %attr(0444,root,root) /usr/share/man/man1/*
%doc %attr(0444,root,root) /usr/share/man/man7/*
