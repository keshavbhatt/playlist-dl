# Changelog

All notable changes to Playlist Downloader are recorded here. The format follows
Keep a Changelog; the newest version is first.

## [3.0.0] - unreleased

A new app, rebuilt from the ground up: the same flow as before (search or paste, browse
the playlist, play, choose the download options, watch the queue) on a new foundation, with
a left rail for the Search, Playlist, Browser and Downloads pages.

### Added
- Search: playlist search with suggestions as you type, recent queries, a card grid with
  the video count on every playlist, a list view, and Load more. When the search service
  does not answer, the download engine searches instead and the page says so.
- Paste a playlist or video link into the search field to open it directly.
- Browser: the full YouTube site in tabs with sign-in, find in page, an ad blocker with a
  blocked-request count, full screen, and a Download button that knows whether the page is
  a playlist or a video.
- Downloads: one queue with pause, resume, retry, cancel, remove, open and show in folder;
  a playlist is one entry that shows which video it is on; filters for active, finished
  and failed; notifications with Open and Show in folder; taskbar progress; the screen
  stays awake while downloads run.
- The download engine sets itself up on first use and keeps itself updated; your YouTube
  sign-in is reused for downloads.
- Light and dark themes drawn from the new icon, a tray icon, single instance with
  `playlist-dl <link>` and `playlist-dl --download <link>`, a shortcuts sheet, What's new,
  diagnostics and Report a bug.

### Changed
- The player is now the Browser page; the desktop or mobile site switch became the browser
  identity setting.
- Download quality is chosen from a list (Best, 2160p to 360p; MP3, M4A, Opus, FLAC, WAV)
  instead of three sliders.

### Removed
- Rate this app, Claim offer and the blocked-request log window.
