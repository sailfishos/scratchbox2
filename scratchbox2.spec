Summary: 	Scratchbox2 crosscompiling environment
License: 	LGPL
Name: 		scratchbox2
Version: 2.3.90
Release: 10
Source: 	%{name}-%{version}.tar.gz
Patch1:	0001-scratchbox2-2.3.27-usrsrc.patch
Patch2:	0002-scratchbox2-2.3.52-wrapperargs.patch
Patch3:	0003-accel-localedef.patch
Patch4:	0004-support-ccache.patch
Patch5:	0005-accel-ccache.patch
Patch6:	0006-accel-qtchooser-qmake.patch
Patch7:	0007-Quote-PWD-variables-that-may-have-spaces-in-them.patch
Patch8:	0008-Accept-file-descriptor-0-as-log-fd.patch
Patch9:	0009-Add-support-for-x86_64-target.patch
Patch10:	0010-Add-patch-to-not-assume-that-readlink-is-located-int.patch
Patch11:	0011-Add-patch-to-fix-error-message-for-unsupported-targe.patch
Patch12:	0012-Accelerate-compression-utils.patch
Patch13:	0013-Add-postprocessing-for-name-parameter-of-opendir-and.patch
Patch14:	0014-Fix-log-printout.patch
Patch15:	0015-Turn-Werror-to-Wno-error-if-E-1-is-on-make-command-l.patch
Patch16:	0016-Fix-python2.7-acceleration-when-python-not-installed.patch
Patch17:	0017-Add-IF_EXISTS_IN-rule-enumeration.patch
Patch18:	0018-Simplify-function-interfaces.patch
Patch19:	0019-Add-handling-for-IF_EXISTS_IN-conditional-rule.patch
Patch20:	0020-Add-an-example-of-a-conditionally-accelerated-rule.patch
Patch21:	0021-Fix-IF_EXISTS_IN-condition-handling.patch
Patch22:	0022-Added-missing-handling-for-IF_EXISTS_IN-rule.patch
Patch23:	0023-Improve-ruletree-printout.patch
Patch24:	0024-Invert-path-found-logic.patch
Patch25:	0025-Exit-more-gracefully-if-valid-sb2-target-not-found.patch
Patch26:	0026-Print-out-action-tree-offset.patch
Patch27:	0027-Finalize-IF_EXISTS_IN-forward-rule.patch
Patch28:	0028-Add-reverse-mapping-for-IF_EXISTS_IN-rules.patch
Patch29:	0029-Only-accelerate-python-if-explicitly-requested.patch
Patch30:	0030-Add-conditional-acceleration-for-doxygen.patch
Patch31:	0031-Fix-IF_EXISTS_IN-rule-to-require-then_actions.patch
Patch32:	0032-Add-conditional-acceleration-rule-to-obs-rpm-build.patch
Patch33:	0033-Fix-call-to-realpath-with-NULL-second-argument.patch
Patch34:	0034-Add-qt4-qt5-moc-and-uic-to-accelerated-binaries.patch
Prefix: 	/usr
Group: 		Development/Tools
ExclusiveArch:	%{ix86}
BuildRequires:	make
Requires:	fakeroot

%description
Scratchbox2 crosscompiling environment

%prep
# Adjusting %%setup since git-pkg unpacks to src/
# %%setup -q -n src
%setup      -q      -n src     
%patch1 -p1
%patch2 -p1
%patch3 -p1
%patch4 -p1
%patch5 -p1
%patch6 -p1
%patch7 -p1
%patch8 -p1
%patch9 -p1
%patch10 -p1
%patch11 -p1
%patch12 -p1
%patch13 -p1
%patch14 -p1
%patch15 -p1
%patch16 -p1
%patch17 -p1
%patch18 -p1
%patch19 -p1
%patch20 -p1
%patch21 -p1
%patch22 -p1
%patch23 -p1
%patch24 -p1
%patch25 -p1
%patch26 -p1
%patch27 -p1
%patch28 -p1
%patch29 -p1
%patch30 -p1
%patch31 -p1
%patch32 -p1
%patch33 -p1
%patch34 -p1

%build
./autogen.sh
./configure; touch .configure
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
