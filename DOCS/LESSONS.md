# Lessons: Playlist-Dl 2.x and the rewrite kit

Red 10's lessons (`rewrite-kit/reference/red-docs/LESSONS.md`, R1 to R13 and V1 to V8) and
UMD 7's (U1 to U15, R1 to R8) apply to any Qt WebEngine wrapper with a download engine and
are not repeated. Below: what 2.x adds, from `reference/analysis-playlist-dl-v2.md` section 7.

## A. Playlist-Dl 2.x

| # | Mistake | Consequence | Rule |
|---|---|---|---|
| P1 | One search backend on a shared host, no timeout, errors shown as "no results" | an outage killed the app's first screen | two backends, a timeout, a visible source chip, error states with Retry (ADR-003) |
| P2 | Formats and progress parsed from the engine's human output (`-F`, `[download]` lines) | `-f 0+0` on empty lists, wrong progress on every engine change | typed protocol only (ADR-000) |
| P3 | Engine run through `python3`, version compared as a mis-parsed date, binary truncated before the download, Cancel loops the modal | no updates ever, broken engine after a failed download | the kit's EngineManager: standalone binary, checksum, `QDate` compare, async |
| P4 | ffmpeg neither bundled nor detected in the snap | every merge and audio extraction failed under confinement | ffmpeg from the runtime, detected, with an install hint (ADR-000) |
| P5 | Ad list rebuilt quadratically and re-deduplicated on every request; QSettings read per request; a QSettings write per blocked request | measurable lag on every page | immutable block list snapshot, atomic counter, no settings in the IO path |
| P6 | Ad blocker toggle half wired, a reload the user may decline, duplicate scripts | page and script collection out of sync | one profile-level bundle, config pushed over the bridge |
| P7 | Default profile in Qt 6 is off-the-record | sign-in, cookies and cache lost on quit | a named persistent profile, one storage function |
| P8 | Blocking waits on the GUI thread (`waitForStarted`, `waitForFinished`, modal exec from timers, `processEvents` in loops) | freezes, re-entrancy | async only; sheets with callbacks |
| P9 | The same filter written three times, inverted twice | private and deleted videos reached the queue | one `MediaInfo` flag, one place that filters, a test |
| P10 | Raw playlist titles in paths, no playlist index in file names | overwrites and "resumes" of the wrong file | sanitised folder, `%(playlist_index)s` in the template |
| P11 | Progress and state in QSettings side files per video, concurrency by `findChildren` and object names | stale "running" rows after a crash, counters reset on every load | one queue model persisted as JSON, states recomputed on load |
| P12 | Trial and account state in `$HOME/Downloads` and base64 flags | trivially bypassed, clutter | the shared licence module; the old files read once, never written |
| P13 | `--disable-web-security` for everyone, plain http endpoints | insecure by default | no Chromium flags beyond the GPU set; https only |
| P14 | Shared `QNetworkAccessManager` aborted and disconnected by thumbnail labels, replies never deleted | thumbnails and searches cancelling each other | `ThumbnailCache` owns its manager; replies are `deleteLater`ed by their owner |
| P15 | Typos baked into object names and strings ("Unclock", "adBocker", "rungaurd") | embarrassing UI, unsearchable code | strings reviewed, names spelt |
| P16 | Dead settings and dead classes shipped (`ClaimOffer`, `Request`, `lastVisited`, `showSearchSuggestion`) | maintenance tax | delete on the spot |

## B. Rules the rewrite kit already encodes

- Ask the owner the product questions early; make the routine calls yourself.
- Probe the live page over CDP before writing a selector.
- Script the screenshots; verify headlessly before asking the owner to look.
- A word-wrapped `QLabel` gives a layout a width-dependent height Qt leaves out of a
  window's minimum size: give it a full-width row and pin its minimum height.
- The kit's navigation policy is a YouTube wrapper's (everything else goes to the system
  browser). A general browser keeps every http and https page in the app and lets only
  mailto, tel and magnet out; a sign-in page's links otherwise leave for the desktop.
- A bar shown and hidden in the layout above a page moves the page by its height at every
  load: draw progress inside a widget that is already there (the address field's hairline).
- A still loader glyph reads as a freeze: turn it on a timer while the page loads.
- Five pill buttons fit one row at about 680 px; a sixth does not. Secondary entries go
  as a flat link beside the line they belong to (What's new beside the version in About).
