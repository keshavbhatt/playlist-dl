# Progress

## Milestones

| Milestone | Status |
|---|---|
| M0 Analysis and contract | done |
| M1 Skeleton | done |
| M2 Engine and search | done (search); the Playlist page follows with M3 |
| M3 Downloads | wip (queue and page done; Playlist page and options sheet next) |
| M4 Player and browser shell | todo |
| M5 Desktop integration | todo |
| M6 Polish and text | todo |
| M7 Packaging and release | todo |

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
- Settings merged: six pages writing straight to settings, the engine card, Sign out and
  clear session through a relaunch. 39 tests pass (`tst_browser_page` needs the network and
  now allows 30 s).
- Verified: grabs of the Search placeholder, the Browser page and the About sheet, looked
  at; `PLDL_DEBUG_OPEN`, `PLDL_DEBUG_GRAB`, `PLDL_DEBUG_WINDOW_SIZE` work offscreen with
  isolated XDG directories.
- Open: the three questions above.
