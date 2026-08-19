# tailshare

A KDE Plasma plugin that adds a **Share via Tailscale** submenu to Dolphin's
context menu, listing the tailnet devices that can receive files and sending
them over [Taildrop](https://tailscale.com/kb/1106/taildrop).

> **Status: early development, but it works.** The submenu, the transfer and
> the notifications have been exercised in a real Dolphin session. What is
> missing is packaging and translation, so for now you build it yourself. See
> [PLAN.md](PLAN.md) for what is verified and what is not.

## How it will work

Right-click one or more files in Dolphin, open **Share via Tailscale**, and pick
a device. Devices that cannot receive files (offline, unsupported OS, file
sharing disabled) are shown greyed out with the reason in the tooltip. Progress
and results arrive as Plasma notifications.

Taildrop cannot transfer directories, so a selection containing a folder is
packed into a single ZIP before sending.

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

- CMake 3.20+, a C++20 compiler
- Qt 6.6+
- KDE Frameworks 6: Extra CMake Modules, CoreAddons, KIO, I18n, Notifications, Archive

On Arch / CachyOS:

```sh
sudo pacman -S --needed base-devel cmake extra-cmake-modules qt6-base \
    kcoreaddons kio ki18n knotifications karchive
```

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
ctest --test-dir build --output-on-failure
```

## Installing

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

`XDG_DATA_DIRS` is what lets the notifications find their event definitions
before `tailshare.notifyrc` is installed; keep the system directories in it or
Dolphin loses its icons and MIME database.

`$TAILSHARE_TAILSCALE` overrides the `tailscale` executable, for an
installation that keeps it outside `PATH` — and for testing the plugin against
scripted output.

## Development probe

`tailshare-probe` is a command line tool built with the project and
deliberately **not installed**. It drives the same send path the plugin will
use, so a transfer can be tested before there is any menu:

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

## Licence

GPL-2.0-or-later. See [LICENSE](LICENSE).

Tailscale and Taildrop are trademarks of Tailscale Inc. This project is not
affiliated with or endorsed by Tailscale Inc.
