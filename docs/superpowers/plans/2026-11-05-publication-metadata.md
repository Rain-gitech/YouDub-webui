# Chinese publication metadata implementation plan

**Goal:** Save faithfully translated Chinese titles and full descriptions for publishing, including historical tasks.

**Architecture:** Read original yt-dlp metadata, translate with the configured text model, and atomically save a separate publication JSON and TXT. Expose read/generate/download APIs and a task detail card. Cached results avoid repeat charges. Missing descriptions remain empty; source attribution and links remain intact. This is independent of video artifacts.

**Tech stack:** FastAPI, existing OpenAI-compatible client, Next.js/React.

**Confirmed scope:** User selected faithful translation and historical backfill; no promotional rewriting. Always output Chinese even for Chinese-to-English video tasks.

- [x] Test metadata translation, no-description behavior, full source text, caching, failure, and API download/auth.
- [x] Implement publication adapter, translation-stage hook and APIs; preserve original metadata.
- [x] Add task detail display, copy/download and retry generation with localized labels.
- [x] Backfill downloaded historical records independently without video processing.
- [x] Run backend/frontend checks and review; activate without interrupting active video work.

Validation: backend 340 passed / 34 skipped; frontend 12 passed; TypeScript check passed. Read-only review passed after adding cross-process file locking and a two-process deduplication test. Browser verified full Chinese copy for Vna0hkq7U1g. All 8 completed tasks have publication artifacts; continuous backfill covers the active legacy queue.

Deployment: temporary API-only service on 8001 serves the two publication routes while the legacy GPU worker continues on 8000. Normal startup explicitly routes publication to updated main backend on 8000. See local-gpu-runtime.md for temporary service logs and normal startup behavior.
