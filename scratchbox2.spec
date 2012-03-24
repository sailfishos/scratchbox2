Summary: 	Scratchbox2 crosscompiling environment
License: 	LGPL
Name: 		scratchbox2
Version: 	2.3.48
Release: 	0
Source: 	%{name}-%{version}.tar.gz
Prefix: 	/usr
Group: 		Development/Tools
ExclusiveArch:	%{ix86}
BuildRequires:	make
Requires:	fakeroot

%description
Scratchbox2 crosscompiling environment

%prep
%setup -q

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

