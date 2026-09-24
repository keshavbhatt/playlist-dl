# Playlist Downloader

Search, browse, play and download YouTube playlists on the Linux desktop. Version 3.0 is a
rewrite of Playlist-Dl 2.x on Qt 6, C++20 and CMake; the 2.x code is on `main`.

Everything about the project is under `DOCS/` (start with `DOCS/README.md`). Building and
running for development:

```sh
sudo snap install kde-qt6-core24-sdk kf6-core24   # once
scripts/dev-build.sh --tests
scripts/dev-run.sh
```

Install: `snap install playlist-dl`
