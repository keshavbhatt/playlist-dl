# Progress

## Milestones

| Milestone | Status |
|---|---|
| M0 Analysis and contract | done |
| M1 Skeleton | done |
| M2 Engine and search | done |
| M3 Downloads | done |
| M4 Player and browser shell | done (the browser page; W rows) |
| M5 Desktop integration | done (tray, notifications, taskbar, screen inhibit, crash handler, GPU fallback) |
| M6 Polish and text | done (see 2026-09-24, M6) |
| M7 Packaging and release | wip (snap recipe, CI workflow, metainfo, public README, guide and screenshots ready; the snap is unbuilt, the public repository and the Flathub manifest wait for the licence decision) |

## Open questions for the owner

1. **Repository licence**: decided 2026-09-25 (ADR-008): open source at
   github.com/keshavbhatt/playlist-dl, GPL-3.0-or-later with the licensing module under
   the Ktechpit Licensing Module License.
2. **The gate** (FEATURES L3): a daily allowance (5 free downloads a day); 2.x
   gated quality above "Poor" instead.
3. **Display name**: decided 2026-09-25: "Playlist Downloader" stays everywhere; About shows
   "Playlist Downloader (playlist-dl)" so the package name is visible; the snap keeps its name.

## Sessions (newest first)

### 2026-09-25, review rounds

- Owner asked for a full flow and UI/UX review, then "do how you proposed". Three rounds:
  1. One paste outcome (a single item opens the options sheet everywhere); Ctrl+W only
     closes a tab, the shortcuts sheet on F1 and Ctrl+/; failed cards show their reason and
     the header gains Retry failed; the first search or playlist read sets the engine up
     in place (`ensureEngine(then, quiet)`, `SearchPage::State::SettingUp`, the sheet only
     on failure) and the Downloads chip invites instead of warning; right-click menus on
     the download cards and the playlist rows.
  2. The items sheet gets Download the missing items (a fresh job limited to the missing
     positions) and an Arrange menu; the Playlist footer adds the selection's duration; the
     Search empty state says where other sites come in and the results carry a count; the
     Downloads header shows one of Pause all or Resume all and the combined speed; the
     options sheet disables Embed without a subtitle language and says the choices become
     the defaults; Account, Plans and About copy; a Close button on Settings.
  3. Items wording on the Downloads page and the gate; the window title follows the
     playlist; toasts take an action (Added to queue offers View); the rail badge and the
     range slider announce themselves; links by drag and drop; a clipboard link offered
     once as a toast when the window comes to the front.
- Engine provisioning test run for real (`PLDL_NETWORK_TESTS=1`): the first run failed at the
  engine download with "Remote host signaled shutdown" (the release host closed the
  connection); the second and a clean third run passed end to end in 27 s and 99 s: yt-dlp
  and the JS runtime fetched and verified, the missing converter reported with the pacman
  hint, ready once the host ffmpeg is visible, and a real probe returned "Me at the zoo".
  `EngineManager::downloadFile` now retries a transient failure once after 1.5 s. The
  engine's requests carried an old user agent; they now say Playlist-Downloader/3.
  Temporary cookie files, notification ids and the test profile dropped an old prefix.
- Owner: the rail expands to show labels beside the glyphs (toggle under the logo, Ctrl+B),
  animated over 200 ms with the labels fading in over the last two thirds of the way, the
  glyphs staying put; the choice is a setting (`general/railExpanded`); `rail` hook;
  tst_side_rail.
- Owner's list of seven (2026-09-25): Search shows rows by default; playlist cards carry a
  Details button and a double click opens the items sheet (a single item's file otherwise);
  a divider above the Settings footer; an ellipsis on every in-progress label; the launcher's
  Download from a link asks for one with the clipboard's link offered; a Supported sites
  sheet (the engine's list, cached per version) from Help and the Search empty state;
  the changelog opens without "rebuilt from the ground up" and without a Removed section.
  The guide carries nine annotated screenshots (`scripts/guide-screenshots.py`: headless
  grabs with a JSON sidecar of the named widgets' rectangles, numbered callouts drawn on the
  real controls, legends in GUIDE.md).
- About carries the GPL's Appropriate Legal Notices (v3, 5d): copyright, GPL v3 or later,
  no warranty, the module's own terms, and one Licences link to LICENSING.md (owner: "one link").
- Owner: shortcuts are invisible unless one knows F1. The rail's last button is a Help menu
  now (Keyboard shortcuts with its key shown, Online guide, Report a bug, About);
  `PLDL_DEBUG_OPEN=help` pops it for a grab.
- Owner: no way back to the clean Search view after a search. `SearchPage::resetToEmpty`:
  the field's clear button, Esc on results and an empty search all clear the query and the
  results and show the invitation again (tst_search_page).
- Owner: the fixed example chips looked odd. They now come from the suggestion service:
  three seeds a day (rotating through twelve), one completion each, deduplicated, the fixed
  three until an answer arrives or when Suggest as I type is off (tst_search_page).
- Owner: a playlist read failed with "The download engine exited with code 255" and nothing
  else (three probes, each dead within 12 ms; the same binary and playlist work from a shell
  and from the scratch profile, so the cause is still open). `core::friendlyError` now puts
  the engine's last useful stderr line on screen when there is no ERROR: line (a traceback's
  last line, the binary loader's message), names the temporary folder when that is full, and
  the probe logs the stderr tail and exit status, so the next occurrence explains itself.
  Leftover wording replaced.
- Owner: "it still labels videos instead of items": items everywhere now (Playlist page counts
  and Download button, the options sheet's playlist button, search cards, the row action),
  no site or kind exception; guide, README and metainfo say item count. Found on the way:
  Esc in the address bar while a page loaded stopped the page instead of restoring the
  address (the stop shortcut fired first); fixed, and the Escape test serves its page locally.
- Round 4 ("fix the remaining batch"): an empty browser tab shows a themed invitation
  (`web::startPageHtml`, served at about:blank so the address stays empty and the tab
  stays untouched); the Playlist toolbar moves filter, sort and skip to a second row below
  1000 px; Appearance folded into General as the Look card (`settings:appearance` still
  lands there); every sheet names its controls from their labels and the two list models
  carry accessible text per item. Left: the three owner questions.

### 2026-09-24, M6

- Store SEO pass (owner: "figure out our niche and make our meta compete"): niche is
  "playlist downloader" (name-level on both words, nobody else has them in a name).
  Keywords youtube, download, soundcloud (the name already covers playlist and downloader);
  summary with YouTube, SoundCloud and MP3 adjacent; phrase-rich description; categories
  AudioVideo, Audio, Video, Network, Utility; desktop Keywords= widened. Research and the
  query list in ~/DCode/FlathubSEO (docs/playlist-dl-keyword-research.md).
- Store copy and docs reworded for playlists from any site (owner: "update the readme and
  docs"): public README, metainfo, snap description, repo README, changelog, guide intro.
  The snapcraft.io listing is hand-maintained: paste the new summary and description there.
- Owner direction: playlists from any site (ADR-006). Download this on a SoundCloud set said
  "not a YouTube link"; now any web link is probed and the answer decides (Playlist page or
  options sheet), from the Search field, the browser and the CLI. Untitled flat entries are
  items, not unavailable; the page says items off YouTube. Verified live: the SoundCloud set
  reads as "Sia", 50 items; a track opens the options sheet with Audio only.
- Owner report: the Signed in badge (a YouTube session) showed on SoundCloud's sign-in page;
  the owner had it removed, and stated the browser is tied to no site. The cookie mirror now
  keeps every site's cookies, so a sign-in anywhere reaches the engine; the YouTube-only
  cookie filter and the signed-in notion are gone; settings and guide text no longer name
  YouTube for the browser (Use my sign-ins).
- Owner report: signing in to SoundCloud in the built-in browser sent every link off the
  sign-in page to the system browser. `core::shouldOpenExternally` now keeps all http and
  https in the app (the general-browser rule); only mailto, tel and magnet leave, file: never.
- Ported from an earlier app (owner request): the page-load progress is a hairline inside the
  address field (`ui::AddressField`), no bar in the layout, so the page never moves; the
  tab's loader glyph turns while the page loads. 
- Owner request: Settings, Search, Suggest as I type (on by default) gates the suggestions;
  the Playlist items sheet shows a banner when the folder already has a playlist file, with
  Play as is and Folder (`playlist-items-demo-file` hook).
- Update ported (owner request): the What's new sheet takes the whole changelog, opens
  on the running version and carries a Version picker once there are two releases; About
  shows a flat What's new link beside the version; `whatsnew:<version>` and
  `PLDL_DEBUG_CHANGELOG=<path>` hooks; `core::changelogReleases`.
- Owner request: Remove on a download that is running or has files asks Keep, Remove from
  list or Delete the files too (`core::job_files`, the controller's one sheet; the page's
  own confirm is gone).
- Owner request: Shuffle on the playlist items sheet puts the items in a random order (never
  the one shown); Original restores the playlist's order; unticked items stay unticked.

- No `QMessageBox` anywhere; every question is a sheet. The About, Plans and Account text is
  this app's; the engine's ffmpeg hint no longer names an earlier app.
- The engine setup sheet the app opens on its own closes itself once the engine is ready;
  one the user opened stays.
- `PLDL_DEBUG_AUTODOWNLOAD=video|audio` (ADR-000's hook family): with `PLDL_DEBUG_OPEN=
  playlist:<url>` it presses Download once the playlist is in, picks the kind, accepts the
  options sheet, prints the queue's outcome and quits with it. Verified live: the 13-entry
  public playlist PLBCF2DAC6FFB574DE, 10 available entries downloaded as audio in playlist
  order, every entry mapped to its file, the `.m3u8` written next to them, exit 0. Found and
  fixed on the way: a finished job is now persisted at once (the debounce lost the last file
  to a quit), and `/tmp` being full on the host showed up as "Disk quota exceeded".
- Store text: metainfo summary within the badge limit, three single-word keywords first
  (`playlist`, `youtube`, `downloader`), the description carrying the query words, branding
  colours from the icon, six screenshots; `packaging/public/README.md` written; the guide
  and changelog current.
- Screenshots: `scripts/screenshots.py` recoloured to the brand and composed from live grabs
  (search, playlist, options, downloads, browser, items) into `screenshots/` and
  `screenshots/store/`, plus the banner.
- Light theme checked on the playlist page, the options and items sheets, About and Plans.
- Open: a few search cards get no count for some queries (the per-playlist count call
  returns nothing for them; to be looked at with the engine's output); FEATURES rows stay
  `done`, not `verified`, until the owner has run the app on a desktop.

### 2026-09-24

- M0: `reference/analysis-playlist-dl-v2.md` (696 lines, 25 defects, 44 settings keys, every
  endpoint), FEATURES with every 2.x feature decided, LESSONS P1 to P16, ADR-000 to ADR-005,
  DESIGN with the brand palette from the new icon (contrast-checked) and every screen,
  ROADMAP, CODING_STANDARDS, CLAUDE.md.
- M1 started: the skeleton assembled from the owner's earlier apps (core, services, platform, app, the yt-dlp
  queue, web layer, BrowserPage, Page, SideRail, Actions, sheets, SearchService),
  identity `pldl` / `com.ktechpit.playlist-dl`, the new icon set under
  `src/resources/icons`.
- M1 done: 32 tests pass, the build is warning-free with `-Werror` against the KDE Qt 6.11
  snap SDK; the window shows the rail, the placeholder pages and the real Browser page with
  YouTube loaded; the brand tokens are applied (`tst_style_contrast`); the About sheet,
  the snap recipe and the CI workflow carry the new identity. The engine manager and the
  probe are owned by the window (`ensureEngine`).
- M2 and M3 started in parallel: the search stack (ktechpit service, engine fallback,
  suggestions, Search page) and the download stack (controller, Downloads page, cards).
- Search merged: the service answered live from this machine with 20 playlists and, on a
  later run, timed out after 8 s with the fallback engaging (the log shows the engine being
  waited for). 36 tests pass. Known polish: the card grid leaves room for a fifth column at
  1280 px; only dark grabs so far.
- Downloads merged: `DownloadsController` (engine, cookies, persistence, notifications,
  taskbar, screen inhibit, the daily-allowance gate), `DownloadsPage` with the card
  delegate and filters. Verified live headless: `PLDL_DEBUG_DOWNLOAD` provisioned the
  engine and downloaded "Me at the zoo" (av1+aac, thumbnail embedded) in 151 s, exit 0.
  39 tests pass. Grabs of the demo queue in dark and light looked at.
- Owner request: the items sheet has a check box per downloaded item and Select all; unticked
  items stay out of Play all and the playlist file ("Play 3 of 5").
- Owner request: a downloaded playlist plays as a whole. `DownloadJob` remembers its
  entries (the queue fills each one's file from the engine's item and file lines), the
  controller writes an `.m3u8` next to the videos when a playlist finishes (setting), and
  Open on a playlist card shows the new items sheet (state per entry, Play, Show in folder,
  Move up and down, sort, Play all through the file, written in the order shown), which also
  brings back 2.x's list of a playlist's downloaded items.
- Owner request: the Downloads page shows the free tier's remaining downloads for today as
  a chip next to the title (hidden on Pro and during the evaluation), refreshed when a
  download is admitted and when the page is shown again.
- Owner request: the Account and Plans sheets now state the real model (the earlier apps' files,
  torrents and 8K presets were still in the text): Free is 5 downloads a day with every
  quality, a playlist counts each picked video, Pro has no daily limit; the evaluation shows
  "Checking" instead of "0 days left" until the server has answered.
- Owner report: the rail's bottom buttons missed clicks. Cause: the empty toast host (a
  transparent child widget at its default size) sat in the bottom-left corner over them and
  took the mouse. It now starts hidden and lives over the page area; tst_smoke checks that
  every rail button is what lies under its centre.
- Owner request: About reachable from the rail's bottom (info glyph under Account); the
  About sheet itself already matched the earlier shape.
- Owner request: a new browser tab opens empty by default; Settings, Browser offers Empty
  tab, YouTube or a custom address (the address row shows for the custom choice only).
  Grabbed: the empty New tab and the settings page.
- Engine search verified live: 20 playlists for "lofi" with thumbnails, the sizes asked
  per playlist (`SearchService::countPlaylist`) filling the pills, Load more visible; the grid
  now keeps 4 px of slack so a row does not wrap one card short beside a scrollbar.
- Owner decision: the ktechpit search service is dropped; the engine is the single search
  source (ADR-003 revised). The service client, the fallback, the source chip and the
  "Search service" setting are removed; Load more now pages every search.
- Owner report: suggestions looked missing. Cause: the suggestion host stalls for over 5 s
  on some connections (curl reproduced it); a stall was a silent failure. Now a second host
  is tried at once on any failure or timeout (4 s each), with a test; the popup grabbed live.
- Owner request: the Search page switches between the card grid and a list of rows (two
  header buttons, `search/gridView`); grabbed in both modes.
- Playlist page and Download options sheet merged: the playlist read flat through the
  engine, selection tools, unavailable entries, the sheet for a playlist selection and for one
  video, the browser's Download this routed by link kind. Verified live headless: a public
  playlist loaded with 13 entries (2 unavailable) in 4 s after a 20 s engine setup. 41 tests
  pass (the engine provisioning test and the browser page test need the network).
- Settings merged: six pages writing straight to settings, the engine card, Sign out and
  clear session through a relaunch. 39 tests pass (`tst_browser_page` needs the network and
  now allows 30 s).
- Verified: grabs of the Search placeholder, the Browser page and the About sheet, looked
  at; `PLDL_DEBUG_OPEN`, `PLDL_DEBUG_GRAB`, `PLDL_DEBUG_WINDOW_SIZE` work offscreen with
  isolated XDG directories.
- Open: the three questions above; then screenshots for the store, the Flathub manifest
  and the public repository (M7), the plans sheet wording (after the gate decision), the
  search grid's spare column at 1280 px, the engine setup sheet staying open once the engine
  is ready, an end-to-end hook that accepts the options sheet headlessly.
