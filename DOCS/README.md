# DOCS: how this project is tracked

Everything about the rewrite lives here. If it is not in `DOCS/`, it did not happen.

| File | What it is |
|---|---|
| `FEATURES.md` | Feature list with the decision per row. The scope contract. |
| `ROADMAP.md` | Milestones with exit criteria. |
| `PROGRESS.md` | Session log and milestone status table, newest first. |
| `CODING_STANDARDS.md` | Binding Qt 6 / C++20 rules (from the rewrite kit, adapted). |
| `DECISIONS.md` | Architecture Decision Records, append-only. |
| `LESSONS.md` | Mistakes in the old app and the rule each became. |
| `DESIGN.md` | The UI design: brand, tokens, layout, every screen. |
| `reference/analysis-playlist-dl-v2.md` | Full technical audit of the original app (frozen). |

## Workflow

1. Before coding a feature: its row in `FEATURES.md` must say `KEEP`.
2. While coding: follow `CODING_STANDARDS.md`; new architectural choices get an ADR.
3. After coding: update the row's Status, add a `PROGRESS.md` entry, run the tests.
4. Commit style: conventional commits, plain messages, no trailers.

## Build and run (dev)

```sh
sudo snap install kde-qt6-core24-sdk kf6-core24   # once
scripts/dev-build.sh --tests
scripts/dev-run.sh
QT_LOGGING_RULES="pldl.*.debug=true" scripts/dev-run.sh
```
