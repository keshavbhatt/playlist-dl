# Architecture Decision Records

Short, numbered, append-only. Format: Context, Decision, Consequences. Headings use a colon,
not a dash. ADRs inherited from the owner's earlier apps are listed once in ADR-000.

---

## ADR-000: inherited from the owner's earlier apps (2026-09-24)

The rewrite adopts, verbatim in spirit and mostly in code, these decisions from the owner's earlier apps (their R-ADR numbering kept) and the
refinements of them:

- Qt 6.11 minimum built against the KDE snap SDK, run against the `kf6-core24` runtime; C++20
  and CMake only; warnings as errors in CI; layered static libraries `core -> services /
  platform / web -> ui -> app`; no `.ui` files; PMF connects only; `QT_NO_KEYWORDS`.
- One typed `core::Settings` facade; every key once in `settings_keys.h`.
- Injected JavaScript as resources on the profile-level script collection, concatenated into
  one DocumentCreation bundle behind `bootstrap.js` and `hooks.js`, one `QWebChannel` bridge
  per tab (R-ADR-006, the tabbed form).
- User agent derived from the engine default with the QtWebEngine token stripped; the Firefox
  identity only on Google sign-in hosts (R-ADR-008).
- Chromium sandbox on outside the snap; GPU on by default with the storm detector fallback,
  hardware video decoding turned off first; Wayland to XCB retry once.
- Portals first: notifications, file manager, screen inhibit; freedesktop D-Bus as fallback.
- Single instance over `QLocalServer` with JSON commands; `--profile` for isolated profiles.
- Downloads: standalone engine binary provisioned and verified per CPU, JS runtime bundled,
  ffmpeg from the system, JSON in and typed print lines out, never text parsing (R-ADR-004,
  R-ADR-005); the app's own session cookies handed to the engine in a 0600 temp file
  (R-ADR-006), read from Chromium's cookie database because `loadAllCookies()` is a no-op.
- Ad blocking in three cheap layers (host list in the interceptor, InnerTube response hooks,
  cosmetic CSS), curated lists, no filter engine (R-ADR-003).
- Headless verification hooks documented, not hidden; licence overrides only in Debug builds
  (R-ADR-010).
- Accounts through the shared AccountAndLicense module with a per-app config; gate decided by
  the owner; nothing about it in user-facing release text (R-ADR-011).
- What's new once per version from the bundled changelog; a Version picker shows earlier
  releases and About links to it (R-ADR-012, update of 2026-09-24).
- Every dialog is the app's own sheet, including the ones raised by pop-up windows.
- The interaction and accessibility standard for every control (DESIGN.md section 1).

## ADR-001: product shape, one window with four pages (2026-09-24)

**Context.** Playlist-Dl 2.x is a single window whose sliding stacked widget holds the
search, the playlist, the player (a Qt WebEngine view of YouTube with ad-skipping scripts),
the download widget, settings and the account screens, all reached from a toolbar. The
owner's brief for 3.0: rebuild it with a general browser shell, add
the engine's search as a fallback when the ktechpit search service fails, an own theme
built from the new icon, and keep the flow users know while fixing what is broken.

**Decision.** One window: a rail and a stacked page area with **Search**, **Playlist**,
**Browser** and **Downloads** (DESIGN.md section 2). The Browser page is the earlier apps' browser
shell (tabs on one persistent profile, toolbar, badges, the page-detected Download button)
and replaces the 2.x WebEnginePlayer as the place where videos are played and YouTube is
browsed. The Playlist page is the app's core: the videos of one playlist, selectable, with a
Download options sheet. Downloads run through the yt-dlp queue. Settings is a dialog.
The 2.x extras that were not about playlists (Rate this app, Claim offer, the blocked
request log window) are dropped.

**Consequences.** One profile, one download pipeline, one theme. The player gains tabs,
find in page, sign-in and pop-up handling for free. The cost is that the 2.x sliding
animation and its toolbar are gone; the rail and the shortcuts replace them.

## ADR-002: identity `com.ktechpit.playlist-dl`, settings under `ktechpit/playlist-dl` (2026-09-24)

The 2.x app stored settings in `org.keshavnrj.ubuntu/Playlist DL.conf` and shipped as
snap `playlist-dl`. New identity: `applicationName=playlist-dl`,
`organizationName=ktechpit`, desktop and AppStream id `com.ktechpit.playlist-dl`, display
name "Playlist Downloader". The snap keeps its store name `playlist-dl`. Version starts at
**3.0.0**, above the 2.2 in the store. Migrated once on first start, never written back:
the 2.x account id (FEATURES L2) and the 2.x download folder when it is not the default.
Everything else starts fresh.

## ADR-004: the app's own theme, from the icon (2026-09-24)

**Context.** 2.x used Fusion with hand-written per-widget style sheets in several colours
and a light/dark toggle that reloaded the page. The owner asked for an own theme matching
the new icon.

**Decision.** The token sheet (`ui::Tokens` and the `{{token}}` style sheet, applied by
`ThemeApplier` on Fusion) with the palette in DESIGN.md section 1: the icon's purple as the
accent, its yellow as the download badge colour, a purple-tinted neutral scale
for both schemes. Theme setting System, Light, Dark; System follows the platform live. The
page background of the browser follows the scheme. `tst_style_contrast` guards the pairs.

**Consequences.** One place to change a colour; both schemes are first-class; the old
per-widget sheets and the theme reload are gone.

## ADR-005: rewrite in this repository on `v3-rewrite`, sources from the owner's earlier apps (2026-09-24)

**Context.** The owner asked for the rewrite in this project after the Qt 6 port, without a
word on the repository.

**Decision.** Branch `v3-rewrite` in this repository; `main` and `qt6-cmake-migration` stay
as the frozen 2.x reference. The tree is assembled from the owner's earlier apps: core,
services, platform, app, the yt-dlp download queue and the process documents from one, the
browser shell (`web/`, `BrowserPage`, `BrowserTabButton`), the page-based window pieces
(`Page`, `SideRail`, `Actions`), the generic sheets and widgets and `SearchService` (the
engine-based search) from another. The download queue is the yt-dlp-only one.

**Consequences.** The repository holds code the earlier apps ship as proprietary while its
`LICENSE` is GPL-3 from 2.x and the repository is public on GitHub. Nothing is pushed until
the owner decides between a private repository with a metadata-only public one and keeping the GPL, in which case the copied code has to be relicensed by its
owner. Recorded as the first open question in PROGRESS.md.

## ADR-008: open source, two licences by path (2026-09-25)

**Context.** ADR-005 left the repository question open: the tree holds code copied from
the owner's earlier apps, both shipped proprietary, and the owner owns all of it. Playlist-Dl 2.x
had been GPL-3 all along (its `LICENSE`), which the owner had forgotten.

**Decision.** The code goes public at github.com/keshavbhatt/playlist-dl: `main` holds
3.x, `old-qt5` holds the 2.x code that was on the private `main`, and the old snap
launcher artifacts that were the public repository's only content stay on
`packaging-archive`. The program is GPL-3.0-or-later. The licensing module (the account,
licence, evaluation-period and allowance code: `src/modules/AccountAndLicense/`,
`src/core/licensing/`, `src/services/licensing/`, `src/ui/license_gate.*`, its two tests)
is source-available under the Ktechpit Licensing Module License: read and build, never
modify, bypass, redistribute or reuse. A GPLv3 section 7 additional permission lets
official builds combine the two; anyone else's GPL fork must leave the module out.
`LICENSING.md` explains it, `REUSE.toml` maps it, the module files carry SPDX headers,
the metainfo declares `GPL-3.0-or-later AND LicenseRef-proprietary=...`, the snap says
GPL-3.0-or-later. The private repository (p-pldl) keeps its history as a mirror.

**Consequences.** Contributions to the GPL part are welcome; forks must replace the gate.
A build switch that compiles the app without the module would make forking practical
and is a follow-up. The copied code is relicensed by its owner through this decision; the
earlier apps themselves stay proprietary.

## ADR-007: the display name (2026-09-25)

The app is "Playlist Downloader" in the window, the store copy and the docs; the binary,
the snap and the desktop id stay `playlist-dl`. About shows "Playlist Downloader
(playlist-dl)" so the name people type and search for is visible in one place. Closes the
third open question of ADR-005.

## ADR-006: playlists from any site (2026-09-24)

The owner: "playlist-dl is not anymore just for YouTube, it's playlists from anywhere", and
the browser is tied to no site. Consequences:

- Every http(s) link is a candidate (`core::isDownloadable`); the engine says what it is.
  A link on another site, from the Search field, the browser's Download this or the CLI, is
  probed flat: a playlist answer opens the Playlist page, a single item the options sheet
  (`MainWindow::openAnyLink`). YouTube links keep their shortcuts (a playlist opens the
  page without a probe, a video probes in full, a channel queues with the defaults).
- Flat entries from other sites may carry no title (a SoundCloud set lists links alone);
  they are items, not unavailable ones, named "Item N" until the engine names the file. A
  full read of such a set took 90 s, so the flat answer has to do. Only YouTube's markers
  ("[Private video]", "[Deleted video]") mean an item is gone.
- The Playlist page says "items" off YouTube and "videos" on it.
- The web layer carries no site gate (navigation, cookies, badges: ADR-000 revised).
- Search stays YouTube (the engine's playlist search); other sites are reached by link.
  Store copy still leads with YouTube and should be revised at release (owner).

## ADR-003: search through the ktechpit service, the engine as the fallback (2026-09-24)

**Context.** 2.x searched playlists through one PHP proxy on the author's shared host
(the ktechpit search service, an Invidious-shaped JSON
array of playlists with a video preview), with no timeout, no retry and no fallback, and an
outage rendered as "No result found". The owner asked for the engine's search as a fallback.
An earlier app's `services::SearchService` already searches playlists through the engine (the site's
filtered search page read flat, `countPlaylist` for the sizes).

**Decision.** `services::PlaylistSearch` runs a query in two stages: (1) the ktechpit service
over https with an 8 s timeout, parsed into the same `SearchResult` shape as the engine's;
(2) when the request fails, times out, or the body is not a non-empty JSON array, the same
query goes to `SearchService` (engine) and the page shows an "Engine search" chip with a
tooltip. Results from either source are one list; "Load more" continues with the source
that answered. Setting "Search service: Automatic (default) / Engine only". The engine is
provisioned lazily when the fallback is first needed. Nothing user-facing names either
backend; the chip says "Engine search" and is hidden while the service answers. Suggestions use the https JSON client of the suggestion
endpoint with an encoded query, silently off when it fails.

**Consequences.** The first screen keeps working when the service is down, at the cost of a
first-time engine setup on that path. The service stays first because it answers with the
video preview the cards show; the engine's playlist search carries less per item.

**Revised (owner, 2026-09-24): the engine is the single source.** The ktechpit service is
dropped: `services::KtechpitSearch` and the fallback logic are deleted, `PlaylistSearch` is
a thin owner of `SearchService` (engine paths, lazy provisioning, paging), the source chip
and the "Search service" setting are gone. Consequence: the first search on a fresh install
provisions the engine (the setup sheet shows progress) and every search pages with "Load
more". The suggestion endpoint stays as it was.
