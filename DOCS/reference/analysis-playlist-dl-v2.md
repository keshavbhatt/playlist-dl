# Analysis: Playlist-Dl v2.x (frozen, 2026-09-24)

Source: the 2.x tree, now branch `old-qt5` (git history: 14 commits, 2022-03-07 to 2026-09-23; last
user-facing release "2.1"/"2.2" in 2024, project version now 3.0.0 after the Qt 6 port). About
10,000 lines of C++ (`.cpp` + `.h`), 4,171 lines of `.ui`, 1,164 lines of JS/CSS/ad-list.
Everything below is from reading the full source, not from memory. Runtime behaviour is that of
the Qt 5 app; the CMake/Qt 6 commit only ported APIs and fixed one script re-insert bug.

---

## 1. Build and stack

**Until 2026-09-23:** qmake, `src/PlaylistDownloader.pro` (created by Qt Creator 2021-04-18).
`QT += core gui network webenginewidgets widgets`, `CONFIG += c++11`, `VERSION = 2.2` exported
as `VERSIONSTR`, release builds define `QT_NO_DEBUG_OUTPUT`. Four `.pri` includes:
`SlidingStackedWidget`, `WebEnginePlayer`, `PlaylistDownloadWidget`, `RateApp`. Built against
Qt 5.15.4 from the beineri focal PPA inside the snap.

**Now (commit 74c2525, "build: migrate to Qt 6 and CMake"):** CMake 3.21+, C++20, Qt 6.8+
(`Core Gui Widgets Network WebEngineWidgets WebChannel`), `qt_standard_project_setup()`, AUTORCC,
warnings `-Wall -Wextra -Wnon-virtual-dtor -Woverloaded-virtual` (`PLDL_WERROR` off by default),
optional clang-tidy, `QT_NO_DEBUG_OUTPUT` in Release. `cmake/Version.cmake` generates
`version.h` (`PLDL_VERSION`, `PLDL_GIT_REVISION`); `cmake/SnapSdkWorkaround.cmake` strips the
snap SDK's generic `/usr/include` from imported Qt targets; `-Wl,--allow-shlib-undefined` on
Linux because of libproxy in the KDE content snap. Single target `playlist-dl`, installs to
`${CMAKE_INSTALL_BINDIR}`, `dist/linux/playlist-dl.desktop` and hicolor icons 16..512.
`scripts/dev-build.sh` / `dev-run.sh` build and run against `kde-qt6-core24-sdk` and `kf6-core24`.

**Resources:** `src/icons.qrc` (373 entries: app icons, Remix-style line icons, the "primo" icon
set with ~200 PNGs, category icons, placeholders), `WebEnginePlayer/js.qrc` (`ads.light`,
`skip.js`, `qwebchannel.js`), `WebEnginePlayer/css.qrc` (`core.css`, `scroll.css`).
`PlaylistDownloadWidget/icons.qrc` (98 entries, duplicates of the main set) is NOT in CMake and
was not in the `.pri` either: dead. No translations (`tr()` is used but no `.ts` files).

**Defines and flags at runtime:** `main.cpp` appends `--disable-web-security` to argv for
Chromium, sets `QTWEBENGINE_REMOTE_DEBUGGING=23654` in debug builds, app name `Playlist DL`,
org `org.keshavnrj.ubuntu` (so QSettings lands in `~/.config/org.keshavnrj.ubuntu/Playlist DL.conf`
and data in `~/.local/share/org.keshavnrj.ubuntu/Playlist DL/`), single instance via
`RunGuard("org.keshavnrj.ubuntu.Playlist DL")` (release only).

**Vendored third-party code (all inside `src/`, no `3rdparty/` directory):**

| Path | Lines | Purpose | Origin / licence |
|---|---|---|---|
| `waitingspinnerwidget.cpp/.h` | 393 | Spinner used in search, playlist view, account, toolbar | Alexander Turkin 2012-2014, W. Hallatt, J. Dawid; MIT (header intact) |
| `SlidingStackedWidget/` | 288 | Animated QStackedWidget (main pages, download options, download widget) | Tim Schneeberger (ThePBone) 2020, MIT; "inspired by qt.shoutwiki.com" |
| `circularprogessbar.cpp/.h` | 319 | Ring progress bar on download items | Author "缪庆瑞" 2019, Chinese comments, no licence stated |
| `qadvancedslider.cpp/.h` | 353 | Slider with coloured error/warning/optimal/best bands | No attribution; API and comments match VirtualBox's `QIAdvancedSlider` (GPL) |
| `scrolltext.cpp/.h` | 234 | Marquee label (download list title, playlist info) | No attribution; the widely copied Qt forum `ScrollText` |
| `elidedlabel.cpp/.h` | 98 | One-line eliding QLabel | No attribution |
| `rungaurd.cpp/.h` (sic) | 111 | QSharedMemory + QSystemSemaphore single instance | References habrahabr post 173281 |
| `WebEnginePlayer/fullscreenwindow.*`, `fullscreennotification.*` | 148 | Full screen handling | Verbatim from Qt's `webenginewidgets/videoplayer` example, unattributed |
| `onlinesearchsuggestion.cpp/.h` | 260 | Google suggest popup | Adapted from Qt's `googlesuggest` example |
| `WebEnginePlayer/js/qwebchannel.js` | 427 | QWebChannel client | Qt Company LGPL; bundled but never injected or referenced |
| `RateApp/` | 265 | "Rate this app" nag | Author's own, shared with his other apps |
| `remotepixmaplabel2.cpp/.h` | 163 | Async thumbnail label with embedded base64 retry icon | Author's own |

---

## 2. Source map

Line counts are `wc -l`. "Talks to" lists the classes or endpoints a file depends on.

### `src/` (application)

| File | Lines | Role | Notes |
|---|---|---|---|
| `main.cpp` | 48 | Entry point | Argv patch `--disable-web-security`, app/org name, `RunGuard`, shows `MainWindow` |
| `mainwindow.cpp/.h/.ui` | 511/84/43 | Shell window | Toolbar actions, `SlidingStackedWidget` with 5 pages, back-stack `stackVector`, dark/light palette, shared `QNetworkAccessManager` + `QNetworkDiskCache`, wires all pages, Settings/About/Account dialogs, RateApp, `Esc` filter |
| `playlistsearch.cpp/.h/.ui` | 282/62/71 | Home page: search | Calls the ktechpit search service, parses JSON array into `PlayListItem`s, URL detection regex, Force Reload (drops cache entry), `Esc` cancels |
| `onlinesearchsuggestion.cpp/.h` | 210/50 | Suggest popup on the search line edit | `http://suggestqueries.google.com` every 500 ms after typing, own `QNetworkAccessManager` |
| `playlistitem.cpp/.h/.ui` | 88/38/215 | Search result row | Thumbnail, title, author, count, first videos list, menu (View / Bookmark) |
| `playlistview.cpp/.h/.ui` | 352/73/259 | Playlist page | Runs `python3 core --dump-single-json --flat-playlist`, caches JSON in `playlist_cache/<id>`, `VideoItem` list, filter, select all, play buttons, copy URL |
| `videoitem.cpp/.h/.ui` | 74/41/191 | Playlist row | Checkbox, `#pos`, thumbnail, title, duration, play button wired straight to `MainWindow::playVideo` |
| `playlistdownloadoptions.cpp/.h/.ui` | 365/64/554 | Download options page | Audio/Video radio, container radios, three `QAdvancedSlider`s, location, writes `download_records/<pl>__S__<ms>.json`, pro gate |
| `settingwidget.cpp/.h/.ui` | 267/77/521 | Settings dialog | Owns `Engine`; writes 9 keys; emits theme/blocker/website/cache signals |
| `engine.cpp/.h` | 344/67 | yt-dlp binary manager | GitHub download with manual redirects, version JSON, `--rm-cache-dir`, `--version`, update dialogs |
| `account.cpp/.h/.ui` | 386/72/250 | Licensing dialog | 20-char id, 30-day trial, `check.php` purchase check, `.dbn` and `~/Downloads/.Playlist DL.id` |
| `claimoffer.cpp/.h/.ui` | 139/33/103 | "Claim offer" form | Never instantiated (`init_claimOffer()` commented out), uses `Request` |
| `request.cpp/.h` | 53/31 | GET helper | New QNAM + disk cache per call; only user is `ClaimOffer` (dead) |
| `about.cpp/.h/.ui` | 102/31/247 | About dialog | Donate / Rate / More apps / Debug info; source link hidden (`isOpenSource=false`) |
| `RateApp/rateapp.cpp/.h/.ui` | 118/49/98 | Rate nag | 5 launches or 5 days, 30 s delay, opens `snap://playlist-dl` |
| `spinner.cpp/.h/.ui` | 49/29/38 | Toolbar spinner wrapper | Wraps `WaitingSpinnerWidget` |
| `utils.cpp/.h` | 355/52 | Static helpers | Path makers under AppLocalData, JSON load/save, random id, cache size, `getMainWindow()` parent walk, `is_number` via std::regex |
| `remotepixmaplabel2.cpp/.h` | 126/37 | Thumbnail label | Uses the shared QNAM, aborts all its in-flight replies on re-init |
| `rungaurd.cpp/.h` | 79/32 | Single instance | See vendored table |
| `elidedlabel`, `scrolltext`, `qadvancedslider`, `circularprogessbar`, `waitingspinnerwidget` | see 1 | UI helpers | Vendored |
| `version.h.in` | 5 | Generated version header | `PLDL_VERSION`, `PLDL_GIT_REVISION` |

### `src/PlaylistDownloadWidget/` (download manager)

| File | Lines | Role | Notes |
|---|---|---|---|
| `downloadwidget.cpp/.h/.ui` | 633/74/339 | Downloads page | Two sliding sub-pages (playlist list, item list), loads all `download_records/*.json` on start, Start/Stop/Remove, filter, item info panel, `PLaylistInfo` dialog |
| `downloadmanager.cpp/.h` | 234/54 | Queue | `QList<DownloadProcess*>` queue, concurrency from `concurrent_downloads`, start/stop per playlist UUID |
| `downloadprocess.cpp/.h` | 480/89 | One video download | Two `QProcess` phases (`-F` probe, then download), progress regex parsing, per-video QSettings progress file |
| `downloaditem.cpp/.h/.ui` | 226/59/179 | Item row | Status colour strip, ring progress, "Downloaded X of Y, Speed" text, reads progress file |
| `playlistentryitem.cpp/.h/.ui` + `playlistitemoverlay.ui` | 313/75/246+147 | Playlist row | Overlay with type icon, container, count, A:/V: quality; per-playlist `index` counters; menu (View, Info, Open folder via `xdg-open`) |
| `playlistinfo.cpp/.h/.ui` | 112/25/251 | Details dialog | Shows record JSON fields, clickable download location |
| `helper.cpp/.h` | 48/15 | Static helpers | `getFormatName(0..100)` -> Poor/Low/Medium/Good/Best, `isDownloaded()` reads progress file status |
| `icons.qrc` + `icons/` | 98 files | Duplicate icon set | Not built |

### `src/WebEnginePlayer/` (embedded browser)

| File | Lines | Role | Notes |
|---|---|---|---|
| `webengineplayer.cpp/.h` | 418/211 | Player page | `QWebEngineView` on the default profile, `RequestInterceptor` (in the header), injected scripts/CSS, WebChannel `qt_helper`, history persistence, full screen, UA switch, login prompt |
| `toolbar.cpp/.h/.ui` | 76/46/225 | Vertical toolbar | Close Player, Back, Stop, Home, Reload, Forward, Restore previous session |
| `blocked/blocked.cpp/.h/.ui` | 89/40/176 | Blocked requests log | Counter in QSettings, red = ad, blue = tracker, auto clear every 2 min |
| `fullscreenwindow.*`, `fullscreennotification.*` | 49+29, 48+22 | Full screen | Qt example code |
| `js/skip.js` | 18 | Ad skip clicker | Every 300 ms clicks `ytp-ad-skip-button-text`, `ytp-ad-overlay-close-button`, `ytp-ad-skip-button` |
| `js/ads.light` | 589 | Block list | 104 unique host/path substrings plus ~485 hard-coded `rN---sn-xxxx.googlevideo.com` edge hosts, each listed with and without trailing slash |
| `js/qwebchannel.js` | 427 | Unused | Never injected |
| `css/core.css` | 96 | Cosmetic ad hiding, static header, no text selection | Injected on all frames |
| `css/scroll.css` | 34 | Red `#cb0000` scrollbars on `#161616` | Main frame only |

**Biggest files:** `downloadwidget.cpp` (633), `mainwindow.cpp` (511), `downloadprocess.cpp` (480),
`webengineplayer.cpp` (418), `account.cpp` (386), `playlistdownloadoptions.cpp` (365),
`playlistview.cpp` (352), `engine.cpp` (344). Biggest `.ui`: `playlistdownloadoptions.ui` (554),
`settingwidget.ui` (521), `downloadwidget.ui` (339).

---

## 3. Features as the user sees them

### 3.1 Main window

`QMainWindow`, min 600x400, title `Playlist DL`, icon `icon-64.png`, geometry/state saved in
`windowGeometry` / `windowState`. Central widget is a `SlidingStackedWidget` (650 ms, OutQuart)
holding five pages in this order: `PlaylistSearch` (home), `PlaylistView`,
`PlaylistDownloadOptions`, `DownloadWidget`, `WebEnginePlayer`. `switchStackWidget()` pushes the
current page on `stackVector` so the Back action pops it; Home clears the stack.

Toolbar (`Qt::ToolButtonTextBesideIcon`, not movable), left to right: **Back** (`prev.png`,
enabled when the stack is non-empty), separator, **Home**, expanding spacer, **Spinner**
(shown during searches, playlist loads, page loads), **Downloads**, **Player** (icon turns to the
red play icon while the web view holds a page other than `about:blank`; tooltip = page title),
separator, **Settings**, **Account** (`lock.png` "Account" when pro, `unlock.png` "Unclock" [sic]
otherwise), **About**. No menu bar, no keyboard shortcuts except a global `Esc` event filter that
aborts the search replies when the search page is current. No tray, no CLI arguments.

Switching pages calls `WebEnginePlayer::reset()` (loads the blank page unless `keepPlayer`) and
`DownloadWidget::home()`. Theme: `windowTheme` = `dark` sets Fusion style and a hard-coded
palette (`#262D31` window, `#323739` base, highlight `rgb(38,140,196)`); `light` restores the
start-up palette but leaves Fusion in place once dark was chosen. `updateWindowTheme()` also
re-connects a `currentItemChanged` handler on every `QListWidget` to paint the selected item widget
with the Highlight role.

### 3.2 Search (home page, `playlistsearch.*`)

Line edit (YouTube icon, clear button, placeholder "Search YouTube playlist or paste Playlist
Url"), **Search** button (text becomes **Process Playlist** when the regex
`.*(youtu.be\/|list=)([^#\&\?]*)` matches), **Force Reload** (removes the last URL from the
QNetworkDiskCache and searches again), results `QListWidget`. Suggestions popup
(`onlineSearchSuggestion`) fires 500 ms after typing unless the text contains `http`/`www.`.

Flow: Enter or Search -> `doSearch()` -> if the button says "Process Playlist" extract the id and
emit `loadPlaylist(id)`; otherwise the ktechpit search service
on the shared manager (disk cache, default cache policy). `processResult()` parses the body as a
JSON array; each object yields `title`, `playlistId`, `author`, `authorId`, `videoCount`,
`playlistThumbnail` (with `hqdefault` rewritten to `mqdefault`) and `videos[]` of
`{title, lengthSeconds, videoId}`. Each result becomes a `PlayListItem` (thumbnail via
`RemotePixmapLabel2`, title, "by", "N videos", a plain text list `- title (hh:mm:ss)` of the
preview videos, a menu button with **View Playlist** and **Bookmark Playlist**; Bookmark has no
handler). Double-click or View loads the playlist. `QApplication::processEvents()` runs after
every appended item.

Failure behaviour: reply error -> `QMessageBox::critical` "An error occured while search
<errorString with 'ktechpit.com/USS/' and '.php' stripped>"; `OperationCanceledError` (Esc) ->
silent; empty array, non-array JSON or HTML -> a disabled "No result found for your query X" row
and the cache entry for that URL is removed. No timeout, no retry, no fallback host.

### 3.3 Playlist view (`playlistview.*`)

Header: Playlist name (`ElidedLabel`), Author, Id, Item count; buttons **Play whole Playlist**,
**Play Author Uploads**, **Copy Url** (puts `https://www.youtube.com/playlist?list=<id>` on the
clipboard), **Force Reload** (deletes `playlist_cache/<id>` and reloads; only enabled when the
view came from cache). Then **Select All** checkbox, **Filter results** line edit (substring on
titles, hides rows), the `VideoItem` list, a status label "N items selected" and
**Next: Set Download Option** (enabled when N > 0). Double-click toggles the row checkbox.

Loading: `loadPlaylist(id)` resets the UI and calls `flat_playlist()`: if
`<AppLocalData>/playlist_cache/<id>` exists it is used forever (until Force Reload), otherwise
`python3 <AppLocalData>/core --dump-single-json --flat-playlist https://m.youtube.com/playlist?list=<id>`
runs in a `QProcess` (with a blocking `waitForStarted()` and a retry with the same program). The
JSON object gives `id`, `title`, `uploader`, `entries[] {id, title, duration}`; entries titled
`[Deleted video]` / `[Private video]` are skipped. Every `readyReadStandardError` chunk pops a
critical dialog "An error occured while processing playlist. Error code <exitCode>" even for
warnings, and `exitCode()` is read while the process still runs.

Each `VideoItem` row: checkbox, `#pos`, thumbnail `https://i.ytimg.com/vi/<id>/mqdefault.jpg`,
title, duration, **play** button -> `MainWindow::playVideo(id)`.

### 3.4 Playing (`WebEnginePlayer`)

`play(id)` loads `https://keshavbhatt.github.io/YtTest/?v=<id>`; `playPlaylist(id)` loads
`.../YtTest/?p=<id>`; `playAuthorUploads(name)` loads `.../YtTest/?u=<uploader display name>`
(the playlist's `uploader` string, not a channel id). Home button loads `https://youtube.com`.
The player page (a GitHub Pages site owned by the author, presumably an IFrame Player API wrapper)
is expected to call `qt_helper.onHovered(1)` over QWebChannel when a video is age restricted;
the app then shows "Age verification is required ... Play on YouTube?" and loads
`http://www.youtube.com/watch?v=...` (plain http) in the same view.

Toolbar (vertical, left of the view): **Close Player** (back to the blank page), **Back**,
**Stop**, **Home**, **Reload**, **Forward**, **Restore previous session** (deserialises the
`QWebEngineHistory` saved to `playerHistory/history` on destruction; enabled if that file is
larger than 1000 bytes). Back/Forward/Reload/Stop enablement follows `loadStarted`/`loadProgress`.

Profile: `QWebEngineProfile::defaultProfile()` with `ForcePersistentCookies` (ineffective on
Qt 6, see 7.13), `FullScreenSupportEnabled`, `ShowScrollBars` off while loading and on again
after 90 % unless `website_to_load` is `mobile`. User agent: desktop =
`Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:72.0) Gecko/20100101 Firefox/72.0`, mobile =
`Mozilla/5.0 (Linux; Android 6.0.1; Moto G (4)) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/90.0.4430.85 Mobile Safari/537.36`.

Injected via `QWebEngineScriptCollection` (all `Deferred`, `ApplicationWorld`):

| Name | Source | Sub-frames | What it does |
|---|---|---|---|
| `scroll` | `css/scroll.css` wrapped in a `<style id=scroll>` inserter | no | Red scrollbars |
| `core` | `css/core.css` | yes | Static masthead, hides `ytd-display-ad-renderer`, `.ytp-ad-image-overlay`, `[class*=promoted]`, `[id*=promo]`, `[id*=-ad-]`, `#player-ads`, `.ytp-pause-overlay`, `ytd-mealbar-promo-renderer`, links to youtubekids/music/tv, disables text selection and drag, `overflow-x:hidden` |
| `skipper` | `js/skip.js` | no | 300 ms interval clicking the three ad skip/close button classes |
| `theatre` | inline | no | On `yt-navigate-finish` clicks `button.ytp-size-button` if `#player-theater-container` is empty |
| `theatreCookie` | inline | no | `document.cookie='wide=1;path=/;domain=.youtube.com'` |
| `darkMode` | inline | no | `document.cookie='PREF=f6=400&f5=30000;path=/;domain=.youtube.com'` |

First run only (`player_firstrun`): at 70 % load of a youtube URL the PREF cookie is also set in
`MainWorld` and the page reloaded. `qwebchannel.js` from the qrc is never injected; the page must
load `qrc:///qtwebchannel/qwebchannel.js` itself.

`RequestInterceptor` (set on the profile, so it runs on Chromium's IO thread in Qt 6): loads
`:/js/ads.light` once; per request re-reads `adblocker`, `eventlogger`, `comments`; when
`adblocker` is on appends the whole store to the working list, adds `/log_event`, `/ptracking`,
`/api/stats/`, `/stats` when `eventlogger` is on, adds `comment_service_ajax?` when `comments` is
off, `removeDuplicates()`, then a linear case-insensitive `contains()` scan. URLs containing
`/accounts`, `accounts.youtube` or `signin=` are never blocked. Blocked requests emit
`blocked(url)` -> `Blocked::appendLog()` on the GUI thread, which increments
`adblocker_blocker_count` in QSettings and appends a coloured `<p>` to a `QTextBrowser`.

**Blocked log window:** "Total blocked: N", Clear, legend (Ads red `#ea0000`, Tracker blue
`#4682B4`, tracker = path contains `/log_event?`, `/ptracking?` or `api/stats/`), the log, Close.
Auto clears every 120 s. Opened from Settings ("See blocked requests" closes Settings); closing
it re-shows Settings.

**Full screen:** `fullScreenRequested` accepted; a `FullScreenWindow` takes the page into a new
`QWebEngineView`, hides the main window, shows "You are now in full screen mode. Press ESC to
exit!" fading after 2 s; Esc triggers `ExitFullScreen`; the destructor hands the page back and
restores the window geometry.

### 3.5 Download options (`playlistdownloadoptions.*`)

"Selected Items:" list (`VideoItem` rows with the checkbox hidden, built from the cached
playlist JSON filtered by the chosen ids; deleted/private are NOT filtered here, see 7.9), then
**Select operation type:** `Download Audio only` / `Download Video` radios switching a
`SlidingStackedWidget`:

- **audioPage:** "Select Audio container:" radios `opus`, `m4a`, `wav`, `mp3` (default), `aac`,
  `flac`, `vorbis` (objectName is the format string passed to yt-dlp); "Select Audio Quality:"
  `aAudioQualitySlider`.
- **videoPage:** "Select Video container:" `mkv` (default) / `mp4`; "Select Audio Quality:"
  `vAudioQualitySlider`; "Select Video Quality:" `vVideoQualitySlider`.

Every `QAdvancedSlider` is 0..100, step 10, snapping, bands error 0-20 / warning 20-40 / optimal
40-70 / best 70-100, label Poor (0-20), Low (21-40), Medium (41-70), Good (71-90), Best (91-100),
default 100. Below: "Download location" line edit (default `download_path`) with **Change**
(directory dialog, must be writable), `statusLabel` such as
`Selected format: VIDEO(mkv) | Best Video | Best Audio`, and **Add to Download**.

Add: `account::check_pro(true)`; if any chosen slider is > 20 ("pro feature") and the trial is
over and the account is not pro -> "Evaluation period ended, Upgrade to Pro" dialog and abort.
Otherwise `generateDownloadRecordFile()` writes
`<AppLocalData>/download_records/<playlistId>__S__<msSinceEpoch>.json`:

```json
{ "playlist_meta": [ { "id", "title", "uploader" } ],
  "download_params": [ { "download_type": "audio|video", "audio_quality": 0..100,
                         "video_quality": 0..100, "container": "mp3|...|mkv|mp4",
                         "download_location": "<location>/<playlist title>-<CONTAINER>/" } ],
  "items": [ <entries objects from the playlist JSON> ] }
```

`MainWindow` then slides to the Downloads page and, after a 500 ms timer "so the animation can be
seen", calls `DownloadWidget::addToDownload(file, true)`.

### 3.6 Downloads page (`DownloadWidget`, `DownloadManager`, `DownloadProcess`)

**Page 1 (playlist list):** "Downloads:" title, "Filter Playlists" line edit, **Start/Stop**
button (label and icon follow whether the selected playlist UUID has a Starting/Running process),
**Remove**, and a `QListWidget` of `PlaylistEntryItem` rows: coloured status strip, thumbnail of
the first video, black gradient overlay with type icon (music/film), container, `C: <count>`,
`A:<quality>` and `V:<quality>`, title, "by", and a status line
`Downloaded X of Y      Failed: Z      Queued: W` read from `progress/<UUID>/index`. Row menu:
**View Playlist**, **Show Playlist Info**, **Open Download Location** (`xdg-open` with a
blocking `waitForFinished()`, fallback `QDesktopServices`). On start-up every
`download_records/*.json` (sorted by time) is re-added; nothing auto-starts.

**Page 2 (item list):** **Go back** button, a `ScrollText` marquee
`Name: <title> | By: <uploader> | Format: VIDEO(mkv)` (scrolls on hover), info button, list of
`DownloadItem` rows (status strip, title, duration, `Status: idle|starting|running|paused|
finished|error|queued`, ring progress bar with the exact percent), and a bottom info panel
(thumbnail, title, id, duration) for the selected row. Rows bind to a live `DownloadProcess`
when one exists (`addedNewDownloadProcess` / `getDownloadProcess("__DP__"+UUID)`).

**Status colours:** error `rgb(204,0,0)`, running `rgb(0,204,0)`, paused `rgb(239,167,75)`,
idle transparent, finished `rgb(110,179,228)`, queued `rgb(245,241,104)`.

**Queue (`DownloadManager`):** `addDownload(record)` creates one `DownloadProcess` per item whose
`progress/<UUID>/<videoId>` file does not say `status=finished`, names it
`__DP__<UUID>__V__<videoId>`, marks it queued and calls `startDownloader()`, which starts queued
processes while the number of children in Starting/Running is below `concurrent_downloads`
(default 2, spin box 1..10). `stopDownload(UUID)` marks every matching process idle/paused,
`QProcess::close()`s them (kill), deletes them and calls `startDownloader()` again. There is no
per-item pause; a whole playlist entry is started or stopped.

**One item (`DownloadProcess`):** phase 1 `python3 <core> -F <videoId>` (bare id); on exit 0 the
stdout is split at `"resolution note"` (youtube-dl's old header; with yt-dlp the split is a
no-op), every line containing `audio only` / `video only` contributes its first token to
`audioFormatCode` / `videoFormatCode` (so yt-dlp's `-drc` and storyboard variants are included
and combined formats ignored). Phase 2 argv (see section 4) is built from the format lists and
the sliders and persisted as `formatCode` in the progress file, so a restarted item skips the
probe. Progress is parsed from `readyRead()` chunks (section 4). `finished(int)` exit 0 ->
`status=finished`, `progress=100.0`; anything else -> `status=error`. "Resume" means re-running
yt-dlp with the same `-o`, relying on its `.part` continuation.

**Remove:** stops the playlist, deletes the record JSON and `progress/<UUID>/`; downloaded media
is left in place.

### 3.7 Settings dialog (`settingwidget.*`, a `QWidget` with `Qt::Dialog`)

Group **Download Engine settings:** Status line edit (Present / Absent / Downloading... / Host
not Found), loading gif, **Download/Update** ("Click, if video downloads are not working"),
**Clear Cache** ("if you are getting 403 or 421 error messages").
Group **Application settings:** Default download location + **Change**; Number of concurrent
downloads (1..10) + **Warning!** popup about throttling; Theme **Light** / **Dark**.
Group **Player settings:** Website version **Desktop** / **Mobile** (UA switch, asks to reload);
**Keep the player running** + More info; **Disable YouTube Event Logger** + What are Event
Loggers; **Enable YouTube adBocker** [sic] + **See blocked requests**; **Load Comments on Videos**.
Group **Application Cache and Cookies:** "Application Cache" size (measures the
`QNetworkDiskCache` directory) + **Delete Cache** (clears the WebEngine HTTP cache instead).
Geometry saved as `settingsGeo`.

### 3.8 Account, claim offer, rate, about

See section 6 for licensing. **Account dialog:** Account Id, Account type (Pro / Evaluation),
Evaluation time left, status line with spinner, **Restore Previous Purchase** (re-runs the check),
**Purchase Licence** (blinks three times when shown; opens the checkout URL externally), hidden
"Offer" group with **Learn how to claim this offer**, "Support email: keshavnrj@gmail.com" shown
only when pro, otherwise "In pro version only". **ClaimOffer** (dead code): account id, email,
video id (Claim enabled at 11+ chars), overlay with progress and Cancel, posts to
`offer/index.php`, remembers `claim_submitted`. **RateApp:** shown 30 s after launch once
`app_launched_count >= 5` or 5 days since `app_install_time`; buttons Later (resets counters),
Already Done (`rated_already=true`), Rate Now (`snap://playlist-dl`). **About:** name, "Playlist
downloader Application for Linux Desktop", author/email/website, version, **Donate**
(`https://paypal.me/keshavnrj/5`), **Rate in Store** (`snap://playlist-dl`), **Source Code**
(hidden), **More Application by Developer** (`https://snapcraft.io/publisher/keshavnrj`),
**Debug Info** (version, build date/time, Qt versions, OS, arch).

### 3.9 QSettings keys

Main store: `QSettings()` = `~/.config/org.keshavnrj.ubuntu/Playlist DL.conf` (under the snap:
`~/snap/playlist-dl/current/.config/...`). 25 keys:

| Key | Default | Written by | Read by |
|---|---|---|---|
| `windowGeometry` | none | `mainwindow.cpp` closeEvent | `mainwindow.cpp` ctor |
| `windowState` | none | same | same |
| `settingsGeo` | none | `settingwidget.cpp` closeEvent (stores a `QRect`) | `mainwindow.cpp` (`toByteArray()` -> `restoreGeometry`, never matches) |
| `windowTheme` | `light` | `settingwidget.cpp` | `mainwindow.cpp`, `settingwidget.cpp` |
| `download_path` | `<Downloads>/Playlist DL` | `settingwidget.cpp` (on every text change) | `settingwidget.cpp`, `playlistdownloadoptions.cpp` |
| `concurrent_downloads` | `2` | `settingwidget.cpp` | `settingwidget.cpp`, `downloadmanager.cpp` (per `startDownloader` call) |
| `comments` | `true` | `settingwidget.cpp` | `webengineplayer.h` interceptor (per request) |
| `adblocker` | `true` | `settingwidget.cpp` | interceptor (per request) |
| `eventlogger` | `true` | `settingwidget.cpp` | interceptor (per request) |
| `keepPlayer` | `true` | `settingwidget.cpp` | `webengineplayer.cpp` reset() |
| `website_to_load` | `desktop` | `settingwidget.cpp` | `webengineplayer.cpp` (x2), `settingwidget.cpp` |
| `showSearchSuggestion` | `"true"` | nobody (no UI) | `onlinesearchsuggestion.cpp` |
| `embed_thumbnail_to_audio` | `false` | nobody (no UI) | `downloadprocess.cpp` |
| `lastVisited` | none | `webengineplayer.cpp` (base64 URL on every load) | nobody |
| `player_firstrun` | `true` | `webengineplayer.cpp` | `webengineplayer.cpp` |
| `adblocker_blocker_count` | `0` | `blocked.cpp` (per blocked request) | `blocked.cpp` |
| `accountId` | none | `account.cpp` | `account.cpp` |
| `Playlist DL` (= applicationName) | none | `account.cpp` (base64 `activated` or `sfjhkfngkj`) | `account.cpp` |
| `Playlist DL_emit` | none | `account.cpp` (base64 trial start epoch) | `account.cpp` |
| `claim_submitted` | none | `claimoffer.cpp` | `claimoffer.cpp` |
| `submitted_video_id` | none | `claimoffer.cpp` | `claimoffer.cpp` |
| `submitted_email_id` | none | `claimoffer.cpp` | `claimoffer.cpp` |
| `app_launched_count` | `0` | `rateapp.cpp` | `rateapp.cpp` |
| `app_install_time` | none | `rateapp.cpp` | `rateapp.cpp` |
| `rated_already` | `false` | `rateapp.cpp` | `rateapp.cpp` |

Side stores (`QSettings(path, NativeFormat)` files under `<AppLocalData>`):

| File | Keys | Written by | Read by |
|---|---|---|---|
| `progress/<pl>__S__<ms>/<videoId>` | `status` (queued/starting/running/idle/paused/finished/error), `formatCode`, `progress`, `progress_exact`, `size`, `downloaded`, `eta`, `speed`, `hasProgressMap`, `statusStyle`, plus copies of `download_location`, `download_type`, `container`, `audio_quality`, `video_quality` | `downloadprocess.cpp`, `downloaditem.cpp` | `downloadprocess.cpp`, `downloaditem.cpp`, `helper.cpp` |
| `progress/<UUID>/index` | `finished`, `failed`, `queued` | `playlistentryitem.cpp` | `playlistentryitem.cpp` |
| `playerHistory/history` | `history` (serialised `QWebEngineHistory`) | `webengineplayer.cpp` dtor | toolbar "Restore previous session" |

Total: 25 + 15 + 3 + 1 = 44 distinct keys. Other files under `<AppLocalData>`: `core`,
`core_version`, `playlist_cache/<playlistId>`, `download_records/*.json`, `.dbn`; under
`~/Downloads`: `.Playlist DL.id`. Thumbnail and API responses go to the `QNetworkDiskCache` at
`~/.cache/org.keshavnrj.ubuntu/Playlist DL/`.

### 3.10 What users get (feature list for the scope contract)

- Search YouTube playlists by keyword, with Google search suggestions, or paste a playlist URL.
- Browse a playlist: numbered items with thumbnails and durations, filter, select all/individual.
- Play a single video, the whole playlist, or "author uploads" in an embedded browser page.
- Embedded YouTube browser with back/forward/home/reload/stop, session restore, full screen,
  desktop or mobile site, dark theme cookie, theatre mode, ad request blocking, ad skip clicker,
  cosmetic ad hiding, tracker blocking, optional comment blocking, blocked request log.
- Download selected videos as audio (opus, m4a, wav, mp3, aac, flac, vorbis) or video (mkv, mp4)
  with three 0..100 quality sliders mapped onto yt-dlp's format list.
- Per-playlist download entries persisted on disk, start/stop/remove, concurrency 1..10,
  resume by re-running yt-dlp, per-item progress ring, speed/size/ETA text, playlist counters.
- Open the download folder, show playlist details, copy playlist URL, force reload cache.
- Settings: engine download/update/cache clear, download folder, concurrency, light/dark theme,
  player options, cache size/clear.
- yt-dlp engine fetched from GitHub on first run with a version check.
- 30-day evaluation with a paid "Pro" licence gating quality above "Poor"; rate-app nag; About.

---

## 4. Network and external processes

### 4.1 Endpoints

| # | URL | Scheme | Purpose, where |
|---|---|---|---|
| 1 | the ktechpit search service | https | Playlist search ("ktechpit based search", shared with the author's Olivia app; response is Invidious `search?type=playlist` shaped JSON). `playlistsearch.cpp:89` |
| 2 | `http://suggestqueries.google.com/complete/search?ds=yt&client=youtube&hjson=t&cp=1&format=5&alt=json&q=<text>` | http | Search suggestions, parsed as nested JSON arrays. `onlinesearchsuggestion.cpp:5` |
| 3 | `https://i.ytimg.com/vi/<videoId>/mqdefault.jpg` | https | Thumbnails everywhere (`playlistitem`, `videoitem`, `playlistview`, `playlistdownloadoptions`, `downloadwidget`, `playlistentryitem`) |
| 4 | `https://m.youtube.com/playlist?list=<id>` | https | Fed to yt-dlp for flattening. `playlistview.cpp:68` |
| 5 | `https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp` | https | Engine binary, manual 3xx following. `engine.cpp:17` |
| 6 | `https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest` | https | Engine version (`tag_name`). `engine.cpp:11` |
| 7 | `https://keshavbhatt.github.io/YtTest/?v=<id>`, `?p=<id>`, `?u=<name>` | https | Player page. `webengineplayer.cpp:344-360` |
| 8 | `https://youtube.com` | https | Player home. `webengineplayer.cpp:22` |
| 9 | `http://www.youtube.com/<watch?v=|playlist?list=|c/><x>` | http | Age restricted fallback. `webengineplayer.cpp:327` |
| 10 | `http://www.ktechpit.com/USS/PLDL/checkout/chkout/check.php?username=<accountId>` | http | Purchase check. `account.cpp:132` |
| 11 | `http://ktechpit.com/USS/PLDL/checkout/index.php?accountId=<id>` | http | Buy page (external browser). `account.cpp:349` |
| 12 | `http://ktechpit.com/USS/PLDL/offer/index.php?acc_id=&email_id=&v_id=` | http | Offer claim (dead). `claimoffer.cpp:127` |
| 13 | `https://paypal.me/keshavnrj/5`, `https://snapcraft.io/publisher/keshavnrj`, `snap://playlist-dl`, `http://ktechpit.com`, `https://github.com/keshavbhatt/playlist-dl` | mixed | About / Rate links |
| 14 | `https://www.youtube.com/playlist?list=<id>` | https | Clipboard text only |

Commented-out legacy engine sources in `engine.cpp`: `https://rg3.github.io/youtube-dl/update/LATEST_VERSION`,
`https://yt-dl.org/downloads/latest/youtube-dl`, `http://ktechpit.com/USS/engine/core.eco`,
`http://ktechpit.com/USS/engine/core_version`. No Invidious instance is contacted directly.

### 4.2 The search endpoint in detail

Request: plain GET with the default Qt user agent, no API key, on the shared
`QNetworkAccessManager` whose `QNetworkDiskCache` may serve a cached body (the app never sets a
cache policy on this request; Force Reload removes the entry). Expected body: a JSON array of

```json
{ "title": "...", "playlistId": "PL...", "author": "...", "authorId": "UC...",
  "videoCount": 12, "playlistThumbnail": "https://i.ytimg.com/vi/<id>/hqdefault.jpg",
  "videos": [ { "title": "...", "videoId": "...", "lengthSeconds": 213 } ] }
```

Parsing: `QJsonDocument::fromJson`, `array()`; missing fields become empty strings / 0; a missing
thumbnail falls back to `https://i.ytimg.com/vi/<playlistId>/mqdefault.jpg` (which is not a valid
video id). Failure: any transport error is shown in a critical box; any non-array body is
reported as "No result found". The app has no way to know whether the PHP proxy or the Invidious
instance behind it is down; both look like "no results".

### 4.3 Child processes

Engine path: `<AppLocalData>/core` (`Engine::enginePath()`), always executed as
`python3 <core> ...`; never `chmod`ed, never invoked directly. `python3` must exist on the host or
in the snap; `ffmpeg` is required by yt-dlp for every audio extraction and every video+audio
merge and is neither bundled nor checked.

| Purpose | argv | Where |
|---|---|---|
| Version | `python3 <core> --version` | `engine.cpp` (start-up, after download) |
| Clear cache | `python3 <core> --rm-cache-dir` | `engine.cpp` (button, after download) |
| Flatten playlist | `python3 <core> --dump-single-json --flat-playlist https://m.youtube.com/playlist?list=<id>` | `playlistview.cpp` |
| Format probe | `python3 <core> -F <videoId>` | `downloadprocess.cpp:startGetFormatsProcess` |
| Video download | `python3 <core> -f <videoCode>+<audioCode> --merge-output-format <mkv|mp4> -o <download_location>%(title)s.%(ext)s <videoId>` | `downloadprocess.cpp:getDownloadArgs` |
| Audio download | `python3 <core> --add-metadata --extract-audio --audio-format <container> --audio-quality <0..10> --prefer-ffmpeg [--embed-thumbnail if embed_thumbnail_to_audio and mp3] -o <download_location>%(title)s.%(ext)s <videoId>` | same |
| Open folder | `xdg-open <dir>` (blocking `waitForFinished`) | `playlistentryitem.cpp`, `playlistinfo.cpp` |

Format mapping: `roundFormat(count, slider) = abs(int(count * slider / 100 - 1 + 0.5))` indexes
the `-F` list (yt-dlp prints worst to best, so 100 = last = best, 0 = first); count 0 -> code
`"0"`, giving `-f 0+0`. `roundAudioFormat(slider) = abs(int((slider/100 - 1) * 10 + 0.5))` maps
100 -> `--audio-quality 0` (best VBR) and 0 -> 10. No `--newline`, no `--progress-template`, no
`--no-playlist`, no cookies, no rate limit, no subtitles, no `--restrict-filenames`, no
`%(playlist_index)s` (the commented example in `playlistview.cpp` shows it was considered).

Progress parsing (`downloadProcessReadyRead`, stdout only): if the chunk contains `[download]`
and none of `[download] Destination:`, `Merging formats into`, `Resuming download`, `[ffmpeg]`,
`fragments`: percent = regex `(\d+\.\d+%)` with the last three chars chopped (so `12.3%` ->
`12`); exact percent = regex `(\d.+%)` minus `%`; speed = text between ` at ` and ` ETA`; eta =
after `ETA `; size = between `% of ` and ` at `; unit = first alphabetic run of size; downloaded =
exact% x size. Non-numeric results fall back to the previous values in the progress file. The six
values are written to the per-video QSettings file on every chunk.

### 4.4 Engine lifecycle (`Engine`)

1 s after Settings is constructed (Settings is built inside `MainWindow`, so at start-up):
`checkEngine()` (file exists and size > 0). Absent -> modal "needs to download it's engine ...
(1.4Mb & is based on youtube-dl with some modifications)" with Ok / Cancel / Quit; Ok emits
`openSettingsAndClickDownload` (shows Settings and starts the download), Cancel calls
`evoke_engine_check()` again (the dialog re-opens immediately, forever), Quit quits. Present ->
`--version` and `check_engine_updates()`: reads `core_version`, fetches endpoint 6, compares via
`QDate::fromString(s, Qt::ISODate)` on both sides with `n_year > year || n_month > month ||
n_day > day`, and shows an Ok/Cancel/Quit "Engine update available" dialog. Download: the target
file is opened with `Truncate` before the request, redirects are followed by hand with a fresh
unparented `QNetworkAccessManager`, then `--rm-cache-dir`, the version JSON is written, and
`engineDownloadSucceeded` is emitted (also on failure, "fake UI fixer").

### 4.5 Cookies and sessions

Only the WebEngine profile holds cookies (`PREF`, `wide`, YouTube login). Nothing is exported to
yt-dlp. The Qt network side uses no cookies. The only "session" is the serialised
`QWebEngineHistory` and the `lastVisited` key nobody reads.

---

## 5. Packaging and release

**Snap (current, `snap/snapcraft.yaml`):** name `playlist-dl`, `base: core24`, `confinement:
strict`, `grade: stable`, `compression: lzo`, amd64 only, `adopt-info` with the version read from
`CMAKE_PROJECT_VERSION` in the CMake cache, `extensions: [kde-neon-6]` (Qt 6 from the
`kf6-core24` content snap), `plugin: cmake` with `-DCMAKE_BUILD_TYPE=Release
-DCMAKE_INSTALL_PREFIX=/usr`, build snap `cmake/latest/stable`, build packages `libxkbcommon-dev
libdrm-dev`, stage packages `libdrm2 libxcb-shape0 fonts-noto-core`, icon copied to
`meta/gui/icon.png`. Plugs: `audio-playback browser-support desktop desktop-legacy home
mount-observe network network-status opengl removable-media screen-inhibit-control unity7
wayland x11`. Neither `python3`, `ffmpeg` nor `xdg-open` is staged; `python3` comes from the
base, ffmpeg does not exist inside the snap at all.

**Snap (2.2, before the port):** `base: core20`, beineri `opt-qt-5.15.4-focal` PPA added as a
package repository, `build-src` part with `plugin: nil` cloning `https://github.com/keshavbhatt/playlist-dl.git`
and running `qmake src && make -j4` by hand, `desktop-launch` from the author's
`qt515-core20` content snap (`SNAP_DESKTOP_RUNTIME=$SNAP/qt515-core20`), env `DISABLE_WAYLAND=1
IS_SNAP=1 QT_QPA_PLATFORMTHEME=gtk3`, gtk/icon/sound theme content plugs, extra plugs
`avahi-observe camera gsettings upower-observe`.

**Desktop file:** `dist/linux/playlist-dl.desktop` (`Name=Playlist-dl`, `Exec=playlist-dl`,
`Icon=playlist-dl`, `Categories=Utility;Video;Qt;`, `Keywords=YouTube;Playlist`).

**Flathub, CI, tags:** none. Releases were cut by editing `VERSION` in the `.pro` and `version:`
in `snapcraft.yaml` (commits "chore(snap): bump version 2.1", "chore: versionbump 2.1 + qt
5.15.4 build dep") and pushing to the Snap Store as `playlist-dl`. There are no git tags.

---

## 6. Accounts and licensing

- **Module:** `account.cpp` (`Last Edited: Fri Mar 26 18:53:35 IST 2021`), app code
  `serverUID = "PLDL"`, evaluation `ev_time = 86400 * 30` (30 days; commit "chore: 30 days
  trial" raised it).
- **Account id:** `utils::generateRandomId(20)` (UUID without braces/dashes, trimmed to 20),
  stored in QSettings `accountId` and in `~/Downloads/.Playlist DL.id` (first line). If QSettings
  has no id the file is read back and its second line (base64 trial start) is restored into
  `Playlist DL_emit`.
- **Trial start:** `write_evaluation_val()` writes the epoch (base64) to `Playlist DL_emit`, to
  `<AppLocalData>/.dbn` and appends it to the `.id` file; `.dbn` wins on later runs.
- **Purchase check:** `GET http://www.ktechpit.com/USS/PLDL/checkout/chkout/check.php?username=<id>`
  on an unparented, never deleted `QNetworkAccessManager`. Body containing `Account is active`
  -> Pro (`Playlist DL` = base64 `activated`), otherwise Evaluation (`Playlist DL` = base64
  `sfjhkfngkj`); `plan expired on` in the body pops a "Purchase Licence" dialog. On network
  error the previous stored state is kept. The error text shown replaces `www.ktechpit.com` with
  a random fake IPv6 string (`utils::randomIpV6()`) to hide the host.
- **What is gated:** only `PlaylistDownloadOptions::is_pro_feature()`: any quality slider above
  20 once `evaluation_used && !pro`. Concurrency, containers, search, player are never gated.
- **Buy:** `http://ktechpit.com/USS/PLDL/checkout/index.php?accountId=<id>` in the system
  browser. **Restore:** re-runs the check. **Offer:** `ClaimOffer` never constructed.
- **Bypass:** writing base64 `activated` into the `Playlist DL` key makes the app Pro; the
  check is plain http and substring based.

---

## 7. Defects worth not repeating

1. **Ad list store grows quadratically at construction.** `RequestInterceptor()` does
   `adsUrl.append(line); adsUrlStore.append(adsUrl);` per line, so the store ends with
   1+2+...+589 = 173,755 entries. `interceptRequest()` then appends the whole store to the
   working list on every request and calls `removeDuplicates()`, plus three `QSettings` reads,
   before a linear substring scan. Every network request pays for this. (`webengineplayer.h`)
2. **Hard-coded googlevideo edge hosts and `manifest.googlevideo.com`, `s.ytimg.com`,
   `www.youtube-nocookie.com` in the block list.** Region specific, stale, and able to break
   playback (DASH/HLS manifests, embedded player). `#`-prefixed lines are not comments.
3. **Ad blocker toggle is only half wired.** `on_adblockCheckBox_toggled` removes/inserts the
   `skipper` and `core` scripts and asks for a reload the user may decline, leaving the
   collection and the page out of sync; if only one of the two scripts is present both are
   inserted again (duplicate); `scroll`, `theatre`, `darkMode` and the interceptor list are not
   touched; the checkbox also fires during Settings construction. The Qt 5 version re-inserted
   the scripts on every second toggle (fixed in the port). (`settingwidget.cpp`, `webengineplayer.cpp`)
4. **Per-request QSettings write and unbounded QTextBrowser append in the blocked log**
   (`Blocked::appendLog`), on the GUI thread, for every blocked request.
5. **Engine update check is dead.** yt-dlp tags look like `2025.09.05`; `QDate::fromString(...,
   Qt::ISODate)` needs dashes, so both dates are invalid and no update is ever offered once
   `core_version` exists. When the file is missing, the fallback `2019.01.01` is also invalid
   while the remote side is `currentDate()`, so the dialog always appears. The comparison
   `n_day > day` is wrong anyway. (`engine.cpp:compare_versions`)
6. **Engine download truncates the existing binary before the request** and `evoke_engine_check()`
   re-opens its modal dialog immediately on Cancel (infinite loop). Redirect hops leak a
   `QNetworkAccessManager` each. "fake UI fixer" emits success on failure. (`engine.cpp`)
7. **Format list parsed from `-F` human output**, split on the youtube-dl header string
   `"resolution note"`; only lines with `audio only`/`video only` are used, so `-drc`, storyboard
   and combined formats are mis-handled and an empty list yields `-f 0+0`. Progress is scraped
   from `[download]` lines with `split(" at ")`/`split("ETA ")` and two regexes; no `--newline`
   or `--progress-template`. (`downloadprocess.cpp`)
8. **Blocking calls on the GUI thread:** `waitForStarted()` in `flat_playlist`,
   `startGetFormatsProcess`, `startDownloadProcess` (retrying with the identical program on
   failure), `xdg_open.waitForFinished()` (30 s) in two places, modal `QMessageBox::exec()` from
   `Engine` timers at start-up, and `QApplication::processEvents()` inside four list population
   loops (guarded only by a `quiting` flag on `MainWindow`).
9. **Deleted/private filter inverted** in `PlaylistDownloadOptions::loadToView` and
   `DownloadWidget::loadPlaylist`: `!a || !b` is always true, so those entries reach the record
   file and the download queue. Only `PlaylistView` filters correctly.
10. **Unsanitised paths and titles.** `download_location` is `<dir>/<playlist title>-<CONTAINER>/`
    with the raw title (slashes, quotes), and `-o <dir>%(title)s.%(ext)s` has no
    `--restrict-filenames`; two videos with the same title overwrite or "resume" each other.
11. **Stale download state after quit or crash.** Progress files keep `status=running`; on the
    next start rows show a green "running" strip and old speed text with no process behind them;
    nothing resumes automatically. Quit kills yt-dlp mid-merge (QProcess destructor), losing the
    merge step.
12. **Playlist stderr handler pops a dialog per chunk** (`flattererror`), including yt-dlp
    warnings, and reads `exitCode()` of a running process. The `QProcess` is never deleted.
13. **Qt 6 port regression: `QWebEngineProfile::defaultProfile()` is off-the-record in Qt 6**, so
    `ForcePersistentCookies`, `clearHttpCache()` and the YouTube login/PREF cookies do not persist
    across runs (verify on a build; a named persistent profile is the fix).
14. **`--disable-web-security` is appended to Chromium argv in every build** and the age
    restricted fallback loads `http://www.youtube.com` in plain http.
15. **Account state kept in loose files under `$HOME`,** and the purchase check leaks a QNAM
    and a reply per call.
16. **Shared `QNetworkAccessManager` abuse.** `RemotePixmapLabel2::init` aborts every in-flight
    reply of the shared manager and `disconnect()`s the manager when a label is re-used (the
    Downloads info panel does this on every row change); `MainWindow::cancelAllRequests` and
    `PlaylistSearch::cancelAllRequests` return early on the first unreadable reply; replies in
    `RemotePixmapLabel2` are never `deleteLater()`ed (commented out); `PlaylistView` looks the
    manager up with `parent()->findChild<QNetworkAccessManager*>()`.
17. **`settingsGeo` type mismatch:** written as `QRect`, read with `toByteArray()` into
    `restoreGeometry()`; the settings window never restores its geometry.
18. **Connections accumulate:** `PlaylistDownloadOptions::resetUi()` re-connects every radio
    button's `toggled` on each visit; `DownloadWidget::updatePlaylisInfoButton` relies on
    `disconnect()` of the whole button; `VideoItem` connects to `MainWindow` through a
    `parent()` walk and `dynamic_cast` at construction.
19. **Dead code and dead settings:** `ClaimOffer`, `Request`, `qwebchannel.js`,
    `PlaylistDownloadWidget/icons.qrc`, "Bookmark Playlist" menu entry, `lastVisited`,
    `showSearchSuggestion` and `embed_thumbnail_to_audio` (no UI), `DownloadProcess::formatReady`
    property plumbing, `dp = nullptr; delete dp;` and `networkManager_ = nullptr; delete
    networkManager_;` no-ops, the "push" hidden `QPushButton` used as a signal relay in
    `MainWindow::init_account`, `utils::getMainWindow` `dynamic_cast` everywhere.
20. **Cache label and Delete Cache disagree:** the label measures the Qt disk cache directory,
    the button clears the WebEngine HTTP cache. Playlist JSON cache never expires except by
    Force Reload.
21. **Search suggestions over plain http** with an unencoded query, a bespoke nested-array
    parser, and `client=youtube` which historically returns a JSONP wrapper (`window.google.ac.h`)
    that `QJsonDocument` rejects; suggestions silently do nothing when that happens.
22. **Single point of failure for search:** one PHP proxy on the author's shared host, no
    timeout, no fallback, errors indistinguishable from "no results"; `playAuthorUploads` sends
    the uploader display name where the player expects a channel id.
23. **Concurrency bookkeeping via `findChildren` and object names** (`__DP__`, `__FM__`,
    `__IDP__`, `item_<id>`, `"push"`), `contains(UUID)` substring matching, per-playlist counters
    kept in a QSettings file that is reset on every UI load, `getStatus()` hitting the disk each
    call. The concurrency limit is re-read from QSettings inside the scheduling loop.
24. **UI strings with typos** baked into behaviour: "Unclock", "adBocker", "Continer Type",
    "Unale to locate directory", "An error occured", "it's engine". Ids and class names
    misspelt (`rungaurd`, `circularprogessbar`, `PLaylistInfo`, `homePaegUrl`,
    `playAuthotUploads`, `statisInfoLabel`).
25. **ffmpeg never bundled or detected** in the snap, so audio extraction and every video+audio
    merge fail under strict confinement unless the host happens to be on `PATH` (it is not).

---

## 8. What to keep verbatim

- **Container list and defaults:** audio `opus m4a wav mp3 aac flac vorbis` (default mp3), video
  `mkv mp4` (default mkv); quality band names Poor / Low / Medium / Good / Best and the
  0-20 / 21-40 / 41-70 / 71-90 / 91-100 thresholds; `--audio-quality` 0..10 mapping.
- **Status colours** (error `204,0,0`, running `0,204,0`, paused `239,167,75`, finished
  `110,179,228`, queued `245,241,104`) and the dark palette in `MainWindow::updateWindowTheme()`.
- **Download record JSON shape** (`playlist_meta`, `download_params`, `items`) so old records can
  be imported.
- **Data locations** (`org.keshavnrj.ubuntu` / `Playlist DL`, `download_records`,
  `playlist_cache`, `progress`) for migration, even if the rewrite moves them.
- **Cosmetic rules in `core.css`** worth carrying: `ytd-display-ad-renderer`,
  `.ytp-ad-image-overlay`, `.video-ads .ytp-ad-module`, `[class*=promoted]`, `#player-ads`,
  `.ytp-pause-overlay`, `ytd-mealbar-promo-renderer`, `#masthead-ad`,
  `ytd-promoted-video-renderer`, `ytd-compact-promoted-video-renderer`; the skip classes in
  `skip.js`; the tracker paths `/log_event`, `/ptracking`, `/api/stats/`; the sign-in whitelist
  (`/accounts`, `accounts.youtube`, `signin=`); the 104 non-googlevideo host rules in
  `ads.light`.
- **YouTube cookies:** `PREF=f6=400&f5=30000` (dark), `wide=1` (theatre), the Firefox 72 desktop
  UA that keeps Google sign-in working in an embedded Chromium.
- **Icons:** `src/icons/app/icon-*.png`, the Remix line icons and the `primo` set (check
  licences before shipping), `wall_placeholder_180.jpg`, `no-results-hub_128.png`.
- **Strings worth keeping as-is:** the desktop entry, "Search YouTube playlist or paste Playlist
  Url", the concurrency warning text, the "What are Event Loggers" explanation.
- **Vendored widgets with clear licences:** `WaitingSpinnerWidget` (MIT), `SlidingStackedWidget`
  (MIT), Qt's full screen example pair; `RunGuard` if single instance stays a requirement.
