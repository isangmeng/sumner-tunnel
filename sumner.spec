%define revcount %(git rev-list HEAD | wc -l)
%define treeish %(git rev-parse --short HEAD)
%define localmods %(git diff-files --exit-code --quiet  || echo .modified)

Summary: Sumner IP Tunnel
Name: sumner-tunnel
Version: 1.0
Release: %{revcount}.%{treeish}%{localmods}
Group: System Environment/Daemons
License: BSD
Vendor: Karl N. Redgate <Karl.Redgate@stratus.com>
Packager: Karl N. Redgate <Karl.Redgate@stratus.com>
%define _topdir %(echo $PWD)/rpm
BuildRoot: %{_topdir}/BUILDROOT
%define MyExports %(echo $PWD)/exports

%description
Mock for domaincontroller.
Start and manages virtual machine instances on a single node.

%prep
%build

%install
tar -C %{MyExports} -cf - . | (cd $RPM_BUILD_ROOT; tar xf -)

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0755)
/usr/sbin/sumner

%post
[ "$1" -gt 1 ] && {
    : upgrade
}

[ "$1" = 1 ] && {
    : install
}

: upgrade and install

%changelog

* Thu Feb 14 2013 Karl N. Redgate <karlredgate.github.io>
- Initial release

# vim:autoindent
# vim:syntax=plain
