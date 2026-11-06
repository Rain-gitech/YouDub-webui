import json

from backend.app.v1 import tts


def test_voxcpm15_assets_are_advertised_as_their_actual_model(tmp_path, monkeypatch):
    for name in ('tokenizer_config.json', 'tokenizer.model', 'model.safetensors', 'audiovae.pth'):
        (tmp_path / name).write_bytes(b'asset')
    (tmp_path / 'config.json').write_text(json.dumps({'architecture': 'voxcpm'}))
    monkeypatch.setenv('YOUDUB_VOXCPM_MODEL_DIR', str(tmp_path))
    assert tts.available_models() == ['VoxCPM1.5']


def test_unknown_architecture_is_not_advertised(tmp_path, monkeypatch):
    for name in ('tokenizer_config.json', 'tokenizer.model', 'model.safetensors', 'audiovae.pth'):
        (tmp_path / name).write_bytes(b'asset')
    (tmp_path / 'config.json').write_text(json.dumps({'architecture': 'unknown'}))
    monkeypatch.setenv('YOUDUB_VOXCPM_MODEL_DIR', str(tmp_path))
    assert tts.available_models() == []
