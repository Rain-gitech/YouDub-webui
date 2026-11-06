"""Progress files connect cancellable model subprocesses to graph progress."""
from __future__ import annotations

import json
import math
import os
from pathlib import Path
from time import monotonic
from types import FunctionType, SimpleNamespace


def write_progress(path: Path | None, value: float, message: str) -> None:
    if path is None:
        return
    temporary = path.with_suffix('.tmp')
    try:
        temporary.write_text(json.dumps({'value': min(.99, max(0, value)), 'message': message}), encoding='utf-8')
        os.replace(temporary, path)
    except OSError:
        pass  # Failure to report progress must not abort successful inference.


def progress_check(path: Path, callback, check_cancel, *, duration: float | None = None):
    previous = ''
    def check():
        nonlocal previous
        check_cancel()
        try:
            content = path.read_text(encoding='utf-8')
            if content == previous:
                return
            if duration is not None:
                packet = dict(line.split('=', 1) for line in content.splitlines() if '=' in line)
                seconds = max(0, float(packet.get('out_time_us', 0)) / 1_000_000)
                speed = float(packet.get('speed', '0').strip().removesuffix('x'))
                if not math.isfinite(seconds) or not math.isfinite(speed):
                    return
                message = f'Encoding video · {seconds:.1f} / {duration:.1f} s'
                if speed > 0:
                    eta = math.ceil(max(0, duration - seconds) / speed)
                    message += f' · {speed:.2f}x realtime · ETA {eta // 60:02d}:{eta % 60:02d}'
                callback(min(.99, seconds / duration), message)
            else:
                data = json.loads(content)
                callback(min(.99, max(0, float(data['value']))), str(data['message']))
            previous = content
        except (OSError, ValueError, KeyError):
            pass
    return check


def transcribe_with_progress(model, audio: str, progress_path: Path | None, **options):
    method = model.transcribe
    function = getattr(method, '__func__', None)
    source = function.__globals__.get('tqdm') if isinstance(function, FunctionType) else None
    if progress_path is None or source is None:
        return method(audio, **options)

    class FrameProgress(source.tqdm):
        def __init__(self, *args, **kwargs):
            self.started = monotonic()
            super().__init__(*args, **kwargs)
        def update(self, amount=1):
            result = super().update(amount)
            if self.total:
                seconds, total = max(0, self.n) / 100, self.total / 100
                speed = seconds / max(.001, monotonic() - self.started)
                message = f'Recognizing speech · {seconds:.1f} / {total:.1f} s · {speed:.2f}x realtime'
                if speed > 0:
                    eta = math.ceil(max(0, total - seconds) / speed)
                    message += f' · ETA {eta // 60:02d}:{eta % 60:02d}'
                write_progress(progress_path, seconds / total, message)
            return result

    namespace = {**function.__globals__, 'tqdm': SimpleNamespace(tqdm=FrameProgress)}
    observe = FunctionType(function.__code__, namespace, function.__name__, function.__defaults__, function.__closure__)
    observe.__kwdefaults__ = function.__kwdefaults__
    return observe(model, audio, **options)
