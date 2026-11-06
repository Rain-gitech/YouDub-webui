import subprocess

import pytest

from backend.app.config import ffmpeg_binary
from backend.app.v1.personal_progress import progress_check, write_progress
from backend.app.v1.tts_process import initialization_dtype


def test_child_progress_is_deduplicated_and_never_marks_task_complete(tmp_path):
    path = tmp_path / 'progress.json'
    events = []
    check = progress_check(path, lambda *args: events.append(args), lambda: None)
    check()
    write_progress(path, .5, 'Halfway')
    check(); check()
    assert events == [(.5, 'Halfway')]
    write_progress(path, 1, 'Finished encoding')
    check()
    assert events[-1][0] == .99


def test_real_ffmpeg_progress_and_eta(tmp_path):
    path = tmp_path / 'encode.txt'
    subprocess.run([ffmpeg_binary(), '-hide_banner', '-loglevel', 'error', '-progress', str(path),
                    '-f', 'lavfi', '-i', 'color=c=black:s=160x90:d=2:r=24',
                    '-c:v', 'libx264', str(tmp_path / 'out.mp4')], check=True)
    events = []
    progress_check(path, lambda *args: events.append(args), lambda: None, duration=2)()
    assert events and events[-1][0] > .5 and events[-1][0] < 1
    assert 'realtime' in events[-1][1]


def test_dtype_is_restored_after_constructor_failure(monkeypatch):
    import torch
    before = torch.get_default_dtype()
    monkeypatch.setenv('VOXCPM_INIT_DTYPE', 'bfloat16')
    with pytest.raises(RuntimeError):
        with initialization_dtype():
            assert torch.get_default_dtype() == torch.bfloat16
            raise RuntimeError('load failed')
    assert torch.get_default_dtype() == before


def test_progress_observation_does_not_swallow_cancellation(tmp_path):
    def cancel():
        raise RuntimeError('cancelled')
    with pytest.raises(RuntimeError, match='cancelled'):
        progress_check(tmp_path / 'absent.json', lambda *_: None, cancel)()
