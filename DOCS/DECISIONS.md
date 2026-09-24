# Architecture Decision Records

Short, numbered, append-only. Format: Context, Decision, Consequences. Headings use a colon,
not a dash. ADRs inherited from Red 10 (the rewrite kit) are listed once in ADR-000.

---

## ADR-000: inherited from Red 10 and UMD 7 (2026-09-24)

The rewrite adopts, verbatim in spirit and mostly in code, these Red decisions (numbers refer
to `rewrite-kit/reference/red-docs/DECISIONS.md`) and the UMD 7 refinements of them:

- Qt 6.11 minimum built against the KDE snap SDK, run against the `kf6-core24` runtime; C++20
  and CMake only; warnings as errors in CI; layered static libraries `core -> services /
  platform / web -> ui -> app`; no `.ui` files; PMF connects only; `QT_NO_KEYWORDS`.
- One typed `core::Settings` facade; every key once in `settings_keys.h`.
- Injected JavaScript as resources on the profile-level script collection, concatenated into
  one DocumentCreation bundle behind `bootstrap.js` and `hooks.js`, one `QWebChannel` bridge
  per tab (R-ADR-006, UMD's tabbed form).
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
- What's new once per version from the bundled changelog (R-ADR-012).
- Every dialog is the app's own sheet, including the ones raised by pop-up windows.
- UMD's interaction and accessibility standard for every control (DESIGN.md section 1).

## ADR-001: product shape, one window with four pages (2026-09-24)

**Context.** Playlist-Dl 2.x is a single window whose sliding stacked widget holds the
search, the playlist, the player (a Qt WebEngine view of YouTube with ad-skipping scripts),
the download widget, settings and the account screens, all reached from a toolbar. The
owner's brief for 3.0: rebuild it with a browser shell like Ultimate Media Downloader, add
the engine's search as a fallback when the ktechpit search service fails, an own theme
built from the new icon, and keep the flow users know while fixing what is broken.

**Decision.** One window: a rail and a stacked page area with **Search**, **Playlist**,
**Browser** and **Downloads** (DESIGN.md section 2). The Browser page is UMD's browser
shell (tabs on one persistent profile, toolbar, badges, the page-detected Download button)
and replaces the 2.x WebEnginePlayer as the place where videos are played and YouTube is
browsed. The Playlist page is the app's core: the videos of one playlist, selectable, with a
Download options sheet. Downloads run through the kit's yt-dlp queue. Settings is a dialog.
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

**Decision.** The kit's token sheet (`ui::Tokens` and the `{{token}}` style sheet, applied by
`ThemeApplier` on Fusion) with the palette in DESIGN.md section 1: the icon's purple as the
accent, its yellow as the download badge colour, a purple-tinted neutral scale
for both schemes. Theme setting System, Light, Dark; System follows the platform live. The
page background of the browser follows the scheme. `tst_style_contrast` guards the pairs.

**Consequences.** One place to change a colour; both schemes are first-class; the old
per-widget sheets and the theme reload are gone.

## ADR-005: rewrite in this repository on `v3-rewrite`, sources from the kit and UMD (2026-09-24)

**Context.** The playbook starts a new private repository per rewrite. The owner asked for
the rewrite in this project after the Qt 6 port, without a word on the repository.

**Decision.** Branch `v3-rewrite` in this repository; `main` and `qt6-cmake-migration` stay
as the frozen 2.x reference. The tree is assembled from the rewrite kit (`scripts/adopt.sh`
for core, services, platform, app, the yt-dlp download queue, the process documents) and
from Ultimate Media Downloader 7 for the browser shell (`web/`, `BrowserPage`,
`BrowserTabButton`), the page-based window pieces (`Page`, `SideRail`, `Actions`), the
generic sheets and widgets, and `SearchService` (the engine-based search). The download
queue is the kit's yt-dlp-only one, not UMD's aria2 composite.

**Consequences.** The repository holds code that Red and UMD ship as proprietary while its
`LICENSE` is GPL-3 from 2.x and the repository is public on GitHub. Nothing is pushed until
the owner decides between a private repository with a metadata-only public one (the kit's
model) and keeping the GPL, in which case the copied code has to be relicensed by its
owner. Recorded as the first open question in PROGRESS.md.

## ADR-003: search through the ktechpit service, the engine as the fallback (2026-09-24)

**Context.** 2.x searched playlists through one PHP proxy on the author's shared host
(`https://ktechpit.com/USS/Olivia/youtube/api.php?query=<term>`, an Invidious-shaped JSON
array of playlists with a video preview), with no timeout, no retry and no fallback, and an
outage rendered as "No result found". The owner asked for the engine's search as a fallback.
UMD 7's `services::SearchService` already searches playlists through the engine (the site's
filtered search page read flat, `countPlaylist` for the sizes).

**Decision.** `services::PlaylistSearch` runs a query in two stages: (1) the ktechpit service
over https with an 8 s timeout, parsed into the same `SearchResult` shape as the engine's;
(2) when the request fails, times out, or the body is not a non-empty JSON array, the same
query goes to `SearchService` (engine) and the page shows the "Search (engine)" chip with a
tooltip. Results from either source are one list; "Load more" continues with the source
that answered. Setting "Search service: Automatic (default) / Engine only". The engine is
provisioned lazily when the fallback is first needed. Nothing user-facing names either
backend; the chip says "engine". Suggestions use the https JSON client of the suggestion
endpoint with an encoded query, silently off when it fails.

**Consequences.** The first screen keeps working when the service is down, at the cost of a
first-time engine setup on that path. The service stays first because it answers with the
video preview the cards show; the engine's playlist search carries less per item.
