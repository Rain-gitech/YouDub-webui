from backend.workers.download_progress import DownloadProgress


def test_separate_streams_report_aggregate_progress_speed_and_current_file_eta():
    events = []
    report = DownloadProgress(lambda *args: events.append(args))
    report.seed_formats({'requested_formats': [{'format_id': 'video', 'filesize': 80}, {'format_id': 'audio', 'filesize': 20}]})
    report({'status': 'finished', 'info_dict': {'format_id': 'video'}, 'downloaded_bytes': 80})
    assert events[-1][0] == .8
    report({'status': 'downloading', 'info_dict': {'format_id': 'audio'}, 'downloaded_bytes': 5, 'total_bytes': 20, 'speed': 1024, 'eta': 15})
    assert events[-1][0] == .85
    assert 'KiB/s' in events[-1][1] and '00:15' in events[-1][1]
    report({'status': 'finished', 'info_dict': {'format_id': 'audio'}, 'downloaded_bytes': 20})
    assert events[-1][0] == .99


def test_unknown_size_does_not_invent_a_percentage():
    events = []
    report = DownloadProgress(lambda *args: events.append(args))
    report({'status': 'downloading', 'downloaded_bytes': 10})
    assert events[-1][0] is None
