%define name	vorbis-tools
%define version	1.0beta4
%define release 1

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
Requires:       libogg >= 1.0beta4
Requires:       libvorbis >= 1.0beta4
Requires:       libao >= 0.6.0

%description
vorbis-tools contains oggenc (and encoder) and ogg123 (a playback tool)

%prep
%setup -q -n %{name}-%{version}

%build
if [ ! -f configure ]; then
  CFLAGS="$RPM_FLAGS" ./autogen.sh --prefix=/usr
else
  CFLAGS="$RPM_FLAGS" ./configure --prefix=/usr
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
/usr/bin/oggenc
/usr/bin/ogg123
/usr/share/man/man1/ogg123.1.gz
/usr/share/man/man1/oggenc.1.gz

%clean 
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%post

%postun

* Mon Jan 22 2000 Jack Moffitt <jack@icecast.org>
- updated for prebeta4 builds
%changelog
* Sun Oct 29 2000 Jack Moffitt <jack@icecast.org>
- initial spec file created
