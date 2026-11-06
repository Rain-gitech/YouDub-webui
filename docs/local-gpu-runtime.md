# 本机 GPU 运行环境

## 启动

双击项目根目录的 `start-youdub.cmd`，保持启动窗口打开，然后访问
<http://127.0.0.1:3000>。在窗口按 Ctrl+C 停止服务。

启动器固定使用项目内 `.venv-gpu`，先运行一次真实 CUDA 运算，再启动后端
和网页。CUDA 不可用时直接报错，不会自动退回 CPU。
如果提示 3000 或 8000 端口已占用，先关闭之前的 YouDub 启动窗口。
后端日志在 `data/launcher/backend.stdout.log` 和 `backend.stderr.log`。

原来的 `.venv`、任务数据、模型和 API/登录配置均保留。
当前网页仍使用 `.env` 中原有的登录密码哈希；本次没有重置密码。

## 已确定的问题

- 旧 `.venv` 的 PyTorch 是 `2.13.0+cpu`，没有 CUDA 支持。
- 历史任务日志明确记录 `demucs=cpu, whisper=cpu`。
- 该历史任务分离约 757 秒、识别约 505 秒，后一次运行的 12 句配音约
  1945 秒。配音阶段包含可能的加载开销，不能当作纯推理基准。
- 原 `.env` 的 FFmpeg 路径已经失效。
- CUDA 驱动 API 能识别 GTX 1660 Ti 6GB，计算能力 7.5，报告驱动接口
  版本 11.4。`nvidia-smi` 的 NVML 初始化失败，但 CUDA 运算可以成功。

## 环境与配置

- Python 3.12，独立目录 `.venv-gpu`。
- PyTorch `2.7.1+cu118`，torchaudio `2.7.1+cu118`。
- FFmpeg 9.0.2 shared，位于 `data/tools/ffmpeg/bin`。
- `DEVICE=cuda`，各组件默认继承此设置。
- `VOXCPM_OPTIMIZE=false`：本机先使用不依赖 Triton/动态编译的路径。
- `RELEASE_MODELS_AFTER_STAGE=true`：识别和配音阶段结束后释放模型缓存，
  分离阶段结束后清理未使用的 CUDA 缓存，避免多个模型挤占 6GB 显存。
- 本机使用官方 VoxCPM1.5，支持中英双向配音，保留 10 步推理。
  该模型的克隆接口需要“原音频 + 对应原文”，适配器已自动处理。
- `VOXCPM_INIT_DTYPE=bfloat16`：构造时直接使用检查点精度，降低内存峰值，
  加载结束或失败时均恢复 PyTorch 的默认 dtype。
- 本地 VoxCPM1.5 配置将 `max_length` 从 8192 调为 2048，减少逐句配音的
  KV 缓存；原配置保存在同目录 `config.original.json`。
- 原 VoxCPM2 文件保留。实测其加载在已分配约 4.65GiB 显存时仍要求额外
  显存并失败，所以没有将它作为这台 6GB 显卡的默认模型。
- Whisper 的检查点先在 CPU 内存读取，再将模型权重移到 GPU，避免加载时
  检查点和模型两份数据同时挤占显存；识别计算仍在 GPU 上执行。

备份的原配置位于 `data/diagnostics/env-before-gpu.env`，其中包含私人配置，
不要提交或分享此文件。`.env` 和该诊断目录均被 Git 忽略。

## 恢复依赖

在项目根目录的 PowerShell 执行：

```powershell
py -3.12 -m venv .venv-gpu
.\.venv-gpu\Scripts\python.exe -m pip install -r requirements-pytorch-cu118.txt
.\.venv-gpu\Scripts\python.exe -m pip install -r requirements.txt -c constraints-gpu-cu118.txt --find-links data/diagnostics/wheels
```

后续安装 Python 依赖时继续使用 `-c constraints-gpu-cu118.txt`，避免误换成
CPU 版或不匹配的 torch/torchaudio。不要在这台旧驱动机器上直接安装
`requirements-pytorch-cu128.txt`。

本机已有的 `diffq` Python 3.12 Windows wheel 也保存在上述缓存中，恢复时使用
`--find-links` 可避免因为没有配置 C++ 编译工具而构建失败。

CUDA 安装包已按 PyTorch 官方索引提供的 SHA256 校验；FFmpeg 安装包也已按
Gyan 官方 SHA256 校验。大型包下载缓存保存在 `data/diagnostics/wheels`。

参考：[PyTorch 历史版本](https://docs.pytorch.org/get-started/previous-versions/)、
[NVIDIA CUDA 小版本兼容规则](https://docs.nvidia.com/deploy/cuda-compatibility/minor-version-compatibility.html)。

## 本地诊断

诊断脚本在 `data/diagnostics`，输出也保存在该目录，不覆盖历史任务：

```powershell
.\.venv-gpu\Scripts\python.exe data/diagnostics/benchmark_speech.py asr
.\.venv-gpu\Scripts\python.exe data/diagnostics/benchmark_speech.py tts --clips 3
.\.venv-gpu\Scripts\python.exe data/diagnostics/benchmark_speech.py separate
```

每次运行会生成独立目录和 `report.json`，记录加载时间、推理时间、显存峰值、
音频长度以及清理后的显存占用。诊断使用现有样本，不调用翻译 API。

## 实测结果

样本为现有任务的约 41.4 秒视频。以下为单机实测，包含首次加载与冷缓存影响，
不是各种视频的固定性能保证。

| 阶段 | 实测 |
| --- | --- |
| Demucs htdemucs_ft，shifts=3 | 含加载共 44.7 秒，显存峰值 0.54GiB |
| Whisper large-v3-turbo | 加载 13.7 秒，识别 47.9 秒，显存峰值 3.54GiB |
| VoxCPM1.5 三句中文配音 | 加载 33.0 秒，配音阶段 63.2 秒，输出 15.68 秒音频，显存峰值 2.61GiB |

识别计入加载约 61.6 秒，历史 CPU 任务约 505 秒，约快 8 倍。
分离阶段由历史约 757 秒降至约 44.7 秒，约快 17 倍。
配音测试换用了更小模型且仅测三句，不能直接与旧模型的整段耗时计算倍数。
配音阶段还包含首次文本归一化等开销。

识别与配音结束后的 PyTorch 已分配显存均降至约 0.008GiB。
CUDA 上下文/驱动占用不包含在此值中。

最终验证：后端完整测试 313 项通过、34 项按平台/条件跳过；`pip check`
无依赖冲突。启动器实测前后端均返回 HTTP 200，Ctrl+C 后两个端口均释放。
未重新执行视频下载和外部翻译 API；上述为本地三个模型阶段的独立实测。
# 中文发布资料

每个任务的 `metadata/publication.zh.json` 保存原文、中文标题、完整中文简介、作者和来源；`publication.zh.txt` 可直接用于发布。任务详情提供复制和 TXT 下载。缺失简介保持为空，不生成简介。翻译使用设置中的文本模型，独立缓存，不重新处理视频。

已下载任务可通过 `.venv-gpu\Scripts\python.exe scripts\backfill-publication.py` 补齐；加 `--watch` 会持续检查新下载。生成失败可在任务详情单独重试。跨进程文件锁避免批量补齐与前端同时重复调用模型。

为不打断 2026-11-05 正在运行的旧 GPU 队列，本次临时启用了 8001 端口的接口服务（`YOUDUB_API_ONLY=true`），以及 `data/launcher/publication-backfill.pid` 对应的补齐进程。接口服务不重置任务、不启动 GPU worker。相关日志与 PID 均在 `data/launcher/publication-*`。

正常启动脚本强制将发布接口转回 8000，并禁用 API-only 模式；更新后的主后端在翻译阶段自动生成发布资料，因此正常启动无需额外服务。若手工启动前端，注意 `apps/web/.env.local` 中的 `NEXT_SERVER_PUBLICATION_API_BASE_URL` 是本次临时部署值，可删除该变量以使用主后端。不要在队列运行中停止主后端。
# Classic runtime only

This document describes the preserved Next.js/FastAPI edition at commit `543f3fb`. For the integrated v0.0.12 Harness edition, use [personal-upstream.md](personal-upstream.md) and `scripts/start-personal.py`.
