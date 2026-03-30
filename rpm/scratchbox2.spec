Summary:	Crosscompiling environment
License:	LGPLv2
URL:		https://github.com/sailfishos/scratchbox2
Name:		scratchbox2
Version:	2.3.90+git58
Release:	0
Source:		%{name}-%{version}.tar.gz
%if 0%{?!suse_version:1}
ExclusiveArch:	%{ix86} %{x86_64} x86_64
%endif
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

%ifarch %{ix86}
# Workaround crashes in libsb2 for programs compiled with non-default
# the stack boundary i.e , such as 3.  only happens on x86_32.
# Examples are: busybox and potentially valgrind.
# (the latter doesn't work fully in sb2.)
# Can be removed with GCC 15. See: JB#63809
%global optflags %(echo %{optflags} | sed 's|-msse2||')
%endif

%prep
%autosetup
# Tell autoconf the package version
# Note we don't strip the version here to not remove the indicator
# for a development build
echo %{version} > .tarball-version


%build
./autogen.sh
# FIXME: switch to vpath macros once we have them
%define build_dir %{_builddir}/build-%{_arch}
%define source_dir %{_builddir}%{?buildsubdir:/%{buildsubdir}}
mkdir -p %{build_dir}
%global _configure %{source_dir}/configure
(
    cd %{build_dir}
    %configure
)
%make_build -C %{build_dir} -f %{source_dir}/Makefile

%install
%make_install -C %{build_dir} -f %{source_dir}/Makefile

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
