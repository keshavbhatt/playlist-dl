# Injected scripts

JavaScript injected by the built-in browser (FEATURES B1, B2, B4). Rules:

* One file per concern, registered in `../CMakeLists.txt` via `qt_add_resources`, installed by
  `web::ScriptBundle` on the **profile-level** script collection.
* The bootstrap bundle runs at `DocumentCreation`: `qwebchannel.js`, then `bootstrap.js` with
  the config object prepended by C++ (it creates `window.__red`: `config`, `report()`, `log()`,
  `bridge`, `onBridge()`, `onConfig()`, `style()`), then `hooks.js` (`__red.hooks.onJson /
  onRequest / onInitial / prune`), then the feature scripts. The bootstrap only arms itself on
  YouTube hosts; Google's sign-in pages inspect their environment and get nothing.
* `page-media.js` is installed separately at `DocumentReady` on every site (it needs the DOM).
* Every file starts with a header block (name / purpose / depends / verified / on-fail), is
  wrapped in an IIFE with `try/catch`, and reports failures through
  `window.__red.report(name, error)` → `web::Bridge::scriptFailed` → the `pldl.web.js` log.
* Only the *stable tier* is allowed (CODING_STANDARDS §7): standard APIs, our own CSS,
  InnerTube field names, YouTube's public player API, semantic element tags.

| File | Where | Feature |
|---|---|---|
| `bootstrap.js` | YouTube | infrastructure |
| `hooks.js` | YouTube | JSON / fetch / XHR / initial-data modifier registry |
| `adblock.js` | YouTube | B2: the response and cosmetic layers (the network layer is `web::RequestInterceptor`) |
| `page-media.js` | every site | B4: reports the media the page plays for the "Download detected" button |

Bridge API (`web::Bridge`, one per tab): JS→C++ `scriptFailed`, `log`, `mediaState`, `retry`,
`downloadRequested`, `pageMedia`; C++→JS `mediaCommand`, `configChanged`.

Diagnostics: `PLDL_DISABLE_WEB_SCRIPTS=all` (or a comma list of names, e.g. `adblock,page-media`)
drops scripts.
