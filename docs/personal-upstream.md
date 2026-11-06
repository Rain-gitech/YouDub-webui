# Personal edition and upstream updates

This edition integrates upstream `6929643` (v0.0.12). The Harness workspace and classic Next/FastAPI workspace are separate applications with separate task databases.

## Entrypoints

- New workspace: `http://127.0.0.1:3086/`. Open the access URL printed by the launcher on first browser use; the token is not a public URL.
- Classic history: `http://127.0.0.1:3000/`. The original checkout and `data/youdub.sqlite` remain unchanged. The new sidebar and publication panel link to it.
- Run `start-youdub.cmd` in the **new** checkout, or `.venv-personal\Scripts\python.exe scripts/start-personal.py`.
- On first launch, pass `--legacy-root <absolute classic checkout> --prepare-only`. Local paths are saved under ignored `data/personal/paths.json`; the generated Harness profile stores credential references, not API keys. Later UI model edits are preserved.
- The classic launcher belongs to the preserved classic checkout. In the new checkout `scripts/start-local-gpu.ps1` forwards to the new personal launcher; the Qt client still uses the classic HTTP API.

## Runtime isolation

New settings, database and task files live below `data/personal/`. Never point them at the classic `data` or `workfolder`. The launcher reads classic translation settings using a read-only SQLite connection and provides the DeepSeek key only through the child environment. It copies media tools into the new runtime and reuses explicit existing model paths. It does not resume, migrate or rewrite classic tasks.

The local `.venv-personal` is an overlay environment. Its `youdub-legacy-runtime.pth` references the existing classic GPU environment read-only; packages needed only by the new runtime are installed into the overlay. Recreate this `.pth` with your actual classic site-packages path if moving the project. Do not delete the referenced classic environment.

On a fresh machine, create a dedicated Python environment, install the required dependency groups with a GPU-compatible Torch build, and configure its interpreter in the profile. `reuseRuntime` is an explicit existing-runtime option: base Python and media tools are checked before opening the workspace; individual model capabilities and workflow resource checks still reject missing or incompatible providers. Normal upstream first-run installation remains the default.

## Personal functionality

- Model asset probing distinguishes VoxCPM1.5 from VoxCPM2. A 1.5 installation is not upgraded to 2 merely to pass the resource preparation screen. Existing default templates choose an installed model if the original default is unavailable.
- The launcher scopes `VOXCPM_INIT_DTYPE=bfloat16` to isolated model construction and uses CPU-first Whisper checkpoint loading to avoid duplicate CUDA allocations on a GTX1660Ti 6GB. Models run in cancellable subprocesses and are released on exit.
- Downloads, ASR, TTS and video export report progress, rate and estimated remaining time; 100% is reserved for validated successful stage completion.
- Downloaded source metadata includes full descriptions and attribution. Chinese title and description are generated independently after successful tasks, cached as `publication.zh.json` / `publication.zh.txt` in the new task directory, and available from task details for copying/downloading. Missing descriptions stay empty. Translation failure can be retried without media reprocessing. The background model request is cancelled when its Host scope closes.
- Classic-era GPU startup, progress and publication implementations remain available in commit `543f3fb` and the classic branch, including all historical artifacts on disk.

## Future updates

`origin` is the personal Fork; `upstream` is the original project. Keep `main` aligned to upstream. Develop on `personal`, and merge future upstream revisions in an isolated update branch. Validate Python, SDK/workflow, frontend and Harness tests plus the actual local runtime before updating `personal`. Do not overwrite the classic checkout when advancing the personal branch.

## Validation on this PC

Python: 411 passed / 14 skipped. SDK/workflows: 139 passed. Frontend: 242 passed. Harness: 84 passed (run with `--test-concurrency=1` to avoid concurrent GPU cold-probe contention on Windows). Type checks, source framework build, product build and composition checks pass.

With the user's explicit approval, task `529434f8-85b3-4dad-a345-c8c155c19529` completed all eight real workflow stages on a 12-second public clip: media preparation, Demucs separation, large-v3-turbo recognition, DeepSeek translation, reference preparation, GTX1660Ti VoxCPM1.5 synthesis, mixing and export. Final MP4 is 12.012 seconds with video and audio streams; WAV, source SRT and translated SRT were produced. Independent publication generation completed and the TXT download returned HTTP 200 with an attachment header. Only text was sent to DeepSeek; audio/video stayed local. Local-upload metadata has no source description and correctly displays an empty description.

Optional Qwen alignment is not provisioned or validated on this PC; its normal resource preparation remains available.
