# Roadmap: Playlist Downloader 3.0

Milestones with exit criteria. The order is the playbook's. Packaging is deliberately last;
a polished app first. The owner is not in the loop session by session, so every milestone
records its open questions in PROGRESS and proceeds under stated assumptions.

| Milestone | Exit criteria |
|---|---|
| M0 Analysis and contract | `reference/analysis-playlist-dl-v2.md`; FEATURES with every 2.x feature decided; LESSONS; ADR-000 to ADR-005; DESIGN with the brand tokens from the new icon and every screen described |
| M1 Skeleton | Layered libraries `core -> services / platform / web -> ui -> app` build with `-Werror`; settings facade with tests; single instance and CLI; the main window shows the shell (rail, pages) in the brand theme; `dev-run.sh` runs against the snap runtime; the 2.x account id is migrated |
| M2 Engine and search | Engine provisioned and verified per CPU; the search page finds playlists through the ktechpit service and falls back to the engine's search; a pasted playlist link resolves; the playlist page lists the videos with thumbnails |
| M3 Downloads | Download options sheet (video and audio presets, quality, container, per-video selection); the queue runs playlist entries with typed progress, pause, resume, retry, notifications with Show in folder; the queue survives a restart |
| M4 Player and browser shell | The built-in player and YouTube browser run on the shared web layer: one profile, script bundle, bridge, pop-ups, permissions, ad blocking, full screen; the page-detected Download button; sign-in shared with the engine |
| M5 Desktop integration | Tray, screen inhibit, taskbar progress, notifications through the portal, crash handler, GPU fallback |
| M6 Polish and text | Sheets everywhere, shortcuts sheet, About with diagnostics, Report a bug, What's new, Online guide, settings pages; CHANGELOG, README, GUIDE, metainfo for search, screenshots. Done 2026-09-24 |
| M7 Packaging and release | Snap built in CI on edge; store listing; Flathub manifest lint-clean and submitted by the owner; public repo refreshed; release checklist done |
