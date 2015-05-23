%define reldate 20150523
%define reltype C
# may be one of: C (current), R (release), S (stable)

Name: sqpack
Version: 1.9.%{reldate}%{reltype}
Release: 1
Group: Applications/FTN
Summary: sqpack - purges squish or jam msgbases taken from fidoconfig
URL: http://huskyproject.org
License: GPL
Requires:  huskylib >= 1.9, fidoconf >= 1.9, smapi >= 2.5
BuildRequires: huskylib >= 1.9, fidoconf >= 1.9, smapi >= 2.5
BuildRequires: fidoconf-devel >= 1.9
Source: %{name}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root

%description
sqpack is a tool for purging messages in squish or jam msgbases

%prep
%setup -q -n %{name}

%build
make

%install
rm -rf %{buildroot}
make DESTDIR=%{buildroot} install

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%{_bindir}/*
%{_mandir}/man1/*
