from __future__ import annotations

import sys
from types import SimpleNamespace
from unittest.mock import MagicMock
import pytest
import tqdm

from backend.app.adapters import whisper_asr


def test_load_model_removes_corrupt_cache_and_retries(monkeypatch, tmp_path):
    calls = {"count": 0}
    model = object()
    cache_file = tmp_path / "tiny.pt"
    cache_file.write_bytes(b"bad")

    def load_model(name, device, download_root=None):
        calls["count"] += 1
        assert name == "tiny"
        assert device == "cpu"
        assert download_root == str(tmp_path)
        if calls["count"] == 1:
            raise RuntimeError("SHA256 checksum does not match")
        return model

    fake_whisper = SimpleNamespace(_MODELS={"tiny": "https://example.com/tiny.pt"}, load_model=load_model)
    monkeypatch.setitem(sys.modules, "whisper", fake_whisper)
    monkeypatch.setenv("WHISPER_MODEL", "tiny")
    monkeypatch.setenv("WHISPER_DOWNLOAD_ROOT", str(tmp_path))
    monkeypatch.setattr(whisper_asr, "_MODEL", None)
    monkeypatch.setattr(whisper_asr, "resolve_device", lambda component: SimpleNamespace(selected="cpu"))

    assert whisper_asr._load_model() is model
    assert calls["count"] == 2
    assert not cache_file.exists()


def test_cuda_model_loads_checkpoint_on_cpu_before_moving_weights(monkeypatch):
    cpu_model = MagicMock()
    gpu_model = object()
    cpu_model.to.return_value = gpu_model
    load_model = MagicMock(return_value=cpu_model)
    monkeypatch.setitem(sys.modules, "whisper", SimpleNamespace(load_model=load_model))
    monkeypatch.setattr(whisper_asr, "_MODEL", None)
    monkeypatch.setattr(whisper_asr, "resolve_device", lambda component: SimpleNamespace(selected="cuda:0"))
    monkeypatch.setenv("WHISPER_MODEL", "large-v3-turbo")
    monkeypatch.delenv("WHISPER_DOWNLOAD_ROOT", raising=False)

    assert whisper_asr._load_model() is gpu_model
    load_model.assert_called_once_with("large-v3-turbo", device="cpu", download_root=None)
    cpu_model.to.assert_called_once_with("cuda:0")


@pytest.mark.parametrize("fails", [False, True])
def test_frame_progress_preserves_original_transcribe_globals(fails):
    callback = MagicMock()
    original_tqdm = tqdm.tqdm

    class Model:
        def transcribe(self, audio, *, language, word_timestamps, verbose):
            assert language == "en" and word_timestamps is True and verbose is False
            with tqdm.tqdm(total=100, disable=False) as bar:
                bar.update(50)
                if fails:
                    raise RuntimeError("recognition failed")
                bar.update(50)
            return {"text": "hello", "segments": []}

    if fails:
        with pytest.raises(RuntimeError, match="recognition failed"):
            whisper_asr._transcribe_with_progress(Model(), "audio.wav", "en", callback)
    else:
        result = whisper_asr._transcribe_with_progress(Model(), "audio.wav", "en", callback)
        assert result["text"] == "hello"
        assert callback.call_args.args[0] == 99
    assert callback.call_args_list[0].args[0] == 50
    assert "s audio" in callback.call_args_list[0].args[1]
    assert tqdm.tqdm is original_tqdm
    assert Model.transcribe.__globals__["tqdm"] is tqdm
