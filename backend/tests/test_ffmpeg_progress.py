import io
import shutil
import subprocess

import pytest

from backend.app.adapters import ffmpeg


def test_ffmpeg_progress_reports_encoded_time_speed_and_eta():
    events = []
    reporter = ffmpeg.EncodingProgress(120, lambda *event: events.append(event))
    for line in ("out_time_us=30000000", "fps=48.5", "speed=2.0x", "progress=continue"):
        reporter.line(line)
    assert events[-1][0] == 25
    assert "48.5 fps" in events[-1][1]
    assert "2.00x" in events[-1][1]
    assert "00:45" in events[-1][1]
    reporter.line("out_time_us=120000000")
    reporter.line("progress=end")
    assert events[-1][0] == 99


def test_unknown_duration_and_speed_do_not_fabricate_eta():
    events = []
    reporter = ffmpeg.EncodingProgress(None, lambda *event: events.append(event))
    for line in ("out_time_us=30000000", "fps=N/A", "speed=N/A", "progress=continue"):
        reporter.line(line)
    assert events[-1][0] == 0
    assert "30.0" in events[-1][1]
    assert "ETA" not in events[-1][1]


def test_encoder_failure_is_not_marked_complete(monkeypatch, tmp_path):
    class Process:
        stdout = io.StringIO("out_time_us=1000000\nprogress=continue\n")
        def __enter__(self):
            return self
        def __exit__(self, *args):
            return False
        def wait(self):
            return 1

    monkeypatch.setattr(ffmpeg.subprocess, "Popen", lambda *args, **kwargs: Process())
    events = []
    with pytest.raises(subprocess.CalledProcessError):
        ffmpeg.run_encoding_with_progress(["ffmpeg", "output.mp4"], 2, lambda *args: events.append(args), tmp_path)
    assert all(percent < 100 for percent, _ in events)


def test_real_encoder_emits_progress(tmp_path):
    binary = ffmpeg.ffmpeg_binary()
    if not shutil.which(binary):
        pytest.skip("FFmpeg is not installed")
    target = tmp_path / "encoded.mp4"
    events = []
    ffmpeg.run_encoding_with_progress([
        binary, "-y", "-f", "lavfi", "-i", "color=c=black:s=160x90:d=2:r=24",
        "-c:v", "libx264", "-pix_fmt", "yuv420p", str(target),
    ], 2, lambda *args: events.append(args), tmp_path)
    assert target.stat().st_size > 0
    assert events and max(percent for percent, _ in events) > 50
    assert all(percent < 100 for percent, _ in events)
