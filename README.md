# Dragoman KTextEditor plugin

A native KTextEditor plugin that translates the selection in place
through [Dragomand](https://github.com/VuteTech/dragomand), the offline
machine-translation daemon (website:
[dragomand.l10n-bg.dev](https://dragomand.l10n-bg.dev)). KTextEditor is
the editor framework behind Kate, KWrite and KDevelop, and the plugin
uses only host-neutral interfaces, so it is not tied to Kate. Kate and
KDevelop load plugins from this namespace; KWrite shares the same
loader since it became a stripped-down Kate, but exposes fewer UI
surfaces, so verify there.

Unlike the LibreOffice extension, it talks D-Bus directly with QtDBus.
`dragomanclient.{h,cpp}` is the reference Qt client for the daemon's
portal-style request pattern, including install-on-demand via
`PreparePair` with progress reporting.

Menu entries (Tools menu, once the plugin is enabled):

- **Translate Selection** (`Ctrl+Alt+T`): translates with the saved
  pair; the selection's script (Cyrillic versus Latin) reverses the
  direction automatically when it clearly points the other way.
- **Translate Selection (Choose Languages)**: pick the pair from what
  the daemon reports (installed and available), save it, translate.
- **Swap Translation Direction**.

Empty lines in the selection are preserved, a pair that is missing is
downloaded on first use (progress appears as a message in the view), and
if the document changes while a translation is in flight the result goes
to the clipboard instead of overwriting the edit. Selections are capped
at the daemon's per-request limits (256 lines, 1 MiB); run bigger jobs
through `dragomanctl`.

## Install

Released versions are packaged on the
[openSUSE Build Service](https://build.opensuse.org/package/show/home:blago:dragomand/dragoman-ktexteditor)
for openSUSE, Fedora, Debian, Ubuntu and Arch Linux, in the same
repository as Dragomand itself. With that repository added (see
[dragomand.l10n-bg.dev](https://dragomand.l10n-bg.dev/install/)),
install the `dragoman-ktexteditor` package, then enable **Dragoman
Translator** under Settings, Configure Kate, Plugins.

## Build from source

Needs CMake 3.24, extra-cmake-modules and KF6 (Config, CoreAddons, I18n,
TextEditor, XmlGui) 6.13 or newer, and Qt 6.8 or newer, plus a running
Dragomand for actual translation:

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build                      # unit tests, and the D-Bus client against a fake daemon
sudo cmake --install build                  # system-wide
```

For an install without root, pass `--prefix ~/.local` on the install
step and start the editor with that plugin path:

```sh
cmake --install build --prefix ~/.local
QT_PLUGIN_PATH=~/.local/lib/plugins kate    # check the install output for the exact dir
```

The version comes from git: a `vX.Y.Z` tag gives `X.Y.Z`, later commits
`X.Y.Z.N`; release tarballs carry it in `.tarball-version`, and
`-DDRAGOMAN_VERSION=` overrides it.

## Development

The code follows the KDE Frameworks style and guidelines: C++20, KDE's
strict Qt settings (no implicit string casts, `QT_NO_KEYWORDS`), no API
deprecated before the minimum Qt and KF versions, `u"..."_s` string
literals, and function-pointer or lambda connections only.
`cmake --build build --target clang-format` applies the style (the
`.clang-format` file comes from ECM at configure time and stays
untracked), and configuring installs a git pre-commit hook that checks
it. On the very first commit of a fresh clone the hook has no `HEAD` to
compare with; commit that one with `--no-verify`.

The layout: `src/` holds the plugin and its D-Bus client, `autotests/`
the QTest suites, `packaging/obs/` the OBS recipes, and `src/Messages.sh`
the translation extraction in KDE's convention.

## Continuous integration and releases

Every push to `master` and every pull request runs `.github/workflows/ci.yml`:
REUSE compliance, shellcheck, and a warnings-as-errors build with the
tests on Debian 13 (the oldest supported KF6 and Qt), Fedora 44 (the
newest, plus the clang-format check) and Debian 13 on ARM64.

Publishing a GitHub release with a `vX.Y.Z` tag runs
`.github/workflows/release.yml`: the same CI on the tagged commit, then
the source tarball (attached to the release with its sha256), the OBS
recipes stamped with the version, the commit to the
`home:blago:dragomand/dragoman-ktexteditor` package, and a wait for every
OBS build. It needs the repository secrets `OBS_USER` and `OBS_PASSWORD`.

## License

GPL-3.0-or-later. Every file carries its copyright and license, as SPDX
headers or through `REUSE.toml`; `reuse lint` checks it.
