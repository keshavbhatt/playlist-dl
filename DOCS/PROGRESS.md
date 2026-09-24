# Progress

## Milestones

| Milestone | Status |
|---|---|
| M0 Analysis and contract | done |
| M1 Skeleton | done |
| M2 Engine and search | done |
| M3 Downloads | done |
| M4 Player and browser shell | done (UMD's browser page; W rows) |
| M5 Desktop integration | done (tray, notifications, taskbar, screen inhibit, crash handler, GPU fallback) |
| M6 Polish and text | wip (sheets, shortcuts, About, What's new, guide, changelog done; screenshots, metainfo polish, plans wording open) |
| M7 Packaging and release | wip (snap recipe and CI written, unbuilt; Flathub manifest and public repo open) |

## Open questions for the owner

1. **Repository licence** (ADR-005): the tree copies Red 10 and UMD 7 code, shipped
   proprietary, into a public GPL-3 repository. Options: a private repository with a
   metadata-only public one (the kit's model, recommended), or relicensing the copied code.
   Nothing is pushed until this is answered.
2. **The gate** (FEATURES L3): assumed Red's daily allowance (5 free downloads a day); 2.x
   gated quality above "Poor" instead.
3. **Display name**: "Playlist Downloader" (the icon set's wording) instead of "Playlist-Dl";
   the snap keeps its name.

## Sessions (newest first)

### 2026-09-24

- M0: `reference/analysis-playlist-dl-v2.md` (696 lines, 25 defects, 44 settings keys, every
  endpoint), FEATURES with every 2.x feature decided, LESSONS P1 to P16, ADR-000 to ADR-005,
  DESIGN with the brand palette from the new icon (contrast-checked) and every screen,
  ROADMAP, CODING_STANDARDS, CLAUDE.md.
- M1 started: the skeleton assembled from the kit (core, services, platform, app, the yt-dlp
  queue) and UMD 7 (web layer, BrowserPage, Page, SideRail, Actions, sheets, SearchService),
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
- Owner request: About reachable from the rail's bottom (info glyph under Account); the
  About sheet itself already matched Red's shape.
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
