Name:           ipfixcol2-clickhouse-output
Version:        1.0.0
Release:        1%{?dist}
Summary:        Plugin for export of IPFIX flow records into a ClickHouse database.

License:        BSD
URL:            http://www.liberouter.org/
Source0:        %{name}-%{version}.tar.gz
Group:          Liberouter
Vendor:         CESNET, z.s.p.o.
Packager:

BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}
BuildRequires:  gcc >= 7.0, gcc-c++ >= 7.0, cmake >= 2.8.8, make
BuildRequires:  ipfixcol2-devel, libfds-devel, /usr/bin/rst2man
Requires:       ipfixcol2 >= 2.0.0, libfds >= 0.1.0

%description
Plugin for converting IPFIX data to UniRec format.

%prep
%autosetup

#%post
#/sbin/ldconfig

#%postun
#/sbin/ldconfig

# Build source code
%build
%global _vpath_builddir .
%cmake .
make %{?_smp_mflags}

# Perform installation into build directory
%install
mkdir -p %{buildroot}/install
make install DESTDIR=%{buildroot}/install
mkdir -p %{buildroot}/%{_libdir}/ipfixcol2/ %{buildroot}/%{_mandir}/man7/
mv %{buildroot}/install/%{_libdir}/ipfixcol2/*.so* %{buildroot}/%{_libdir}/ipfixcol2/
mv %{buildroot}/install/%{_mandir}/man7/*.7* %{buildroot}/%{_mandir}/man7/
rm -rf %{buildroot}/install

%files
%{_libdir}/ipfixcol2/*.so*
%{_mandir}/man7/*.7*

%changelog
* Thu Jan 23 2025 Michal Sedlak <sedlakm@cesnet.cz> 1.0.0-1
- Initial RPM release
