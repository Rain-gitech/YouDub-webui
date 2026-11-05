import json
import os
import subprocess
import sys
from pathlib import Path

import pytest

from backend.app.adapters import publication


def source(tmp_path, description="Full original description\nhttps://example.com"):
    metadata = tmp_path / "metadata"
    metadata.mkdir()
    (metadata / "ytdlp_info.json").write_text(json.dumps({
        "title": "Original title", "description": description,
        "webpage_url": "https://youtube.com/watch?v=example", "uploader": "Author",
    }), encoding="utf-8")


def test_full_description_cache_and_download_text(tmp_path, monkeypatch):
    original = "Long description. " * 300 + "\nhttps://example.com"
    source(tmp_path, original)
    calls = []
    def translate(client, model, system, user):
        calls.append(json.loads(user))
        return {"title_zh": "中文标题", "description_zh": "完整简介\nhttps://example.com"}
    monkeypatch.setattr(publication, "_client", lambda *args: object())
    monkeypatch.setattr(publication, "_call_json", translate)
    settings = {"base_url": "test", "api_key": "test", "model": "test"}
    result = publication.generate_publication(tmp_path, settings)
    assert calls[0]["description"] == original
    assert result["title_zh"] == "中文标题"
    assert result["source_description"] == original
    assert "https://example.com" in publication.publication_text(result)
    assert publication.generate_publication(tmp_path, settings) == result
    assert len(calls) == 1


def test_no_description_is_not_invented(tmp_path, monkeypatch):
    source(tmp_path, "")
    monkeypatch.setattr(publication, "_client", lambda *args: object())
    monkeypatch.setattr(publication, "_call_json", lambda *args: {
        "title_zh": "标题", "description_zh": "Invented description",
    })
    result = publication.generate_publication(tmp_path, {"base_url": "", "api_key": "", "model": "test"})
    assert result["description_zh"] == ""


def test_failure_does_not_save_partial_artifact(tmp_path, monkeypatch):
    source(tmp_path)
    monkeypatch.setattr(publication, "_client", lambda *args: object())
    monkeypatch.setattr(publication, "_call_json", lambda *args: {"title_zh": "", "description_zh": ""})
    with pytest.raises(ValueError):
        publication.generate_publication(tmp_path, {"base_url": "", "api_key": "", "model": "test"})
    assert publication.read_publication(tmp_path) is None


def test_two_processes_only_translate_once(tmp_path):
    source(tmp_path, "")
    marker = tmp_path / "calls.txt"
    code = """
import sys, time
from pathlib import Path
from backend.app.adapters import publication
publication._client = lambda *args: object()
def translate(*args):
    with open(sys.argv[2], 'a') as handle:
        handle.write('call\\n')
    time.sleep(0.5)
    return {'title_zh': 'Chinese title', 'description_zh': ''}
publication._call_json = translate
publication.generate_publication(Path(sys.argv[1]), {'base_url':'test','api_key':'test','model':'test'})
"""
    env = dict(os.environ, PYTHONPATH=str(Path(__file__).resolve().parents[2]))
    processes = [subprocess.Popen([sys.executable, "-c", code, str(tmp_path), str(marker)], env=env) for _ in range(2)]
    try:
        assert [process.wait(timeout=30) for process in processes] == [0, 0]
        assert marker.read_text().splitlines() == ["call"]
    finally:
        for process in processes:
            if process.poll() is None:
                process.kill()
                process.wait()
