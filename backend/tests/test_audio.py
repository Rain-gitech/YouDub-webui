import numpy as np
import pytest
import soundfile as sf

from backend.app.adapters.audio import _stretch_segment


@pytest.mark.parametrize("ratio", [0.9, 1.1])
def test_real_time_stretch_changes_duration_without_shifting_pitch(tmp_path, ratio):
    sample_rate = 16000
    source = tmp_path / "voice.wav"
    tone = (0.1 * np.sin(2 * np.pi * 440 * np.arange(sample_rate) / sample_rate)).astype(np.float32)
    sf.write(source, tone, sample_rate)
    cache = tmp_path / "stretched"
    cache.mkdir()

    audio, rate = _stretch_segment(source, ratio, ratio, cache)

    assert rate == sample_rate
    assert np.isfinite(audio).all()
    assert len(audio) / rate == pytest.approx(ratio, abs=0.08)
    spectrum = np.abs(np.fft.rfft(audio))
    pitch = np.fft.rfftfreq(len(audio), 1 / rate)[np.argmax(spectrum)]
    assert pitch == pytest.approx(440, abs=10)
