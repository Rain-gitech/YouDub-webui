from types import SimpleNamespace
from unittest.mock import MagicMock, call
import sys

import pytest

from backend.app.adapters import voxcpm
from backend.app import pipeline


def test_voxcpm_loader_honors_explicit_device_and_disables_compile(monkeypatch, tmp_path):
    factory = MagicMock()
    monkeypatch.setitem(__import__("sys").modules, "voxcpm", SimpleNamespace(VoxCPM=factory))
    monkeypatch.setattr(voxcpm, "_MODEL", None)
    monkeypatch.setattr(voxcpm, "_model_path", lambda: tmp_path)
    monkeypatch.setenv("VOXCPM_DEVICE", "cuda:0")
    monkeypatch.setenv("VOXCPM_OPTIMIZE", "false")
    monkeypatch.setenv("VOXCPM_LOAD_DENOISER", "false")
    monkeypatch.setenv("VOXCPM_INIT_DTYPE", "")

    model = voxcpm._load_model()

    factory.from_pretrained.assert_called_once_with(
        str(tmp_path), load_denoiser=False, device="cuda:0", optimize=False,
    )
    assert model is factory.from_pretrained.return_value
    assert voxcpm._load_model() is model
    assert factory.from_pretrained.call_count == 1


def test_voxcpm_loader_inherits_device_and_preserves_optimization_default(monkeypatch, tmp_path):
    factory = MagicMock()
    monkeypatch.setitem(__import__("sys").modules, "voxcpm", SimpleNamespace(VoxCPM=factory))
    monkeypatch.setattr(voxcpm, "_MODEL", None)
    monkeypatch.setattr(voxcpm, "_model_path", lambda: tmp_path)
    monkeypatch.setenv("VOXCPM_DEVICE", "")
    monkeypatch.delenv("VOXCPM_OPTIMIZE", raising=False)
    monkeypatch.setenv("DEVICE", "cuda")
    monkeypatch.setenv("VOXCPM_LOAD_DENOISER", "false")
    monkeypatch.setenv("VOXCPM_INIT_DTYPE", "")

    voxcpm._load_model()

    factory.from_pretrained.assert_called_once_with(
        str(tmp_path), load_denoiser=False, device="cuda", optimize=True,
    )


@pytest.mark.parametrize("fails", [False, True])
@pytest.mark.parametrize("cleanup_fails", [False, True])
@pytest.mark.parametrize("stage,module_name", [
    ("asr", "backend.app.adapters.whisper_asr"),
    ("tts", "backend.app.adapters.voxcpm"),
])
def test_stage_releases_cached_model_even_after_failure(monkeypatch, fails, cleanup_fails, stage, module_name):
    model_module = SimpleNamespace(_MODEL=object())
    cuda = SimpleNamespace(
        is_available=lambda: True,
        empty_cache=MagicMock(side_effect=RuntimeError("cleanup failed") if cleanup_fails else None),
    )
    monkeypatch.setitem(sys.modules, module_name, model_module)
    monkeypatch.setitem(sys.modules, "torch", SimpleNamespace(cuda=cuda))
    monkeypatch.setenv("RELEASE_MODELS_AFTER_STAGE", "true")
    for name in ("update_task", "update_stage", "get_task"):
        monkeypatch.setattr(pipeline.database, name, MagicMock())
    runner = pipeline.PipelineRunner("test-gpu-memory")
    monkeypatch.setattr(runner, "log", MagicMock())
    monkeypatch.setattr(runner, "stage_message", MagicMock())

    def run_stage(task):
        if fails:
            raise RuntimeError("inference failed")

    runner._stage_handlers[stage] = run_stage
    if fails:
        with pytest.raises(RuntimeError, match="inference failed"):
            runner._run_stage(stage)
    else:
        runner._run_stage(stage)

    assert model_module._MODEL is None
    cuda.empty_cache.assert_called_once()


def test_model_cache_is_preserved_when_release_is_disabled(monkeypatch):
    model = object()
    module = SimpleNamespace(_MODEL=model)
    monkeypatch.setitem(sys.modules, "backend.app.adapters.whisper_asr", module)
    monkeypatch.setenv("RELEASE_MODELS_AFTER_STAGE", "false")
    pipeline._release_stage_model("asr")
    assert module._MODEL is model


@pytest.mark.parametrize("fails", [False, True])
def test_voxcpm_low_memory_initialization_restores_torch_dtype(monkeypatch, tmp_path, fails):
    original_dtype, init_dtype = object(), object()
    set_dtype = MagicMock()
    fake_torch = SimpleNamespace(
        bfloat16=init_dtype, float16=object(), float32=original_dtype,
        get_default_dtype=lambda: original_dtype, set_default_dtype=set_dtype,
    )
    factory = MagicMock()
    if fails:
        factory.from_pretrained.side_effect = RuntimeError("model load failed")
    monkeypatch.setitem(sys.modules, "torch", fake_torch)
    monkeypatch.setitem(sys.modules, "voxcpm", SimpleNamespace(VoxCPM=factory))
    monkeypatch.setattr(voxcpm, "_MODEL", None)
    monkeypatch.setattr(voxcpm, "_model_path", lambda: tmp_path)
    monkeypatch.setenv("VOXCPM_INIT_DTYPE", "bfloat16")
    if fails:
        with pytest.raises(RuntimeError, match="model load failed"):
            voxcpm._load_model()
        assert voxcpm._MODEL is None
    else:
        voxcpm._load_model()
    assert set_dtype.call_args_list == [call(init_dtype), call(original_dtype)]
