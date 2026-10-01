#
# spec file for package dragoman-ktexteditor (openSUSE and Fedora targets on OBS)
#
# SPDX-FileCopyrightText: 2026 Blagovest Petrov <blagovest@petrovs.info>
# SPDX-FileCopyrightText: 2026 Vute Tech Ltd. <https://vute.tech>
# SPDX-License-Identifier: GPL-3.0-or-later
#
# @VERSION@ is stamped by packaging/obs/prepare.sh from the release tag; the
# release tarball carries it in .tarball-version as well, for CMake.

Name:           dragoman-ktexteditor
Version:        @VERSION@
Release:        0
Summary:        Offline translation plugin for Kate, KWrite and KDevelop
License:        GPL-3.0-or-later
URL:            https://dragomand.l10n-bg.dev
Source0:        %{name}-%{version}.tar.gz

# The dragomand daemon it talks to exists for these two only.
ExclusiveArch:  x86_64 aarch64

BuildRequires:  cmake >= 3.24
BuildRequires:  gcc-c++
BuildRequires:  gettext
BuildRequires:  cmake(KF6Config) >= 6.13
BuildRequires:  cmake(KF6CoreAddons) >= 6.13
BuildRequires:  cmake(KF6I18n) >= 6.13
BuildRequires:  cmake(KF6TextEditor) >= 6.13
BuildRequires:  cmake(KF6XmlGui) >= 6.13
BuildRequires:  cmake(DragomanQt)
BuildRequires:  cmake(Qt6Core) >= 6.8
BuildRequires:  cmake(Qt6DBus) >= 6.8
BuildRequires:  cmake(Qt6Gui) >= 6.8
BuildRequires:  cmake(Qt6Test) >= 6.8
BuildRequires:  cmake(Qt6Widgets) >= 6.8
%if 0%{?fedora}
BuildRequires:  extra-cmake-modules >= 6.13
BuildRequires:  ninja-build
BuildRequires:  dbus-daemon
%else
BuildRequires:  kf6-extra-cmake-modules >= 6.13
BuildRequires:  ninja
BuildRequires:  dbus-1
%endif
Requires:       dragomand

%description
A KTextEditor plugin (Kate, KWrite, KDevelop) that translates the
selection in place, fully offline, through the Dragomand daemon and
Mozilla's Firefox translation models. The Tools menu gains Translate
Selection, a language chooser and a swap of the direction; a language
pair that is not installed yet is downloaded on first use.

%prep
%setup -q

%build
cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_INSTALL_PREFIX=%{_prefix} \
    -DKDE_INSTALL_USE_QT_SYS_PATHS=ON \
    -DBUILD_TESTING=ON
cmake --build build %{?_smp_mflags}

%check
ctest --test-dir build --output-on-failure

%install
DESTDIR=%{buildroot} cmake --install build

%files
%license LICENSES/GPL-3.0-or-later.txt
%doc README.md
# Kate and KDevelop load plugins from here, but no package owns the
# directory on openSUSE.
%dir %{_libdir}/qt6/plugins/kf6/ktexteditor
%{_libdir}/qt6/plugins/kf6/ktexteditor/dragomanplugin.so
%{_datadir}/metainfo/dev.l10n_bg.dragomand.ktexteditor.metainfo.xml
%dir %{_datadir}/qlogging-categories6
%{_datadir}/qlogging-categories6/dragoman-ktexteditor.categories

%changelog
* @RPM_DATE@ Blagovest Petrov <blagovest@petrovs.info> - @VERSION@
- First commit
