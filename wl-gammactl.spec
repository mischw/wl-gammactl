Name:     wl-gammactl
Version:  0.1
Release:  0.1.20200220git.611846c%{?dist}.wef
Summary:  Small GTK GUI application to set contrast, brightness and gamma for wayland compositors
License:  MIT
URL:      https://github.com/mischw/wl-gammactl

# use this to fetch the source as the build needs to do a git clone
# (is there a better way?):

# git clone https://github.com/mischw/wl-gammactl
# cd wl-gammactl
# git checkout 611846
# meson build
# ninja -C build
# cd ../
# mv wl-gammactl wl-gammactl-0.1
# tar czf ~/rpmbuild/SOURCES/wl-gammactl-0.1.tar.gz --exclude .git\* --exclude build wl-gammactl-0.1

Source0:  %{name}-%{version}.tar.gz

BuildRequires: gcc
BuildRequires: git
BuildRequires: gtk3-devel
BuildRequires: glib2-devel
BuildRequires: meson
BuildRequires: wlroots-devel
BuildRequires: wayland-devel
BuildRequires: wayland-protocols-devel

%description

Small GTK GUI application to set contrast, brightness and gamma for
wayland compositors which support the wlr-gamma-control protocol
extension. Basically this is the example from here:
https://github.com/swaywm/wlroots/blob/master/examples/gamma-control.c
with a nice little GTK GUI slapped on to it. You can set contrast,
brightness and gamma using sliders and reset back to default values.

This was made to make the process of calibrating your monitor a bit
easier, since wayland support for color profiles is not yet
implemented. When you are satisfied with your settings, copy the given
command line and execute it at startup to make the settings load at
apply on every boot.

Keep in mind that it uses the same protocol extension like the
redshift fork
https://aur.archlinux.org/packages/redshift-wlr-gamma-control/ When
running wl-gammactl it will kick out any running redshift instance and
fail to start up. On second run it should work as expected. So
unfortunatly only one can run at a time (?) for now.

%prep
%autosetup

%build
%meson
%meson_build

%install
%meson_install

%files
%{_bindir}/%{name}

%doc README.md

%license LICENSE

%changelog
* Wed Feb 19 2020 Bob Hepple <bob.hepple@gmail.com> - 0.1-0.1.20200220git.611846c.fc31.wef
- Initial version of the package
