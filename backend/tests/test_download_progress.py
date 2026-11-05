from unittest.mock import MagicMock

from backend.app import pipeline
from backend.app.adapters import ytdlp


def packet(format_id, downloaded, total, *, status="downloading", speed=1024, eta=10):
    return {
        "status": status, "downloaded_bytes": downloaded, "total_bytes": total,
        "speed": speed, "eta": eta,
        "info_dict": {
            "format_id": format_id,
            "requested_formats": [
                {"format_id": "video", "filesize": 900},
                {"format_id": "audio", "filesize": 100},
            ],
        },
    }


def test_download_progress_accounts_for_both_video_and_audio():
    callback = MagicMock()
    reporter = ytdlp.DownloadProgress(callback)
    reporter.download(packet("video", 450, 900))
    assert callback.call_args.args[0] == 45
    assert "1.0 KiB/s" in callback.call_args.args[1]
    assert "00:10" in callback.call_args.args[1]
    reporter.download(packet("video", 900, 900, status="finished"))
    assert callback.call_args.args[0] == 90
    reporter.download(packet("audio", 50, 100))
    assert callback.call_args.args[0] == 95
    reporter.download(packet("audio", 100, 100, status="finished"))
    assert callback.call_args.args[0] == 99


def test_real_hook_packets_without_requested_formats_use_preseeded_streams():
    callback = MagicMock()
    reporter = ytdlp.DownloadProgress(callback)
    reporter.seed_formats(packet("video", 0, 900)["info_dict"])
    event = packet("video", 900, 900, status="finished")
    del event["info_dict"]["requested_formats"]
    reporter.download(event)
    assert callback.call_args.args[0] == 90
    event = packet("audio", 50, 100)
    del event["info_dict"]["requested_formats"]
    reporter.download(event)
    assert callback.call_args.args[0] == 95


def test_pre_download_seed_hook_does_not_publish_merging_progress():
    callback = MagicMock()
    reporter = ytdlp.DownloadProgress(callback)
    reporter.postprocess({"status": "started", "postprocessor": "DownloadProgressSeed"})
    callback.assert_not_called()


def test_installed_ytdlp_before_download_processor_seeds_real_hook_shape():
    callback = MagicMock()
    reporter = ytdlp.DownloadProgress(callback)
    with ytdlp.yt_dlp.YoutubeDL({
        "quiet": True, "postprocessor_hooks": [reporter.postprocess],
    }) as downloader:
        downloader.add_post_processor(ytdlp.DownloadProgressSeedPP(reporter), when="before_dl")
        info = packet("video", 0, 900)["info_dict"]
        info.update(id="test", title="test", ext="mp4")
        for fmt in info["requested_formats"]:
            fmt["url"] = "https://example.invalid/media"
        downloader.pre_process(info, "before_dl")
    callback.assert_not_called()
    event = packet("video", 900, 900, status="finished")
    event["info_dict"].pop("requested_formats")
    reporter.download(event)
    assert callback.call_args.args[0] == 90


def test_unknown_totals_do_not_fabricate_percent_speed_or_eta():
    callback = MagicMock()
    reporter = ytdlp.DownloadProgress(callback)
    reporter.download({"status": "downloading", "downloaded_bytes": 2048, "info_dict": {}})
    percent, message = callback.call_args.args
    assert percent == 0
    assert "2.0 KiB" in message
    assert "unknown" in message.lower()
    assert "ETA" not in message
    assert "/s" not in message


def test_finished_stream_corrects_an_overestimated_size():
    callback = MagicMock()
    reporter = ytdlp.DownloadProgress(callback)
    reporter.seed_formats(packet("video", 0, 900)["info_dict"])
    event = packet("video", 800, None, status="finished")
    event["info_dict"].pop("requested_formats")
    reporter.download(event)
    assert callback.call_args.args[0] == 88


def test_merge_hook_does_not_mark_stage_complete():
    callback = MagicMock()
    reporter = ytdlp.DownloadProgress(callback)
    reporter.postprocess({"status": "started", "postprocessor": "Merger"})
    assert callback.call_args.args == (99, "Merging audio and video")


def test_download_options_connect_hooks(monkeypatch, tmp_path):
    observed = []

    class FakeDL:
        def __init__(self, options):
            observed.append(options)
            self.seed = None
        def add_post_processor(self, processor, when):
            assert when == "before_dl"
            self.seed = processor
        def __enter__(self):
            return self
        def __exit__(self, *args):
            return False
        def download(self, urls):
            options = observed[-1]
            self.seed.run(packet("video", 0, 900)["info_dict"])
            event = packet("video", 450, 900)
            del event["info_dict"]["requested_formats"]
            options["progress_hooks"][0](event)

    monkeypatch.setattr(ytdlp, "_ydl_base", lambda *args: {})
    monkeypatch.setattr(ytdlp.yt_dlp, "YoutubeDL", FakeDL)
    callback = MagicMock()
    ytdlp._download_with_format_candidates("url", tmp_path / "video.mp4", None, "", callback)
    assert callback.call_args.args[0] == 45
    assert observed[0]["postprocessor_hooks"]


def test_stage_speed_updates_when_integer_percent_is_unchanged(monkeypatch):
    update = MagicMock()
    monkeypatch.setattr(pipeline.database, "update_stage", update)
    clock = iter([0.0, 1.0, 2.1])
    monkeypatch.setattr(pipeline, "monotonic", lambda: next(clock))
    runner = pipeline.PipelineRunner("task")
    runner.stage_progress("download", 50, "10 KiB/s")
    runner.stage_progress("download", 50, "20 KiB/s")
    runner.stage_progress("download", 50, "30 KiB/s")
    assert update.call_count == 2
    assert update.call_args.kwargs["last_message"] == "30 KiB/s"
