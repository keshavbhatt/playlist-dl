# DESIGN: Playlist Downloader 3.0

Written before any screen is coded. Principle: **the playlist is the unit of work**. Every
path through the app (a search, a pasted link, a page in the built-in browser) ends on a
playlist page whose videos you pick and download, or on a video you play. The chrome is the
app's own: a slim rail, pages, sheets. Nothing here is inherited from the 2.x layout except
the flow (CLAUDE.md standing rule): search or paste, browse the playlist, play, choose the
download options, watch the queue.

## 1. Brand

- Name: **Playlist Downloader** (the 2.x name Playlist-Dl stays as the snap name
  `playlist-dl` and in the store listing until the owner renames it there).
- Mark: the icon set delivered on 2026-09-24 under `new-icon/` (now
  `src/resources/icons/hicolor` and `src/resources/icons/pldl.svg`): a purple playlist card
  with a white track list, two paler cards stacked behind it, and a yellow download badge.
  128 px grid, GNOME style, no baked shadow. The symbolic variant is the list plus the
  download arrow in one colour.
- Tagline (store, About): *Save whole playlists offline.*
- Tokens: the kit's `{{token}}` sheet with a palette taken from the icon. The accent is the
  card's purple; the yellow of the badge is a second accent reserved for the download badge,
  progress and the "new" chips, so the two colours of the icon are the two colours of the
  app. `ui::Tokens` is the source; `tst_style_contrast` asserts every pair below at 4.5:1
  for text and 3:1 for glyphs in both schemes.

| Token | Dark | Light | Used for |
|---|---|---|---|
| `accent` | `#C061CB` | `#813D9C` | active rail item, focus ring, chip borders, section headings |
| `accentStrong` | `#9141AC` | `#813D9C` | primary button fills (white text at 5.9:1 and 6.8:1) |
| `accentHover` | `#813D9C` | `#613583` | hover on primary |
| `accentSoft` | `#2B1D33` | `#F3E8F6` | active rail and tab background, selected chips, banners |
| `accentText` | `#FFFFFF` | `#FFFFFF` | text on accentStrong |
| `badge` | `#F6D32D` | `#E5A50A` | the download badge pill on the rail and "new" chips (never a bar: on light it cannot reach 3:1 against the track) |
| `badgeText` | `#3D1D4B` | `#3D1D4B` | text on badge |
| `bg` | `#141118` | `#F8F6FA` | window and page background |
| `rail` | `#141118` | `#FFFFFF` | the rail (1 px border on the page side) |
| `panel` | `#1C1822` | `#FFFFFF` | cards, sheets, settings nav |
| `elevated` | `#26212E` | `#FFFFFF` | menus, tooltips, hovered cards, toasts |
| `hover` | `#302A3A` | `#ECE6F0` | hover on neutral controls |
| `input` | `#181420` | `#FFFFFF` | text fields |
| `border` | `#2E2838` | `#E2DCE8` | hairlines, idle progress track |
| `text` | `#F2EEF6` | `#1A1523` | primary text |
| `muted` | `#A79FB3` | `#5E5870` | secondary text, idle rail glyphs |
| `link` | `#DC8ADD` | `#6F3A8C` | links |
| `success` | `#34A853` | `#1B7F38` | finished |
| `warning` | `#F9AB00` | `#B45309` | paused, waiting |
| `danger` | `#FF5A52` | `#C62828` | failed, destructive buttons |

- Type: system UI font, 13 and 14 body, 16 titles, 18 sheet headers, 20 page headers,
  weight 500 for titles. Shape: 8 px controls, 12 px cards, 14 px sheets, pill buttons and
  chips (the icon's rounded cards set the radius language). Motion: none that blocks; the
  page switch is instant, the toast slides 160 ms.
- Interaction states and accessibility: the UMD standard applies unchanged (four visible
  states on every control, an immediate busy verb on press, a toast for results the user is
  not looking at, full keyboard order with a visible focus ring, accessible names, colour
  never the only signal, 34 px hit targets).

## 2. Layout

```
+----+----------------------------------------------------------+
| R  |  Page header (title, search field or toolbar, action)    |
| A  |----------------------------------------------------------|
| I  |                                                          |
| L  |  Page content                                            |
|    |                                                          |
+----+----------------------------------------------------------+
```

Rail (56 px, `rail` surface), top to bottom: brand mark, **Search** (Ctrl+1), **Playlist**
(Ctrl+2, the playlist last opened; disabled until one is), **Browser** (Ctrl+3),
**Downloads** (Ctrl+4, with the active-count badge in `badge`); at the bottom Settings
(Ctrl+,) and Account. The rail hides while a video is full screen. Every action is also in
the shortcuts sheet (Ctrl+/).

The window remembers its geometry and the page it was on. Minimum size 960 by 600.

## 3. Screens

### Search (home)

The 2.x home: a search field with online suggestions, results as playlist cards. Rebuilt:

- Header: the title "Search", a wide field "Search YouTube playlists or paste a link",
  a Search button (accent). Enter searches; a pasted playlist or video link resolves straight
  to the Playlist page (or the Browser page for a video, with a "Download" offer).
- Suggestions drop down under the field as the user types (debounced 250 ms, from the
  suggestion endpoint; Esc closes, arrows move, Enter picks).
- Results: a grid of playlist cards (thumbnail 16:9 with the video-count badge in the
  corner, title on two lines, channel). Click opens the Playlist page. A "Load more" pill
  at the end. Two flat buttons at the header's right end switch between the grid and a
  list of rows (small picture on the left, title on one line, channel under it); the
  choice is remembered (owner request, 2026-09-24).
- Source chip in the header's right corner: hidden normally; when the ktechpit service
  fails the results come from the engine and an "Engine search" chip appears with a
  tooltip explaining the fallback (ADR-003). Never the tool's name.
- Empty state: the brand mark, "Search for a playlist or paste a link", three example
  chips. Error state: the reason and Retry.
- Recent: under the field, up to eight chips with the last queries (Settings: keep search
  history, on).

### Playlist

The 2.x playlist view (videos with checkboxes, select all, download button) rebuilt:

- Header: back arrow to Search, playlist thumbnail 96 by 54, title (16/500), channel,
  "N videos, total duration", a **Download** primary button (accent pill, reads "Download
  12 videos" as the selection changes) and a "Play all" flat button (opens the playlist in
  the Browser page).
- Toolbar row: "Select all" checkbox, range "from [ ] to [ ]" spin boxes with the range
  slider (from UMD), a filter field, a sort menu (playlist order, title, duration, newest),
  "Skip videos already downloaded" toggle (on).
- List: rows of 72 px: checkbox, index, thumbnail 96 by 54, title (2 lines), channel and
  duration, a play glyph on hover (opens the video in the Browser page) and a download glyph
  (downloads just this video). Unavailable videos (private, removed) are muted with a badge
  and unchecked.
- Footer: "12 of 120 selected, about 1.4 GB at 720p" (estimate when known).
- Download opens the **Download options** sheet.

### Download options (sheet, 640 px)

The 2.x playlistdownloadoptions page, as a sheet:

- Kind cards: **Video** "Picture and sound", **Audio only** "Just the sound". Opens on the
  last kind used.
- Video: Quality (Best available, 2160p, 1440p, 1080p, 720p, 480p, 360p), Container (MP4,
  MKV, WebM), Subtitles (None, language list, Embed toggle), Embed thumbnail, Embed
  metadata and chapters.
- Audio: Format (Best, MP3, M4A, Opus, FLAC, WAV), Quality (Best, 192, 128 kbps), Embed
  cover art, Embed metadata, "Artist - Song" naming for music.
- Folder row: "Save to <folder>/<playlist title>" with Change; "Number files in playlist
  order" toggle (on).
- Footer: Cancel, **Download N videos**.
- The sheet remembers every choice as the defaults (Settings, Downloads).

### Browser

UMD's browser page, unchanged in shape: tab strip, toolbar (back, forward, reload,
address, "Ads blocked" badge, "Signed in" badge, **Download this** button), find bar, the
web view, the floating "Download detected" button. Start page `https://www.youtube.com/`.
Additions for this app:

- When the page is a playlist (`list=` in the address, or a `/playlist` page) the
  Download this button reads "Open playlist" and lands on the Playlist page.
- When the page is a video, Download this opens the Download options sheet for that one
  video (kind cards, quality, folder).
- Ad blocking on YouTube through the interceptor, the response hooks and the cosmetic layer
  (the 2.x skipper and core.css are dropped; the layers do the job).
- Sign-in shared with the engine: the session cookies go to the download runs (Settings,
  Downloads: "Use my YouTube sign-in", on).

### Downloads

The 2.x download widget rebuilt as a page:

- Header: "Downloads", engine chip ("Download engine 2026.09.xx", "Setting up", "Update
  available"), Pause all, Resume all, a Clear menu (finished, failed, all), Open folder.
- Cards (88 px): thumbnail 96 by 54 with a state bar, title, "playlist title, 3 of 12" for
  playlist entries or the channel for single videos, quality and format line, progress bar
  (`accent` chunk on `border` track) with "12.4 MB of 118 MB, 3.2 MB/s, 00:32" or
  "Merging", "Finished, 118 MB", "Failed, reason". Hover actions: pause or resume, cancel,
  open, show in folder, retry, remove. Double click opens the file.
- A playlist is one card that expands to its entries (chevron), with the aggregate progress
  on the parent.
- Filters: All, Active, Finished, Failed. Empty state: "Downloads you start will show up
  here".
- Notifications on finish with Open and Show in folder (portal, then freedesktop).

### Settings (dialog 860 by 600, sidebar and stacked pages)

- **General**: Start page (Search, Last page), When closing (Quit, Keep in tray), Keep
  search history, Show What's new after updates, Notifications on finish.
- **Appearance**: Theme (System, Light, Dark), Interface scale (restart).
- **Downloads**: Folder, Subfolder per playlist, Number files in playlist order, Default
  kind, Default video quality and container, Default audio format and quality, Subtitles,
  Embed thumbnail and metadata, Concurrent downloads (1 to 5), Speed limit, Skip already
  downloaded, Use my YouTube sign-in; **Engine card**: version, JS runtime, media converter,
  Check for updates, Update now, Auto-update daily, Use an engine already on this system.
- **Browser**: Start page, Restore tabs, Block ads, Do Not Track, Browser identity (presets
  and custom).
- **Search**: Search service (Automatic, Engine only), Results per page.
- **Advanced**: Hardware acceleration, Clear cache, Sign out and clear session, Open log
  folder, Copy diagnostics, Reset settings.

### Sheets and small screens

- Every question is a `MessageSheet` (never QMessageBox): permission prompts, clear
  session, reset settings, the GPU notice, the crash notice, page alert/confirm/prompt.
- First launch: no wizard. The engine is set up on the first search that needs it or the
  first download, with the progress sheet ("Setting up the download engine").
- About: brand mark, version and revision, the tagline, links (Online guide, More apps,
  Contact, Report a bug), Diagnostics.
- Account: the shared account sheet; Plans lists what is free and what Pro adds (owner to
  decide the gate, FEATURES L3).
- What's new: once per version from the bundled changelog.
- Rate this app and Claim offer (2.x): dropped; the store and the account sheet cover them.

## 4. Rules that survive

- No modal dialog for anything that can be a sheet.
- Progress is never text-only; the rail badge and the taskbar mirror the queue.
- Engine names never appear: "download engine", "media converter", "search".
- Every screen works at 960 by 600 and scales up; the playlist list is the only thing that
  scrolls vertically, nothing scrolls horizontally.
- Dark and light are both first-class; screenshots are taken in dark.
