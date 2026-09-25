# Playlist Downloader: project instructions

Playlist Downloader (snap and binary `playlist-dl`, formerly "Playlist-Dl") is an open-source
playlist search, browse, play and download app (Qt 6.11, C++20, CMake): playlists from any
site the engine reads (owner, 2026-09-24), with YouTube as the search source. Version 3.0 is
a from-scratch rewrite on `main`; the 2.x code on `old-qt5` (and its Qt 6 port on
`qt6-cmake-migration`) is the frozen reference. Read `DOCS/` for the feature contract
(FEATURES), design (DESIGN), decisions (DECISIONS, ADRs), lessons (LESSONS) and the progress
log (PROGRESS). The rewrite followed the owner's private rewrite kit (its PLAYBOOK); the
kit itself is not part of this repository. The public repository is
github.com/keshavbhatt/playlist-dl is the only remote (`main`: 3.x, `old-qt5`: 2.x); the
licence split is
in LICENSING.md and REUSE.toml: GPL-3.0-or-later, licensing module under its own licence.

## Standing rules

- Never mention accounts or licensing in user-facing release text: no accounts, licences, Pro
  plans, evaluation or trial, checkout, activation or the licence server in `CHANGELOG.md`,
  the metainfo `<releases>` notes, the What's new sheet, README or store copy. Licensing work
  is described only in commit messages and `DOCS/`. `tst_changelog` enforces it for the
  changelog.
- No commit or PR attribution lines. No em dashes or en dashes anywhere.
- User-facing text never names yt-dlp, Deno, QuickJS or ffmpeg: say "download engine" and
  "media converter". Real names only in install hints, the advanced system-engine option, and
  Diagnostics.
- ffmpeg is never downloaded; it comes from the system or the snap and Flatpak runtimes.
- Search runs through the download engine only (ADR-003 as revised on 2026-09-24); the
  ktechpit search service is gone. No tool name reaches the user; the UI says "search".
- The theme is the app's own (DESIGN.md): brand tokens derived from the app icon, a light and
  a dark scheme, the `{{token}}` sheet. No YouTube red, no system-theme styling.
- The application flow stays the one users know from 2.x: search or paste a playlist, browse
  its videos, play in the built-in player, pick download options, watch the queue. Screens
  are rebuilt, not reinvented; what was broken is fixed (LESSONS.md).
- The snap is built by GitHub Actions only, never locally. The Flatpak is built by Flathub's
  CI only; lint the manifest locally. Never push to the Flathub fork or open a Flathub PR
  without the owner's explicit consent.
- The store listing on snapcraft.io is maintained by hand; `snap/snapcraft.yaml` mirrors it.
- Verify headlessly (debug hooks, CDP helper) before asking the owner to look. Use a scratch
  `--profile` or back up and restore the owner's settings file
  (`~/.config/ktechpit/playlist-dl.conf`; the 2.x file is
  `~/.config/org.keshavnrj.ubuntu/Playlist DL.conf`).
- Release checklist: date the `## [x.y.z]` changelog heading, add the metainfo `<release>`,
  bump `project(VERSION)`, update the snap description, push `main`.

## Build and run (dev)

```sh
sudo snap install kde-qt6-core24-sdk kf6-core24   # once
scripts/dev-build.sh --tests
scripts/dev-run.sh
QT_LOGGING_RULES="pldl.*.debug=true" scripts/dev-run.sh
```
