import asyncio

from backend.app import main


def test_api_only_does_not_reset_tasks_or_start_worker(monkeypatch):
    monkeypatch.setenv("YOUDUB_API_ONLY", "true")
    for name in ("init_db", "delete_expired_auth_sessions", "backfill_titles_from_metadata"):
        monkeypatch.setattr(main.database, name, lambda *args: None)
    monkeypatch.setattr(main, "ensure_runtime_dirs", lambda: None)
    monkeypatch.setattr(main.auth, "validate_auth_configuration", lambda: None)
    def forbidden(*args):
        raise AssertionError("API-only startup touched the GPU worker or task state")
    monkeypatch.setattr(main.database, "fail_stale_active_tasks", forbidden)
    monkeypatch.setattr(main.worker, "start", forbidden)
    async def startup():
        async with main.lifespan(main.app):
            pass
    asyncio.run(startup())
