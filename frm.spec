Summary: Mailbox From/Subject lister
Name: frm
Version: 0.1a
Release: 1
License: BSD
Group: Applications/Mail
Source: frm-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Packager: Oskari Saarenmaa <oskari@saarenmaa.fi>

%description
Mailbox From/Subject lister, similiar to the 'frm' included in Elm's
distribution.

%prep
%setup -q -n frm

%build
make

%clean
rm -rf $RPM_BUILD_ROOT

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/bin
install -o 0 -g 0 -m 755 frm $RPM_BUILD_ROOT/usr/bin/frm

%files
%defattr(-,root,root)
/usr/bin/frm

%changelog
* Thu Mar 27 2003 Oskari Saarenmaa <oskari@saarenmaa.fi>
- initial