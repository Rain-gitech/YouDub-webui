from __future__ import annotations

import json
import logging
import os
from pathlib import Path
from time import monotonic
from types import FunctionType, SimpleNamespace
from typing import Callable
from urllib.parse import urlparse

from pydub import AudioSegment

from ..devices import resolve_device

_MODEL = None
logger = logging.getLogger(__name__)


def _transcribe_with_progress(model, audio: str, language: str, callback=None):
    options = {"language": language, "word_timestamps": True, "verbose": False}
    method = model.transcribe
    function = getattr(method, "__func__", None)
    source = function.__globals__.get("tqdm") if isinstance(function, FunctionType) else None
    if callback is None or source is None:
        return method(audio, **options)

    class FrameProgress(source.tqdm):
        def __init__(self, *args, **kwargs):
            self._started = monotonic()
            super().__init__(*args, **kwargs)

        def update(self, amount=1):
            result = super().update(amount)
            if not self.total:
                return result
            current = max(0, min(self.n, self.total)) / 100
            total = self.total / 100
            elapsed = max(monotonic() - self._started, 0.001)
            speed = current / elapsed
            message = f"Recognizing speech · {current:.1f} / {total:.1f} s audio · {speed:.2f}x realtime"
            if speed > 0:
                seconds = int(max(0, total - current) / speed)
                minutes, seconds = divmod(seconds, 60)
                message += f" · ETA {minutes:02d}:{seconds:02d}"
            try:
                callback(min(99, int(current / total * 100)), message)
            except Exception:
                logger.warning("Unable to publish recognition progress", exc_info=True)
            return result

    # Whisper exposes a frame counter through tqdm, but no public progress
    # callback. Give just this invocation its own globals mapping: the installed
    # module, model method and shared tqdm package remain untouched.
    namespace = {**function.__globals__, "tqdm": SimpleNamespace(tqdm=FrameProgress)}
    observe = FunctionType(function.__code__, namespace, function.__name__, function.__defaults__, function.__closure__)
    observe.__kwdefaults__ = function.__kwdefaults__
    return observe(model, audio, **options)


def _whisper_cache_file(whisper, name: str, download_root: str | None) -> Path | None:
    if not download_root:
        return None
    model_url = getattr(whisper, "_MODELS", {}).get(name)
    if not model_url:
        return None
    filename = Path(urlparse(model_url).path).name
    if not filename:
        return None
    return Path(download_root).expanduser() / filename


def _is_checksum_error(exc: RuntimeError) -> bool:
    return "sha256 checksum" in str(exc).lower()


def _remove_corrupt_whisper_cache(whisper, name: str, download_root: str | None) -> bool:
    cache_file = _whisper_cache_file(whisper, name, download_root)
    if not cache_file or not cache_file.exists():
        return False
    cache_file.unlink()
    return True


def _load_model():
    global _MODEL
    if _MODEL is not None:
        return _MODEL

    import whisper

    name = os.getenv("WHISPER_MODEL", "large-v3-turbo")
    whisper_device = resolve_device("whisper").selected
    # Whisper otherwise retains the CUDA checkpoint while copying a second set
    # of model weights to CUDA, which can exhaust a 6GB GPU during loading.
    load_device = "cpu" if whisper_device.startswith("cuda") else whisper_device
    download_root = os.getenv("WHISPER_DOWNLOAD_ROOT") or None
    try:
        model = whisper.load_model(name, device=load_device, download_root=download_root)
    except RuntimeError as exc:
        if not _is_checksum_error(exc):
            raise
        if not _remove_corrupt_whisper_cache(whisper, name, download_root):
            raise
        model = whisper.load_model(name, device=load_device, download_root=download_root)

    if load_device != whisper_device:
        model = model.to(whisper_device)
    _MODEL = model

    return _MODEL


def _to_ms(seconds: float) -> int:
    return int(round(float(seconds) * 1000))


def _convert_words(words: list) -> list:
    return [
        {
            "text": w.get("word", ""),
            "start_time": _to_ms(w.get("start", 0.0)),
            "end_time": _to_ms(w.get("end", 0.0)),
        }
        for w in words or []
    ]


def _convert_segments(segments: list) -> list:
    return [
        {
            "text": seg.get("text", "").strip(),
            "start_time": _to_ms(seg.get("start", 0.0)),
            "end_time": _to_ms(seg.get("end", 0.0)),
            "words": _convert_words(seg.get("words", [])),
        }
        for seg in segments
    ]


def recognize_speech(
    vocals_file: Path, session: Path, language: str,
    progress_callback: Callable[[int, str], None] | None = None,
) -> Path:
    metadata_dir = session / "metadata"
    metadata_dir.mkdir(parents=True, exist_ok=True)
    output_file = metadata_dir / "asr.json"
    if output_file.exists():
        return output_file

    if progress_callback:
        progress_callback(0, "Loading speech recognition model")
    model = _load_model()
    if progress_callback:
        progress_callback(0, "Preparing audio for recognition")
    result = _transcribe_with_progress(model, str(vocals_file), language, progress_callback)

    utterances = _convert_segments(result.get("segments", []))
    if not utterances:
        raise RuntimeError("Whisper did not return any segments.")

    duration_ms = len(AudioSegment.from_file(vocals_file))
    payload = {
        "audio_info": {"duration": duration_ms},
        "result": {
            "text": (result.get("text") or "").strip(),
            "utterances": utterances,
        },
    }
    output_file.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    return output_file
