# Changelog

All notable changes to this project are documented here, in the format of
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

Versions follow [Semantic Versioning](https://semver.org/). While the project is
below `1.0.0`, an incompatible change raises the **minor** number rather than
the major one; `1.0.0` is reserved for the release where a transfer survives
Dolphin being closed.

## [Unreleased]

## [0.1.0] - 2026-08-19

First release.

### Added

- **Share via Tailscale** submenu in Dolphin's context menu, listing the tailnet
  devices, receivable ones first and then by name.
- Devices that cannot receive files are shown disabled, with the reason in a
  tooltip: offline, Taildrop not enabled, unsupported operating system, owned by
  another user.
- Folders, and selections mixing files and folders, are packed into a single ZIP
  before sending, since Taildrop refuses directories. Symbolic links travel as
  links, including ones that point nowhere.
- Plasma notifications for compressing, sending, success and failure, with a
  Cancel action that stops the transfer.
- A warning before Dolphin closes while a transfer is still running.
- The whole interface in English and Brazilian Portuguese.
- Arch packages: `tailshare` from the released tag, `tailshare-git` from `main`.

### Known limitations

- The transfer runs inside Dolphin, so closing the window ends it. Dolphin warns
  first, and the detached helper that fixes this is planned for the next major
  step.
- Taildrop requires being an operator of `tailscaled`
  (`sudo tailscale set --operator=$USER`); without it every send is refused.
- A send has no timeout by design: a dropped tailnet and a slow transfer look
  identical, and the dropped one recovers on its own.

[Unreleased]: https://github.com/lPitecus/tailshare/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/lPitecus/tailshare/releases/tag/v0.1.0
