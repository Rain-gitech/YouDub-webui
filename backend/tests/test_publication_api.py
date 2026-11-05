from backend.app import database
from backend.app.adapters import publication
from backend.tests.test_auth import client, login


def test_publication_read_generate_download(client, tmp_path, monkeypatch):
    csrf, _ = login(client)
    monkeypatch.setattr(database, "get_task", lambda task_id: {"session_path": str(tmp_path)} if task_id == "test" else None)
    assert client.get("/api/tasks/missing/publication").status_code == 404
    assert client.get("/api/tasks/test/publication").json() == {"publication": None}
    assert client.get("/api/tasks/test/artifact/publication").status_code == 404
    data = {"title_zh": "中文标题", "description_zh": "中文简介\nhttps://example.com", "uploader": "Author", "source_url": "https://example.com"}
    monkeypatch.setattr(publication, "generate_publication", lambda *args: data)
    assert client.post("/api/tasks/test/publication").status_code == 403
    response = client.post("/api/tasks/test/publication", headers={"X-CSRF-Token": csrf})
    assert response.status_code == 200
    assert response.json()["publication"]["title_zh"] == "中文标题"
    monkeypatch.setattr(publication, "read_publication", lambda *args: data)
    response = client.get("/api/tasks/test/artifact/publication")
    assert response.status_code == 200
    assert "中文简介" in response.text
    assert "attachment" in response.headers["content-disposition"]


def test_translation_failure_is_retryable_without_media_changes(client, tmp_path, monkeypatch):
    csrf, _ = login(client)
    monkeypatch.setattr(database, "get_task", lambda _: {"session_path": str(tmp_path)})
    def fail(*args):
        raise ValueError("Empty Chinese title")
    monkeypatch.setattr(publication, "generate_publication", fail)
    response = client.post("/api/tasks/test/publication", headers={"X-CSRF-Token": csrf})
    assert response.status_code == 422
    assert response.json()["detail"] == "Empty Chinese title"
