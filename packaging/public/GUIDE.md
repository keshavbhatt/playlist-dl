# Playlist Downloader guide

Playlist Downloader finds YouTube playlists, plays them in a built-in browser page and saves
whole playlists offline as video or audio. This guide walks through the four pages of the
window and the settings that matter.

## The window

A rail on the left holds the pages: **Search** (Ctrl+1), **Playlist** (Ctrl+2), **Browser**
(Ctrl+3) and **Downloads** (Ctrl+4), with **Settings** (Ctrl+,) and your account at the
bottom. Every action has a keyboard shortcut; press Ctrl+/ for the list.

## Search

Type what you are looking for and press Enter. Suggestions appear as you type; the results
are playlist cards with the number of videos on each. Click a card to open the playlist.
Recent searches come back as chips under the field (Settings, General, Keep search history).

You can also paste a link: a playlist link opens the Playlist page, a video link offers to
download that video.

When the search service does not answer, the download engine searches instead; the chip in
the top right corner then reads "Search (engine)". Settings, Search lets you use the engine
only.

## Playlist

The playlist's videos with their thumbnails and lengths. Tick the ones you want, use
"Select all", the From and To boxes or the slider for a range, the filter field to find a
title, and the sort menu. Videos that are private or removed are greyed out and never
downloaded. Hover a row for **Play** (opens the video on the Browser page) and **Download**
(just that video).

**Download** at the top opens the options sheet: Video or Audio only, quality and container
(MP4, MKV, WebM, up to 4K) or audio format (MP3, M4A, Opus, FLAC, WAV) and bitrate,
subtitles, embedded thumbnail and metadata, the folder (by default a folder named after
the playlist inside your download folder, files numbered in playlist order). Your choices
become the defaults for next time.

## Browser

The full YouTube site in tabs. Sign in once and your subscriptions and history work as on
the website; the sign-in is also used for downloads, so age-restricted videos download too.
Ads are blocked; the "Ads blocked" badge counts them. Ctrl+F finds text on the page, F11
goes full screen, Ctrl+T opens a tab, Ctrl+W closes one.

**Download this** in the toolbar downloads the video on the current page, or opens the
Playlist page when the page is a playlist. When a video is playing, a floating "Download
detected" button appears in the bottom right corner.

## Downloads

Every download is a card with its progress, speed and time left. A playlist is one card
that shows which video it is on. Hover a card for pause or resume, cancel, retry, open,
show in folder and remove. The filters show active, finished or failed downloads; the
Clear menu removes finished or failed ones; Open folder opens your download folder.

You get a notification when a download finishes, with Open and Show in folder. The
download engine sets itself up on first use and keeps itself up to date; the chip next to
the page title shows its state.

## Settings

- **General**: the page the app starts on, what closing does (quit or keep in the tray),
  search history, What's new after updates, notifications.
- **Appearance**: light, dark or the system theme; interface scale.
- **Downloads**: the folder, a folder per playlist, numbering, the defaults for new
  downloads, concurrent downloads, speed limit, skip already downloaded, use my YouTube
  sign-in, and the download engine card (check for updates, update, auto-update).
- **Browser**: start page, restore tabs, block ads, Do Not Track, browser identity.
- **Search**: search service, results per page.
- **Advanced**: hardware acceleration, clear cache, sign out and clear session, reset
  permissions, open the log folder, copy diagnostics, reset settings.

## Command line

```
playlist-dl <link>              open a playlist or video link in the running window
playlist-dl --download <link>   queue a download right away
playlist-dl --settings          open the settings
playlist-dl --profile <name>    run with a separate profile
playlist-dl --quit              quit the running instance
```

## Something wrong?

Settings, Advanced, **Copy diagnostics** puts the versions, paths and recent log lines on
the clipboard; About has a **Report a bug** button that opens an issue with them attached.
