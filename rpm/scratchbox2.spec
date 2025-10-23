Summary:	Crosscompiling environment
License:	LGPLv2
URL:		https://git.sailfishos.org/mer-core/scratchbox2
Name:		scratchbox2
Version:	2.3.90+git58
Release:	0
Source:		%{name}-%{version}.tar.gz
ExclusiveArch:	%{ix86} %{x86_64} x86_64
BuildRequires:	make
BuildRequires:	autoconf
BuildRequires:	pkgconfig(lua)
BuildRequires:	automake
%if 0%{?suse_version}
BuildRequires:	lua-luaposix
Requires:		lua-luaposix
%else
BuildRequires:	lua-posix
Requires:		lua-posix
%endif
Requires:		libsb2 = %{version}-%{release}

%description
Scratchbox2 crosscompiling environment

%package -n libsb2
Summary: Scratchbox2 preload library

%description -n libsb2
Scratchbox2 preload library.

%package docs
Summary: Scratchbox2 docs
BuildArch: noarch

%description docs
Scratchbox2 man pages.

%prep
%autosetup
# Tell autoconf the package version
# Note we don't strip the version here to not remove the indicator
# for a development build
echo %{version} > .tarball-version


%build
./autogen.sh
# FIXME: switch to vpath macros once we have them
mkdir -p build
%global _configure ../configure
(
    cd build
    %configure
)
%make_build -C build -f ../Makefile

%install
%make_install -C build -f ../Makefile

install -D -m 644 utils/sb2.bash %{buildroot}/etc/bash_completion.d/sb2.bash

%check
# Rpmlint suggest to add it even thou we don't
# have any checks so far

%files
%{_bindir}/sb2*
%dir %{_datadir}/scratchbox2
%{_datadir}/scratchbox2/*
%config %{_sysconfdir}/bash_completion.d/sb2.bash

%files docs
%doc /usr/share/man/man1/*
%doc /usr/share/man/man7/*

%files -n libsb2
%dir %{_libdir}/libsb2
%{_libdir}/libsb2/*
