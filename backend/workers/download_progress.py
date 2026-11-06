"""Aggregate yt-dlp's independently downloaded audio/video streams."""
from __future__ import annotations

import math

from yt_dlp.postprocessor import PostProcessor


def number(value):
    return max(0, float(value)) if isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value) else None


def size(value):
    for unit in ('B', 'KiB', 'MiB', 'GiB', 'TiB'):
        if value < 1024 or unit == 'TiB':
            return f'{value:.1f} {unit}'
        value /= 1024


class DownloadProgress:
    def __init__(self, callback):
        self.callback, self.streams = callback, {}

    def seed_formats(self, info):
        for item in info.get('requested_formats') or [info]:
            self.streams.setdefault(str(item.get('format_id') or 'media'), {
                'total': number(item.get('filesize')) or number(item.get('filesize_approx')), 'downloaded': 0,
            })

    def __call__(self, event):
        status = event.get('status')
        if status not in {'downloading', 'finished'}:
            return
        info = event.get('info_dict') or {}
        key = str(info.get('format_id') or event.get('filename') or 'media')
        stream = self.streams.setdefault(key, {'total': None, 'downloaded': 0})
        downloaded = number(event.get('downloaded_bytes'))
        total = number(event.get('total_bytes')) or number(event.get('total_bytes_estimate'))
        if total:
            stream['total'] = total
        if status == 'finished':
            downloaded = stream['total'] if downloaded is None else downloaded
            stream['total'] = downloaded or stream['total']
        stream['downloaded'] = downloaded or 0
        done = sum(item['downloaded'] for item in self.streams.values())
        known = all(item['total'] for item in self.streams.values())
        total = sum(item['total'] or 0 for item in self.streams.values())
        value = min(.99, done / total) if known and total else None
        message = f'Downloading · {size(done)}' + (f' / {size(total)}' if known else ' · total unknown')
        speed, eta = number(event.get('speed')), number(event.get('eta'))
        if status == 'downloading' and speed is not None:
            message += f' · {size(speed)}/s'
        if status == 'downloading' and eta is not None:
            seconds = int(eta)
            message += f' · Current file ETA {seconds // 60:02d}:{seconds % 60:02d}'
        if status == 'finished':
            message += ' · Preparing next stream or merge'
        self.callback(value, message)

    def postprocess(self, event):
        if event.get('postprocessor') != 'DownloadProgressSeed' and event.get('status') in {'started', 'processing', 'finished'}:
            self.callback(.99, 'Merging and checking downloaded files')


class DownloadProgressSeedPP(PostProcessor):
    def __init__(self, reporter):
        super().__init__()
        self.reporter = reporter

    def run(self, info):
        self.reporter.seed_formats(info)
        return [], info
