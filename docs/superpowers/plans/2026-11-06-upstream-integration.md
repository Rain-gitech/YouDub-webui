# Personal upstream integration

**Goal:** Integrate upstream 6929643 (v0.0.12), run the Harness application independently, and preserve classic history and personal GPU/publication behavior.

**User decision:** New app separately, old historical entry retained, personal functionality migrated. No in-place migration of data/youdub.sqlite.

**Architecture:** Upstream is authoritative for the new runtime. Classic commit 543f3fb and its original checkout remain available for all 22 completed tasks and one paused task. Keep the existing data, cookies, model assets and credentials intact. New task storage is separate. Expose the classic history entry and add Chinese publishing copy through the new application's task interface. Add VoxCPM 1.5 compatibility using installed assets for the GTX1660Ti 6GB, retaining upstream VoxCPM2 behavior.

- [x] Resolve structural merge conflicts using upstream architecture; retain classic implementation in its original checkout/branch.
- [x] Install isolated frontend dependencies and establish upstream test baseline using the existing Python environment without changing it.
- [x] Add tests and compatibility for VoxCPM1.5 and scoped low-memory initialization; preserve progress reporting.
- [x] Add independent Chinese publication copy, copy/download UI and classic history entry.
- [x] Configure separate runtime storage, reuse installed model assets, and validate startup and representative local processing.
- [x] Run Python/Node/frontend checks and a whole-branch review; commit and push integration and personal branches.

Ruling: Remove classic HTTP app and Next routes from the new tree rather than retaining incompatible imports beside upstream's replacement. Their complete source is preserved by the merge parent and classic branch. Never point the new store at the classic database.

Validation evidence and the real end-to-end task are recorded in `docs/personal-upstream.md`. Integration is committed on the new worktree; the classic checkout remains on its original commit via `legacy/personal-20261105`.
