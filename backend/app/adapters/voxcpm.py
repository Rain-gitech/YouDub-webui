from __future__ import annotations

import json
import os
import re
from contextlib import contextmanager
from pathlib import Path
from typing import Callable

import soundfile as sf
from pydub import AudioSegment

from ..config import MODEL_CACHE_DIR
from ..devices import resolve_device

_MODEL = None
_MODEL_ARCHITECTURE = "voxcpm2"


@contextmanager
def _initialization_dtype():
    configured = os.getenv("VOXCPM_INIT_DTYPE", "").strip().lower()
    if not configured:
        yield
        return
    import torch

    if configured not in {"float32", "float16", "bfloat16"}:
        raise ValueError("VOXCPM_INIT_DTYPE must be float32, float16, or bfloat16.")
    previous = torch.get_default_dtype()
    try:
        # The pipeline has one model worker. Construct directly at checkpoint
        # precision instead of allocating a full temporary FP32 model first.
        torch.set_default_dtype(getattr(torch, configured))
        yield
    finally:
        torch.set_default_dtype(previous)

_PROMPT_CACHE_GENERATION_DEFAULTS = {
    "min_len": 2,
    "max_len": 4096,
    "retry_badcase": True,
    "retry_badcase_max_times": 3,
    "retry_badcase_ratio_threshold": 6.0,
}


def _model_path() -> Path:
    configured_dir = os.getenv("VOXCPM_MODEL_DIR")
    if configured_dir:
        return Path(configured_dir).expanduser()

    model_id = os.getenv("VOXCPM_MODEL", "OpenBMB/VoxCPM2")
    local_dir = MODEL_CACHE_DIR / model_id.replace("/", "__")
    from modelscope import snapshot_download

    downloaded = snapshot_download(model_id, local_dir=str(local_dir))
    return Path(downloaded)


def _load_model():
    global _MODEL, _MODEL_ARCHITECTURE
    if _MODEL is None:
        from voxcpm import VoxCPM

        model_path = _model_path()
        config_file = model_path / "config.json"
        architecture = (
            json.loads(config_file.read_text(encoding="utf-8")).get("architecture", "voxcpm")
            if config_file.exists() else "voxcpm2"
        )
        selected_device = resolve_device("voxcpm").selected
        with _initialization_dtype():
            _MODEL = VoxCPM.from_pretrained(
                str(model_path),
                load_denoiser=os.getenv("VOXCPM_LOAD_DENOISER", "false").lower() == "true",
                device=selected_device,
                optimize=os.getenv("VOXCPM_OPTIMIZE", "true").lower() == "true",
            )
        _MODEL_ARCHITECTURE = architecture
    return _MODEL


def _first_reference(files: list[Path], min_ms: int) -> Path | None:
    for path in files:
        if len(AudioSegment.from_file(path)) >= min_ms:
            return path
    if files:
        return files[0]
    return None


def _speaker(item: dict) -> str:
    speaker = item.get("speaker")
    if speaker is None:
        return "1"
    speaker = str(speaker).strip()
    return speaker or "1"


def _fallback_references(vocals_dir: Path, items: list[dict], min_ms: int) -> tuple[dict[str, Path], Path]:
    files = sorted(vocals_dir.glob("*.wav"))
    if not files:
        raise FileNotFoundError("No vocal segments were generated for VoxCPM references.")

    global_fallback = _first_reference(files, min_ms) or files[0]
    speaker_files: dict[str, list[Path]] = {}
    for index, item in enumerate(items, start=1):
        reference = vocals_dir / f"{index:04d}.wav"
        if reference.exists():
            speaker_files.setdefault(_speaker(item), []).append(reference)

    fallbacks: dict[str, Path] = {}
    for speaker, refs in speaker_files.items():
        fallback = _first_reference(refs, min_ms)
        if fallback is not None:
            fallbacks[speaker] = fallback

    return fallbacks, global_fallback


def _tts_text(item: dict) -> str:
    text = item.get("dst") or item.get("zh", "")
    if not isinstance(text, str) or not text.strip():
        raise ValueError("target text must be a non-empty string")
    text = text.replace("\n", " ")
    return re.sub(r"\s+", " ", text)


def _reference_arguments(reference: Path, source_text: str) -> dict[str, str]:
    if _MODEL_ARCHITECTURE == "voxcpm":
        if not isinstance(source_text, str) or not source_text.strip():
            raise ValueError("VoxCPM1.x voice cloning requires the reference source transcript.")
        return {"prompt_wav_path": str(reference), "prompt_text": source_text}
    return {"reference_wav_path": str(reference)}


def generate_tts(
    translation_file: Path,
    vocals_dir: Path,
    session: Path,
    progress_callback: Callable[[int, str], None] | None = None,
) -> Path:
    output_dir = session / "segments" / "tts"
    output_dir.mkdir(parents=True, exist_ok=True)
    data = json.loads(translation_file.read_text(encoding="utf-8"))
    items = data["translation"]
    total = len(items)
    if total == 0:
        if progress_callback:
            progress_callback(100, "No TTS clips to generate")
        return output_dir

    model = _load_model()
    min_reference_ms = int(os.getenv("VOXCPM_MIN_REFERENCE_MS", "1200"))
    fallback_references, global_fallback = _fallback_references(vocals_dir, items, min_reference_ms)
    cfg_value = float(os.getenv("VOXCPM_CFG_VALUE", "2.0"))
    inference_timesteps = int(os.getenv("VOXCPM_INFERENCE_TIMESTEPS", "10"))

    fallback_caches = {}
    source_texts = {
        vocals_dir / f"{index:04d}.wav": item.get("src", "")
        for index, item in enumerate(items, start=1)
    }

    for index, item in enumerate(items, start=1):
        output_file = output_dir / f"{index:04d}.wav"
        if not output_file.exists():
            reference = vocals_dir / f"{index:04d}.wav"
            text = _tts_text(item)
            if not reference.exists() or len(AudioSegment.from_file(reference)) < min_reference_ms:
                speaker = _speaker(item)
                if speaker not in fallback_caches:
                    fallback = fallback_references.get(speaker, global_fallback)
                    fallback_caches[speaker] = model.tts_model.build_prompt_cache(
                        **_reference_arguments(fallback, source_texts.get(fallback, ""))
                    )
                result = model.tts_model.generate_with_prompt_cache(
                    target_text=text,
                    prompt_cache=fallback_caches[speaker],
                    cfg_value=cfg_value,
                    inference_timesteps=inference_timesteps,
                    **_PROMPT_CACHE_GENERATION_DEFAULTS,
                )
                wav_tensor, _, _ = result
                wav = wav_tensor.squeeze(0).cpu().numpy()
            else:
                wav = model.generate(
                    text=text,
                    **_reference_arguments(reference, item.get("src", "")),
                    cfg_value=cfg_value,
                    inference_timesteps=inference_timesteps,
                )
            sf.write(output_file, wav, model.tts_model.sample_rate)
        if progress_callback:
            progress = round(index / total * 100)
            progress_callback(progress, f"Prepared {index}/{total} TTS clips")

    return output_dir
