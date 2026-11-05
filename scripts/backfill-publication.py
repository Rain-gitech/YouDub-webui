"""Backfill publication copy without rerunning any media stage."""
import argparse
import json
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from backend.app import database
from backend.app.adapters.publication import generate_publication, read_publication


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--watch", action="store_true", help="Continuously backfill tasks as their downloads finish")
    args = parser.parse_args()
    errors = set()
    while True:
        tasks = database.list_tasks(limit=-1)
        for summary in tasks:
            task = database.get_task(summary["id"])
            if not task:
                continue
            session = Path(task["session_path"]) if task.get("session_path") else None
            if not session or not any((session / "metadata" / name).exists() for name in ("ytdlp_info.json", "local_info.json")):
                continue
            if task["id"] in errors:
                continue
            try:
                if read_publication(session):
                    continue
                data = generate_publication(session, database.get_openai_settings())
                print(json.dumps({"id": task["id"], "title_zh": data["title_zh"],
                                  "has_description": bool(data["description_zh"])}, ensure_ascii=False), flush=True)
            except Exception as exc:
                errors.add(task["id"])
                print(json.dumps({"id": task["id"], "error": type(exc).__name__}), flush=True)
        if not args.watch:
            break
        time.sleep(10)
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
