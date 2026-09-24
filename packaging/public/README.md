<p align="center">
  <img src="screenshots/banner.png" alt="Playlist Downloader: save whole playlists offline" width="100%">
</p>

<p align="center">
  Search YouTube playlists, pick the videos, download them as video or audio, and play the
  whole playlist in your media player. Native, lightweight, built with Qt&nbsp;6.
</p>

<p align="center">
  <a href="https://snapcraft.io/playlist-dl"><img src="https://snapcraft.io/static/images/badges/en/snap-store-black.svg" alt="Get it from the Snap Store" height="56"></a>
</p>

<p align="center">
  <img src="screenshots/00-hero.png" alt="Playlist search" width="100%">
</p>

## Install

**Snap** (amd64 and arm64):

```sh
sudo snap install playlist-dl
```

## Features

**Find a playlist**
- Search YouTube playlists by keyword, with suggestions as you type, or paste a playlist or
  video link. Every result shows its cover, channel and video count, as cards or a list.
- Browse the playlist: every video with its thumbnail and length; select all, a range or a
  few; unavailable videos are set aside; play any video first.

**Download**
- MP4, MKV or WebM up to 4K, or audio only as MP3, M4A, Opus, FLAC or WAV, with subtitles,
  thumbnails and metadata embedded. Your choices become the defaults for next time.
- One queue for the whole playlist: progress per video, pause, resume, retry, a folder per
  playlist with files numbered in playlist order, and a notification with Show in folder.
- A playlist file (.m3u8) lands next to the videos; open the finished playlist to arrange
  the order and play it all in your media player.
- The download engine sets itself up on first use and keeps itself updated.

**Watch**
- A built-in browser for YouTube: tabs, find in page, an ad blocker with a count, full
  screen. Sign in once and the sign-in is reused for downloads, so restricted videos come too.

**Made for the desktop**
- Light and dark themes, a tray icon, taskbar progress, native Wayland and X11, a shortcuts
  sheet, single instance with `playlist-dl <link>` and `playlist-dl --download <link>`.

## Screenshots

<table>
  <tr>
    <td width="50%"><img src="screenshots/01-playlist.png" alt="Pick the videos"></td>
    <td width="50%"><img src="screenshots/02-options.png" alt="Download options"></td>
  </tr>
  <tr>
    <td width="50%"><img src="screenshots/03-downloads.png" alt="The download queue"></td>
    <td width="50%"><img src="screenshots/05-items.png" alt="Play a downloaded playlist"></td>
  </tr>
</table>

## About this repository

This repository is Playlist Downloader's public home: the README, screenshots, guide,
changelog, releases and the issue tracker.

## Help

The [user guide](GUIDE.md) covers the window, search, playlists, downloads, the browser,
every setting, the shortcuts and common problems. Bugs and requests go to the
[issues](https://github.com/keshavbhatt/p-pldl/issues), or by email to
[connect@ktechpit.com](mailto:connect@ktechpit.com); please attach the diagnostics from
*About, Copy* when reporting a problem.

<sub>YouTube is a trademark of Google LLC. Playlist Downloader is an independent app and is
not affiliated with, endorsed by, or sponsored by YouTube or Google.</sub>
