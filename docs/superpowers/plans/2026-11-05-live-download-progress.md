# Live download progress

## Design

Use yt-dlp progress and postprocessor hooks to publish byte-weighted download
progress, human-readable byte counts, transfer speed and current-file ETA through
the existing stage progress/last_message fields. Keep progress below 100 until
the final merged output has been verified. Unknown totals remain indeterminate
in the message; never infer overall completion from a finished video stream when
audio remains. Separate formats and retries must not reuse stale accounting.

Reuse the existing two-second serialized frontend polling and stage progress
display. Include running-stage partial progress in the overview; this is stage
completion, not a prediction of total processing time. Allow speed/message updates
even while the integer percentage is unchanged, with two-second DB throttling.

The existing worker already holds old Python code and is downloading user jobs.
Do not restart it. A temporary PID-bound local monitor may publish file-based
progress for the old worker until it exits, using the same reporter. Native hooks
take over after the next normal backend restart. No task statuses, queue order,
credentials or original media are changed by the monitor.

## Implementation and verification

- [x] Add failing tests for multi-stream bytes, unknown totals, merging and hooks.
- [x] Implement reporter and connect adapter/pipeline; verify throttle updates.
- [x] Test and implement partial overview progress plus live telemetry rendering.
- [x] Run backend/frontend regression checks and TypeScript validation.
- [x] Review and enable temporary progress for currently running downloads.
- [x] Verify persisted live metrics and actual page response without backend restart.

## Constraints

Preserve unrelated working changes and the GPU environment recovery changes.
Never restart/cancel active jobs for deployment. Do not label per-file ETA as an
estimate for the entire task. Do not fabricate speed or percent for ASR stages
that currently have no incremental measurements.

## Verification evidence

- Independent review found that installed yt-dlp removes requested_formats from
  individual stream hooks. Fixed via a before_dl PostProcessor that seeds selected
  formats; tested using the actual installed yt-dlp pre_process API without network.
- Seed-processor events do not publish99; finished streams correct estimated sizes
  to actual bytes; unknown totals/speed/ETA remain explicitly unknown or omitted.
- 322 backend tests passed,34 conditional skips;10 frontend tests passed;TypeScript
  noEmit and targeted git diff whitespace checks passed.
- Existing backend PID24380 was never restarted. Temporary PID-bound monitor
  updates only running/download rows, checks task state atomically, and stops when
  this backend exits or after12h. The native hooks activate at normal next restart.
- Actual authenticated browser page verified download progress, transferred bytes,
  speed and current-file ETA; refreshed client shows overview partial progress.
- At14:04 local time current task was91%,512.4/560.3MiB including video+audio;
  second task remained queued. No downloads or task statuses were reset.
