# tailshare

A KDE Plasma plugin that adds a **Share via Tailscale** submenu to Dolphin's
context menu, listing the tailnet devices that can receive files and sending
them over [Taildrop](https://tailscale.com/kb/1106/taildrop).

> **Status: it works, and it has been used.** Every case of the QA script has
> been exercised in a real Dolphin session with the package installed — see
> *[What has been checked by hand](#what-has-been-checked-by-hand)* below. The
> interface speaks English and Brazilian Portuguese. See [PLAN.md](PLAN.md) for
> what is verified, what is covered by tests only, and what is neither.

## How it works

Right-click one or more files in Dolphin, open **Share via Tailscale**, and pick
a device. Devices that cannot receive files (offline, unsupported OS, file
sharing disabled) are shown greyed out with the reason in the tooltip. Progress
and results arrive as Plasma notifications.

Taildrop cannot transfer directories, so a selection containing a folder is
packed into a single ZIP before sending. Symbolic links inside a folder travel
as links, never as a copy of what they point at — including links that point
nowhere, and links whose target lies outside the selection. A link you select
*directly* is different: if it points at a folder, that folder's contents are
what gets packed, which is what Dolphin showed you.

## Requirements

**Runtime**

- KDE Plasma 6 with Dolphin
- `tailscale` 1.102 or newer in `PATH`, logged in and running
- Permission to use Taildrop as your own user:

  ```sh
  sudo tailscale set --operator=$USER
  ```

  Without it `tailscale file cp` answers `Access denied: file access denied`,
  and so does tailshare — the plugin runs as you, never as root.

**Build**

- CMake 3.20+, a C++17 compiler
- Qt 6.6+
- KDE Frameworks 6: Extra CMake Modules, CoreAddons, KIO, I18n, Notifications, Archive
- gettext, to compile the message catalogs

On Arch / CachyOS:

```sh
sudo pacman -S --needed base-devel cmake extra-cmake-modules gettext qt6-base \
    kcoreaddons kio ki18n knotifications karchive kwidgetsaddons
```

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

## Installing

### Arch / CachyOS

[packaging/](packaging) holds two recipes, and both fetch the sources
themselves — nothing else has to be in the directory:

```sh
cd packaging
makepkg -si                     # tailshare, the released version
makepkg -p PKGBUILD-git -si     # tailshare-git, the current main branch
```

They provide the same thing and cannot be installed at once; pacman replaces one
with the other. Either way the test suite runs as part of the build, and the
plugin, the message catalogs and the notification events land under `/usr`.
Remove with `sudo pacman -R tailshare` (or `tailshare-git`).

`makepkg` rewrites the `pkgver=` line of `PKGBUILD-git` on every run, which is
how VCS packages work — that change is meant to be committed, not reverted.

### Anywhere else

```sh
sudo cmake --install build
```

Then restart Dolphin so it picks up the new plugin:

```sh
killall dolphin
```

KDE Frameworks 6 discovers context-menu plugins by scanning the plugin
directory, so there is no `kbuildsycoca6` step.

### Uninstalling

Installed by hand, `cmake --install` leaves a manifest of everything it wrote:

```sh
sudo xargs rm -v < build/install_manifest.txt
```

### Disabling without uninstalling

Dolphin lets you turn individual context-menu plugins off. Either uncheck it
under *Settings → Configure Dolphin → Context Menu*, or edit
`~/.config/kservicemenurc`:

```ini
[Show]
tailshareitemaction=false
```

## Trying it without installing

The build tree mirrors the installed layout, so Dolphin can load the plugin
straight from it:

```sh
killall dolphin
env QT_PLUGIN_PATH=/absolute/path/to/tailshare/build/plugins \
    XDG_DATA_DIRS=/absolute/path/to/tailshare/build/share:/usr/local/share:/usr/share \
    dolphin ~
```

Two things this gets wrong easily. Use **absolute** paths: a `QT_PLUGIN_PATH`
that points nowhere makes Dolphin start with no plugin and say nothing about
it. And kill any running Dolphin first, or the new command is handed to the old
process, which never saw these variables.

`XDG_DATA_DIRS` is what lets the notifications find their event definitions and
the interface find its translations before either is installed; keep the system
directories in it or Dolphin loses its icons and MIME database.

`$TAILSHARE_TAILSCALE` overrides the `tailscale` executable, for an
installation that keeps it outside `PATH` — and for testing the plugin against
scripted output. It must be an **absolute** path: a bare name would be resolved
against the working directory of whatever loaded the plugin, which for Dolphin
is a folder someone chose. Note this is a convenience, not a security boundary —
`PATH` already decides which `tailscale` runs when the variable is unset.

## Translations

The interface is written in English and translated into Brazilian Portuguese;
the catalogs live in [po/](po) and are compiled and installed by the build.
Plasma follows your language settings, so nothing has to be configured — to see
the other language without changing them, set it for one command:

```sh
LANGUAGE=pt_BR XDG_DATA_DIRS="$PWD/build/share:$XDG_DATA_DIRS" \
    ./build/bin/tailshare-probe --list
```

Adding a language means adding `po/<code>/tailshare.po`; nothing else changes,
`ki18n_install()` picks up whatever is in `po/`. To refresh the template after
touching a string in `src/`, run `./Messages.sh` — it writes `po/tailshare.pot`,
which `msgmerge` then folds into each catalog:

```sh
./Messages.sh
msgmerge --update po/pt_BR/tailshare.po po/tailshare.pot
```

The `translationstest` in the suite does the extraction itself and checks every
catalog under `po/`, so a string added to `src/` and left untranslated shows up
as a failing test rather than as English text in a Portuguese menu.

Some text does not come from the catalogs at all, because it is read before any
translation is loaded: the plugin's name and description, shown in *Configure
Dolphin → Context Menu* and kept in `src/plugin/tailshareitemaction.json`, and
the notification event names, shown in *System Settings → Notifications* and
kept in `data/tailshare.notifyrc`. Both use the KDE convention of a `[pt_BR]`
suffix on the key, and both need the same treatment when a language is added.

## Development probe

`tailshare-probe` is a command line tool built with the project and
deliberately **not installed**. It drives the same send path the plugin
uses, so a transfer can be tested without going through the menu:

```sh
./build/bin/tailshare-probe --list                      # tailnet devices
./build/bin/tailshare-probe --dry-run -d pixel-8 file   # show the argv, send nothing
./build/bin/tailshare-probe -d pixel-8 ~/Pictures       # zip the folder and send it
```

Add `--notify` to also raise the Plasma notifications. Before installing, point
the notification system at the build tree so it finds the event definitions:

```sh
XDG_DATA_DIRS="$PWD/build/share:$XDG_DATA_DIRS" ./build/bin/tailshare-probe --notify -d pixel-8 file
```

`Ctrl+C` cancels the transfer instead of killing the probe. `--program <path>`
runs something else in place of `tailscale`, which is how the slow paths get
exercised without a large transfer.

## What has been checked by hand

The test suite covers the logic, and `ctest` is the place to look for that. This
table is the other half: what was observed clicking through a real Dolphin
session — Plasma 6, Dolphin 26.04, the package installed from `packaging/`, and
a live tailnet of five devices, two of them offline.

| Case | Observed |
|---|---|
| One file | Sent, with no compression step |
| Several files at once | Sent together, one notification: *"Sending 2 files to …"* |
| Name with spaces and accents | `relatório final.txt` arrives with its name intact |
| A folder | *Compressing* notification, then *Sending*: the folder is zipped first |
| A 400 MB folder | Compressed and sent; Dolphin's window stays responsive throughout — the list scrolls and the window resizes while the ZIP is being written |
| A file and a folder together | Packed into a single ZIP and sent |
| A device that cannot receive | Greyed out in the submenu, with the reason in its tooltip — *"This device is offline."* |
| Cancelling from the notification | The transfer stops and the cancellation is notified |
| Closing the window during a transfer | Dolphin warns that a transfer is still running before it closes |
| A selection that is not local | No submenu at all, checked by browsing into a ZIP with `zip:/…` |
| Tailscale stopped (`tailscale down`) | The submenu disappears entirely, and the context menu still opens at once |
| Tailscale logged out | The submenu disappears too — a different backend state, checked separately |
| The tailnet dropping mid-transfer | Nothing fails: `tailscale file cp` blocks while the backend is down and resumes when it comes back. Measured with 300 MB and a 15 second outage — the transfer finished normally, 31.5 s in total. The notification sits on *Sending* meanwhile, which is why the send has no timeout: a dropped network and a slow transfer look identical, and the dropped one recovers |

Two cases in the plan cannot be observed on this tailnet, because it has no
device with Taildrop disabled and no tailnet consisting of the local machine
alone: **a peer that cannot receive files** and **an empty tailnet**. Both are
covered by `plugintest`, against captured status output — checked by test, not
by hand, and listed here rather than left to look verified.

## Licence

GPL-2.0-or-later. See [LICENSE](LICENSE).

Tailscale and Taildrop are trademarks of Tailscale Inc. This project is not
affiliated with or endorsed by Tailscale Inc.
