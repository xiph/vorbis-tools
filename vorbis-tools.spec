%define name	vorbis-tools
%define version	1.0rc3
%define release 2

Summary:	Several Ogg Vorbis Tools
Name:		%{name}
Version:	%{version}
Release:	%{release}
Group:		Libraries/Multimedia
Copyright:	GPL
URL:		http://www.xiph.org/
Vendor:		Xiphophorus <team@xiph.org>
Source:		ftp://ftp.xiph.org/pub/vorbis-tools/%{name}-%{version}.tar.gz
BuildRoot:	%{_tmppath}/%{name}-root
Requires:       libogg >= 1.0rc3
BuildRequires:	libogg-devel >= 1.0rc3
Requires:       libvorbis >= 1.0rc3
BuildRequires:	libvorbis-devel >= 1.0rc3
Requires:       libao >= 0.8.2
BuildRequires:	libao-devel >= 0.8.2
Requires:       curl >= 7.8
BuildRequires:	curl-devel >= 7.8

Prefix:		%{_prefix}

%description
vorbis-tools contains oggenc (and encoder) and ogg123 (a playback tool)

%prep
%setup -q -n %{name}-%{version}

%build
if [ ! -f configure ]; then
  CFLAGS="$RPM_OPT_FLAGS" ./autogen.sh --prefix=%{_prefix}
else
  CFLAGS="$RPM_OPT_FLAGS" ./configure --prefix=%{_prefix}
fi
make

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT
make DESTDIR=$RPM_BUILD_ROOT install

%files
%defattr(-,root,root)
%doc COPYING
%doc README
%doc ogg123/ogg123rc-example
%{_bindir}/oggenc
%{_bindir}/ogg123
%{_bindir}/ogginfo
%{_bindir}/vorbiscomment
%{_datadir}/man/man1/ogg123.1*
%{_datadir}/man/man1/oggenc.1*
%{_datadir}/man/man1/ogginfo.1*

%clean 
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%post

%postun

%changelog
* Fri May 23 2002 Thomas Vander Stichele <thomas@apestaart.org>
- Added more BuildRequires: for obvious packages
* Fri Mar 22 2002 Jack Moffitt <jack@xiph.org>
- Update curl dependency info (Closes bug #130)
* Mon Dec 31 2001 Jack Moffitt <jack@xiph.org>
- Update for rc3 release.
* Sun Oct 07 2001 Jack Moffitt <jack@xiph.org>
- Updated for configurable prefix
* Sun Aug 12 2001 Greg Maxwell <greg@linuxpower.cx>
- updated for rc2
* Sun Jun 17 2001 Jack Moffitt <jack@icecast.org>
- updated for rc1
- added ogginfo
* Mon Jan 22 2001 Jack Moffitt <jack@icecast.org>
- updated for prebeta4 builds
* Sun Oct 29 2000 Jack Moffitt <jack@icecast.org>
- initial spec file created
