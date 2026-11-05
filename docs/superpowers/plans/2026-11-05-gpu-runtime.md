# Local NVIDIA runtime recovery

## Goal and design

Restore local GPU inference on the user's GTX 1660 Ti 6GB without replacing
the old environment, credentials, task database, or unrelated working changes.
The existing installation is CPU-only; the historical task log confirms CPU
Demucs and Whisper. The CUDA driver API reports 11.4 and one available device.

Use Python 3.12 in `.venv-gpu`, with matched CUDA 11.8 torch/torchaudio packages.
Verify actual CUDA kernels before selecting model settings. Keep downloads and
runtime paths inside the project where practical. Provide one Windows launcher.
Avoid torch.compile on the older driver and Windows; expose a reversible
VoxCPM optimization setting. Use explicit CUDA so failure cannot silently become
CPU inference. Preserve model quality defaults until measured evidence justifies
a smaller model or precision change.

## Steps

- [x] Inspect interpreters, dependencies, source, configuration, and historical log.
- [x] Create isolated `.venv-gpu`; preserve `.venv` and existing user changes.
- [x] Install matched CUDA packages; verify device identity and real GPU computation.
- [x] Resolve project dependencies without replacing CUDA torch with CPU torch.
- [x] Add regression coverage for VoxCPM device/optimization configuration before
      implementing the adapter change.
- [x] Configure local runtime/model paths and test individual speech stages on
      existing media in a separate diagnostic output directory.
- [x] Add launcher and environment recovery notes with exact validated versions.
- [x] Verify backend health, frontend startup, relevant regression tests, and
      record benchmark results and any remaining limitations.

## Verification constraints

Never expose API keys or auth hashes in logs. Do not change system GPU drivers.
Do not overwrite historical outputs. Avoid concurrent application workers during
model benchmarks. Distinguish CUDA availability from successful model inference.

## Execution evidence and decisions

- CUDA 11.8 torch/torchaudio 2.7.1 successfully execute CUDA kernels on the actual
  GTX 1660 Ti 6GB. The original runtime was CPU-only.
- Reused the existing compatible local diffq cp312 Windows wheel after a fresh
  source build failed without configured MSVC; retained it in the project cache.
- Ruling: use Transformers 5.15.1, matching the original environment, because the
  attempted 4.x constraint conflicts with current Gradio/Hugging Face dependencies.
- Actual Whisper loading initially exhausted 6GB by retaining a CUDA checkpoint
  during model transfer. CPU checkpoint loading fixes that peak without moving
  inference to CPU; regression test verifies the load/transfer sequence.
- Ruling: use official VoxCPM1.5 for this computer after VoxCPM2 actually failed
  loading at 4.65GiB allocated. Preserve VoxCPM2 and its config. Add automatic
  architecture-specific cloning arguments and matching fallback source transcripts.
- Construct VoxCPM at checkpoint precision with dtype restoration even on failure.
  Set local max_length=2048 for sentence-level work; retain the original JSON.
- Real benchmarks: Demucs 44.7s; Whisper load13.7s+inference47.9s; VoxCPM1.5
  load33.0s+three-clip stage63.2s, output15.68s. Peak allocated memory respectively
  0.54,3.54,2.61GiB; model cleanup leaves approximately0.008GiB allocated.
- Independent review findings fixed: owned Windows process-tree shutdown,
  preserving original stage exceptions on cleanup failure, enforcing CUDA for all
  components in the GPU launcher. Follow-up review found no further major bugs.
- Final checks: 313 backend tests passed,34 conditional skips; pip check clean;
  backend and frontend HTTP200; actual Ctrl+C shutdown releases ports8000/3000.
- Download/translation external services were not rerun; original assets and
  credentials retained. Application restarted with the final model settings.
