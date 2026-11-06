"""Launch the independent personal Harness profile; classic data is read-only."""
from __future__ import annotations

import argparse
import json
import os
import shutil
import sqlite3
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / 'data' / 'personal'


def legacy_settings(legacy: Path) -> dict[str, str]:
    database = legacy / 'data' / 'youdub.sqlite'
    if not database.is_file():
        raise RuntimeError('The classic task database is missing; specify --legacy-root.')
    with sqlite3.connect(f'file:{database.as_posix()}?mode=ro', uri=True) as connection:
        return dict(connection.execute("SELECT key,value FROM settings WHERE key LIKE 'openai.%'"))


def configure(legacy: Path) -> tuple[Path, dict[str, str]]:
    if legacy.resolve() == ROOT.resolve():
        raise RuntimeError('Classic and new runtime roots must be different.')
    settings = legacy_settings(legacy)
    data = LOCAL / 'runtime'
    data.mkdir(parents=True, exist_ok=True)
    home = LOCAL / 'home'
    profile = home / 'profiles' / 'youdub'
    profile.mkdir(parents=True, exist_ok=True)
    python = ROOT / '.venv-personal' / ('Scripts/python.exe' if os.name == 'nt' else 'bin/python')
    if not python.is_file():
        raise RuntimeError('Create the isolated .venv-personal first; see docs/personal-upstream.md.')
    old_environment = {}
    from dotenv import dotenv_values
    old_environment.update(dotenv_values(legacy / '.env'))
    whisper = Path(old_environment.get('WHISPER_DOWNLOAD_ROOT') or Path.home() / '.cache' / 'whisper')
    voxcpm = Path(old_environment.get('VOXCPM_MODEL_DIR') or legacy / 'data/modelscope/OpenBMB__VoxCPM1.5')
    # Copy only tools, never task storage, cookies, or the classic database.
    manifest = json.loads((ROOT / 'packages/builtin/src/setup-runtime.json').read_text(encoding='utf-8'))
    ffmpeg = data / 'runtime/ffmpeg' / manifest['ffmpeg']['win32-x64'][0]['directory'] / 'bin'
    ffmpeg.mkdir(parents=True, exist_ok=True)
    source = Path(old_environment.get('FFMPEG_PATH') or legacy / 'data/tools/ffmpeg/bin/ffmpeg.exe').parent
    for file in source.iterdir():
        target = ffmpeg / file.name
        if file.is_file() and not target.exists():
            shutil.copy2(file, target)
    model = settings.get('openai.model') or old_environment.get('OPENAI_MODEL') or 'deepseek-chat'
    patches = [
        {'id': 'youdub-subtitles', 'config': {
            'python': str(python), 'modelsDir': str(whisper), 'dataDir': str(data), 'reuseRuntime': True,
            'modelDirectories': {'whisper': str(whisper), 'voxcpm': str(voxcpm)},
        }},
        {'id': 'llm-pi-ai', 'config': {'providers': {'personal-deepseek': {
            'api': 'openai-completions', 'baseURL': settings.get('openai.base_url') or 'https://api.deepseek.com',
            'apiKeyEnv': 'YOUDUB_PERSONAL_DEEPSEEK_KEY', 'compat': {'maxTokensField': 'max_tokens'},
            'models': [{'id': model, 'name': model, 'input': ['text'], 'contextWindow': 65536, 'maxTokens': 8192}],
        }}}},
    ]
    patch_file = profile / 'cordis.patch.yml'
    if not patch_file.exists():
        patch_file.write_text(json.dumps(patches, ensure_ascii=False, indent=2), encoding='utf-8')
    (LOCAL / 'paths.json').write_text(json.dumps({'legacyRoot': str(legacy)}, ensure_ascii=False), encoding='utf-8')
    env = dict(os.environ)
    env.update({'YOUDUB_HOME': str(home), 'YOUDUB_PERSONAL_DEEPSEEK_KEY': settings.get('openai.api_key') or old_environment.get('OPENAI_API_KEY') or '',
                'VOXCPM_INIT_DTYPE': 'bfloat16', 'WHISPER_LOAD_ON_CPU': 'true', 'PYTHONUTF8': '1',
                'PYTHONUNBUFFERED': '1', 'YOUDUB_MEDIA_ROOT': str(ROOT)})
    return home, env


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--legacy-root', type=Path)
    parser.add_argument('--prepare-only', action='store_true')
    args = parser.parse_args()
    saved = LOCAL / 'paths.json'
    legacy = args.legacy_root or (Path(json.loads(saved.read_text(encoding='utf-8'))['legacyRoot']) if saved.exists() else None)
    if legacy is None:
        parser.error('First launch needs --legacy-root pointing to the preserved classic checkout.')
    _, env = configure(legacy.resolve())
    if args.prepare_only:
        print('Separate personal runtime configured. Classic task data was not modified.')
        return 0
    node = shutil.which('node')
    if not node:
        raise RuntimeError('Node.js is missing.')
    print('New personal workspace: http://127.0.0.1:3086/ | Classic history: http://127.0.0.1:3000/', flush=True)
    return subprocess.call([node, str(ROOT / 'apps/harness/launch.mjs')], cwd=ROOT, env=env)


if __name__ == '__main__':
    raise SystemExit(main())
