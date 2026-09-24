# Progress

## Milestones

| Milestone | Status |
|---|---|
| M0 Analysis and contract | done |
| M1 Skeleton | wip |
| M2 Engine and search | todo |
| M3 Downloads | todo |
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
- Verified: nothing live yet; the skeleton's build and tests are reported below when done.
- Open: the three questions above.
