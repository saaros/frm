Summary: Mailbox From / Subject Lister
Name: frm
Version: 0.5
Release: 0.os1%{?dist}
License: BSD
Group: Applications/Mail
Source: frm-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-root
Packager: Oskari Saarenmaa <oskari@saarenmaa.fi>

%description
frm(1) is a mailbox from / subject lister, similar to the
frm utility in Elm mailer's distribution.

%prep
%setup -q -n frm

%build
make

%clean
rm -rf $RPM_BUILD_ROOT

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_bindir}
install -m 755 frm $RPM_BUILD_ROOT/%{_bindir}/frm

%files
%defattr(-,root,root)
%{_bindir}/frm

%changelog
* Thu Sep  2 2010 Oskari Saarenmaa <oskari@saarenmaa.fi>
- frm 0.5 - utf8

* Tue Sep 16 2008 Oskari Saarenmaa <oskari@saarenmaa.fi>
- frm 0.4 - mmap

* Wed Aug  3 2005 Oskari Saarenmaa <oskari@saarenmaa.fi>
- frm 0.3.1

* Wed Dec  1 2004 Oskari Saarenmaa <oskari@saarenmaa.fi>
- frm 0.3

* Wed Jun  4 2003 Oskari Saarenmaa <oskari@saarenmaa.fi>
- frm 0.2

* Thu Mar 27 2003 Oskari Saarenmaa <oskari@saarenmaa.fi>
- initial
