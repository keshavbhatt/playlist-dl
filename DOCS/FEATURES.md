# FEATURES: the scope contract

Decision legend: KEEP (in 3.0), LATER (after it), DROP (never).
Status legend: `todo`, `wip`, `done (class)`, `verified` (exercised live on the owner's desktop).

Sources: `reference/analysis-playlist-dl-v2.md` (the 2.x app), the rewrite kit's Red FEATURES
and UMD 7's FEATURES for the desktop-shell and browser features that proved themselves.

The owner's brief (2026-09-24): "we want to re-write the application: implement a browser
shell like ultimate-media-downloader, give engine based search as fallback if the ktechpit
based search is not working, follow the rewrite kit where needed, make our own theming with
the brand theme according to the new icon, keep the application flow mostly like it is but
improve what is broken."

Owner decisions still open (the rows carry the assumption made): L3 the gate, the display
name (ADR-002), the repository licence (ADR-005).

## A. Shell (window, tray, identity)

| # | Feature | 2.x | Decision | Status / class |
|---|---|---|---|---|
| S1 | One window: rail with Search, Playlist, Browser, Downloads; Settings and Account at the bottom (DESIGN.md) | toolbar + sliding pages | KEEP | done (`ui::MainWindow`, `ui::SideRail`, `ui::Actions`) |
| S2 | Persist window geometry and state; restore last page (setting) | geometry only | KEEP | done (`MainWindow`, `window/lastPage`) |
| S3 | Single instance; second launch forwards a URL and commands | RunGuard, no forwarding | KEEP | done (`app::SingleInstance`) |
| S4 | System tray: show/hide, downloads, quit; close-to-tray optional (default quit) | none | KEEP | done (`ui::TrayController`, close action setting) |
| S5 | Full screen for the browser (F11 and page requests), hint overlay, rail hidden | yes (player) | KEEP | done (`BrowserPage`, `MainWindow::setBrowserFullScreen`, `web::FullScreenHint`) |
| S6 | Native notifications on finish with Open and Show in folder (portal, then freedesktop) | none | KEEP | done (`core::NotificationService`, `platform::createNotifier`, wired in `DownloadsController`) |
| S7 | Crash handler, log file, diagnostics copy, Report a bug sheet | Debug Info in About | KEEP | done (`platform::installCrashHandler`, `core::LogSink`, `ui::AboutDialog`, `ui::BugReportDialog`) |
| S8 | GPU auto-fallback, Wayland to XCB retry | none | KEEP | done (`main.cpp`, `core::graphics_fallback`, `platform::GpuStderrWatch`) |
| S9 | What's new once per version from the bundled changelog; Online guide | none | KEEP | done (`ui::WhatsNewDialog`, `links::kGuide`) |
| S10 | CLI: `playlist-dl <url>`, `--download <url>`, `--settings`, `--profile`, `--quit` | none | KEEP | done (`app::CliOptions`) |
| S11 | Shortcuts sheet (Ctrl+/) listing every action | none | KEEP | done (`ui::ShortcutsDialog`) |
| S12 | Rate this app nag, Claim offer, Donate button | yes | DROP | the store and the account sheet cover them |
| S13 | Toast in the bottom left for results the user is not looking at | none | KEEP | done (`ui::ToastHost` on the window) |

## B. Search (the home page)

| # | Feature | 2.x | Decision | Status / class |
|---|---|---|---|---|
| B1 | Keyword search for playlists through the download engine, results as playlist cards with thumbnail, title, channel, video count | list rows from the ktechpit service, with a preview of videos | KEEP | done (`services::PlaylistSearch` over `services::SearchService`, `ui::SearchPage`, `ui::SearchCardDelegate`; the ktechpit service was dropped by the owner on 2026-09-24) |
| B2 | Engine-based search as the fallback: when the service times out (8 s), errors, or answers with anything but a non-empty array, the same query runs through the engine's playlist search; an "Engine search" chip appears in the header (ADR-003) | none: an outage looked like "no results" | KEEP | done (`services::PlaylistSearch`, `services::SearchService`; tst_playlist_search) |
| B3 | Setting "Results per page" | none | KEEP | done (`Settings::searchResultsPerPage`, Settings, Search) |
| B4 | Paste a playlist or video link in the field: a playlist resolves to the Playlist page, a video opens the Download options sheet for it | "Process Playlist" button | KEEP | done (`SearchPage::linkOf`, `MainWindow::openVideoOptions`) |
| B5 | Search suggestions while typing (https, encoded query, JSON client) | plain http, JSONP breaks silently | KEEP | done (`services::SearchSuggestions`, https JSON client, two hosts with a retry; verified live) |
| B6 | Recent queries as chips (setting, on); Load more | none | KEEP | done (`Settings::recentQueries`, Load more) |
| B7 | Bookmark playlist | menu entry without a handler | LATER | a bookmarks page after 3.0 |
| B8 | Force reload of a cached result | yes | DROP | results are not cached beyond the HTTP cache |
| B9 | Empty, loading and error states with Retry; Esc cancels | Esc cancels; error dialog | KEEP | done (`SearchPage` states, Esc cancels) |
| B10 | Results as a card grid or a list of rows, a toggle in the header, remembered (setting) | grid of rows | KEEP | done (`SearchCardDelegate::Layout`, `Settings::searchGridView`; tst_search_page) |

## C. Playlist page

| # | Feature | 2.x | Decision | Status / class |
|---|---|---|---|---|
| P1 | Playlist read flat through the engine (`-J --flat-playlist`), typed `MediaInfo`; header with thumbnail, title, channel, count, total duration | `python3 core --dump-single-json`, blocking start | KEEP | done (`ui::PlaylistPage`, `services::MediaProbe` flat; verified live on PLBCF2DAC6FFB574DE) |
| P2 | Rows with checkbox, index, thumbnail, title, duration; play on hover; download this one | yes (no per-row download) | KEEP | done (`ui::PlaylistEntryDelegate`, `ui::PlaylistModel`) |
| P3 | Select all, range from/to with the range slider, filter, sort | select all, filter | KEEP | done (select all, From/To with `RangeSlider`, filter, sort; no Newest: flat entries carry no date) |
| P4 | Unavailable (private, deleted) entries shown muted and unchecked, never downloaded | filtered in one place, inverted in two | KEEP | done (`PlaylistModel`: private, deleted and untitled entries unavailable; tst_playlist_page) |
| P5 | Skip videos already in the download folder (setting, on) | none | KEEP | done (page-local toggle, Downloaded badge; `Settings::skipExisting` exists for the sheet) |
| P6 | Play whole playlist, play a video: opens the Browser page on the YouTube page | GitHub Pages player wrapper | KEEP | done (Play all and the row's play glyph open the Browser page) |
| P7 | Play author uploads | sent the display name as a channel id | DROP | the channel link on the Browser page does it |
| P8 | Copy playlist URL | yes | KEEP | done (Copy link) |
| P9 | Playlist cache on disk that never expires | yes | DROP | the flat read is one request; the HTTP cache covers thumbnails |
| P10 | Selection footer with count and size estimate | "N items selected" | KEEP | done ("N of M selected"; no size estimate, O6 is LATER) |

## D. Download options (sheet)

| # | Feature | 2.x | Decision | Status / class |
|---|---|---|---|---|
| O1 | Kind cards Video / Audio only; last kind remembered | radios | KEEP | done (`ui::DownloadOptionsSheet`, `KindCard`s, `Settings::lastDownloadKind`) |
| O2 | Video: quality (Best, 2160p to 360p), container MP4 / MKV / WebM, subtitles, embed thumbnail, embed metadata and chapters | three 0..100 sliders mapped onto the `-F` list, mkv/mp4 | KEEP (the sliders become the quality list) | done (quality list, container, subtitles with Embed, embeds) |
| O3 | Audio: Best / MP3 / M4A / Opus / FLAC / WAV, bitrate Best / 192 / 128, cover art, metadata, "Artist - Song" naming | opus m4a wav mp3 aac flac vorbis, 0..10 quality | KEEP (aac and vorbis map to M4A and Opus) | done (format, bitrate, cover art, metadata; Artist - Song naming LATER) |
| O4 | Folder: `<download folder>/<playlist title>` with Change; number files in playlist order; sanitised names, `--restrict-filenames` off but `%(playlist_index)s` on | raw title in the path | KEEP | done (`core::sanitiseFolderName`, `DownloadOptions::playlistSubfolder` and `numberPlaylistItems`) |
| O5 | Every choice becomes the default for next time (Settings, Downloads) | none | KEEP | done (accept writes the defaults back; the Settings page shows them) |
| O6 | Estimated size for the selection when the probe knows it | none | LATER | needs per-entry probes |

## E. Downloads (engine and queue)

| # | Feature | 2.x | Decision | Status / class |
|---|---|---|---|---|
| E1 | Self-provisioning engine: standalone binary per CPU, checksum, daily update, bundled JS runtime, ffmpeg from the system with an install hint | `python3 core` from GitHub, dead update check, no ffmpeg | KEEP | done (`services::EngineManager`, `ui::EngineSetupDialog`; verified live: engine 2026.08.19 provisioned headless) |
| E2 | Typed protocol with the engine (progress template and print lines), never text parsing | regex on `[download]` lines | KEEP | done (`core::ytdlp_output`, `core::DownloadRunner`) |
| E3 | Queue with concurrency (1 to 5), pause, resume, cancel, retry, remove, open, show in folder, clear finished; persisted across restarts; stale state cleaned on start | start/stop per playlist, stale "running" | KEEP | done (`ui::DownloadsController`, `core::DownloadQueue`, `ui::DownloadsPage`; stale states become Paused on load) |
| E4 | A playlist is one job with entries; per-entry progress and the aggregate on the card; failed entries retried alone | one process per item, counters in QSettings | KEEP | done (one job per playlist, `DownloadJob::detailLine` "3 of 12") |
| E5 | Cookies from the app's own YouTube session handed to the engine (setting, on) | none | KEEP | done (`web::CookieExporter::writeTempFile` through the controller's cookies provider) |
| E6 | Notifications on finish (S6); taskbar progress; keep the screen awake while downloading | none | KEEP | done (notifications with Open / Show in folder / Retry, `platform::TaskbarProgress`, `platform::ScreenInhibitor`) |
| E7 | Import 2.x download records (`download_records/*.json`) as finished or queued jobs | n/a | LATER | the record shape is documented in the analysis |
| E8 | Speed limit | none | KEEP | done (`Settings::speedLimitKbps` applied at admission) |
| E9 | Engine chip with version and update actions on the Downloads page and in Settings | Settings status line | KEEP | done (`DownloadsPage` engine chip; the Settings card lands with G1) |

## F. Browser (the player)

| # | Feature | 2.x | Decision | Status / class |
|---|---|---|---|---|
| W1 | UMD's browser page: tabs on one persistent named profile, toolbar, address bar, find in page, badges, pop-ups as windows, script dialogs as sheets, permission prompts | one view, default (off-the-record in Qt 6) profile | KEEP | done (`web::*`, `ui::BrowserPage`, `ui::BrowserTabButton`; tst_browser_page) |
| W2 | Ad blocking in three layers (interceptor host list, InnerTube response hooks, cosmetic CSS) with the "Ads blocked" badge; trackers blocked | 589-line substring list rebuilt per request, skip clicker, core.css | KEEP | done (`core::BlockList`, `web::RequestInterceptor`, `adblock.js`; badge on the page) |
| W3 | Download this: a playlist page opens the Playlist page, a video page opens the Download options sheet; page-detected media button | none | KEEP | done (`BrowserPage` reads "Open playlist" on a playlist page; a video probes and opens the sheet; page-media button) |
| W4 | Sign-in works (sanitised Chrome UA, Firefox identity on Google sign-in hosts) and is shared with the engine | Firefox 72 UA everywhere | KEEP | done (`web::user_agent`, `web::CookieExporter`; the engine hand-off lands with E5) |
| W5 | Theme follows the app (page background, PREF cookie for YouTube's scheme) | dark cookie on first run | KEEP | done (page background follows the scheme; YouTube's own dark mode follows the user's YouTube setting) |
| W6 | Desktop or mobile site switch | yes | DROP | the Browser identity presets in Settings cover it |
| W7 | Keep the player running when leaving the page; session restore; a new tab opens empty by default, or on YouTube or a custom address (setting) | keepPlayer, history restore, always YouTube | KEEP as "Restore tabs" and "Start page" | done (`Settings::browserSession`, `browserStartPage`; tst_settings_dialog) |
| W8 | Blocked request log window, comment blocking, theatre mode forced | yes | DROP | the badge count replaces the log; YouTube remembers theatre mode |
| W9 | Age-restricted fallback page (`YtTest` wrapper, plain http) | yes | DROP | sign-in and the engine's cookies cover age gates |

## G. Settings surface

Six pages (DESIGN.md section 3): General, Appearance, Downloads, Browser, Search, Advanced.
Target 30 options at most. Restart required: interface scale, hardware acceleration.

| # | Feature | 2.x | Decision | Status / class |
|---|---|---|---|---|
| G1 | Sidebar and stacked pages writing straight to settings; Reset settings | one dialog with group boxes | KEEP | done (`ui::SettingsDialog`, `ui::settings_form`; tst_settings_dialog) |
| G2 | 2.x download folder and account id read once on first start | n/a | KEEP | done (`Application` reads the 2.x folder once; `LicenseService` the id) |
| G3 | Cache size and Delete cache that agree with each other | measured one cache, cleared another | KEEP | done (Advanced page: Clear cache clears the profile's HTTP cache) |

## H. Accounts and licensing (never in user-facing release text)

| # | Feature | 2.x | Decision | Status / class |
|---|---|---|---|---|
| L1 | Shared AccountAndLicense module, app code PLDL, 10-day evaluation | own module, 30 days, http | KEEP | done (`services::LicenseService`, app code PLDL, 10 days) |
| L2 | 2.x account id migrated on first start from `org.keshavnrj.ubuntu/Playlist DL.conf` (`accountId`) or `~/Downloads/.Playlist DL.id` | n/a | KEEP | done (`core::legacyAccountId`, tst_legacy_account; the 2.x download folder too, ADR-002) |
| L3 | Gate: assumed Red's model, a daily allowance of free downloads (5 a day), everything visible; the owner may prefer 2.x's quality gate | quality above "Poor" | KEEP (assumption) | done as assumed (`DownloadsController::admit`, `LicenseService::canDownload`, 5 a day; the gate sheet offers View plans; the Downloads page shows "N of 5 downloads left today" on Free) |
| L4 | Plans sheet listing what is free and what Pro adds | none | KEEP | done (`ui::PlansDialog`, `ui::AccountDialog`: Free is 5 downloads a day with every quality, Pro has no daily limit; reworded 2026-09-24) |
