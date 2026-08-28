# YouDub Qt-QML 前端设计

- 日期：2026-08-26
- 状态：已确认，待实现计划
- 范围：用 Qt6 / C++ / QML 重写 YouDub-webui 的前端，作为现有 Next.js WebUI 的原生桌面替代，目标平台为 Windows，功能与现有 WebUI 全面对齐。
- 后端：保留现有 FastAPI 后端不动，Qt 客户端通过 HTTP 消费同一套 `/api` 接口。

## 1. 背景与目标

- 动机：学习 / 技术探索，顺便给项目换一个原生前端。功能对齐现有 WebUI 即可，不强求超越。
- 平台：仅 Windows。
- 范围：全功能对齐——登录、任务创建（URL + 本地视频/字幕上传）、任务列表（分页/过滤/排序/轮询）、任务详情（阶段/日志/动作）、最终视频回放与下载、设置（YouTube Cookie / OpenAI / yt-dlp 代理）。
- 现有前端契约基准：`apps/web/src/lib/api.ts`（类型与端点）；后端鉴权：`backend/app/auth.py`。

## 2. 可行性结论

可行，无阻断性问题。逐项核对：

| 现有 Web 机制 | Qt 端方案 | 结论 |
|---|---|---|
| HttpOnly Cookie 会话 | `QNetworkAccessManager` + `QNetworkCookieJar`；HttpOnly 对原生端无效，自动存储并回带 | 可行 |
| CSRF（`X-CSRF-Token` 头） | 登录响应 JSON 中的 `csrf_token` 存客户端，不安全方法带上请求头 | 可行 |
| CORS / Origin 校验 | CORS 是浏览器概念，原生请求不触发；`auth.py` 的 `_origin_allowed` 在无 Origin 头时直接放行 | 无障碍 |
| 任务列表轮询 | `QTimer` 周期拉取，对齐 `useSerialPolling` | 可行 |
| 4GiB 本地视频 multipart 上传 | `QHttpMultiPart` + 文件 `QIODevice` 流式发送，避免读入内存 | 可行（需小心） |
| 中英 i18n | `qsTr` + `QTranslator`，或 QML 内 JS 字典 | 可行 |

唯一折中点：最终视频回放。播放接口 `/api/tasks/{task_id}/artifact/final-video` 需要 Cookie 鉴权，而 `QtMultimedia` 的 `MediaPlayer`（Windows 下走自身 ffmpeg 后端）不保证复用 `QNetworkAccessManager` 的 `CookieJar`。稳妥做法：先鉴权下载到本地临时文件，再播放本地文件。

## 3. 技术路线选型

候选三条：

- **A. PySide6 + QML**：QML 视图 + Python 胶水逻辑 / API 客户端 / 模型。迭代最快，贴合 Python 既有技能。
- **B. C++ + QML**：QML 视图 + C++ 后端逻辑 / 模型 / 网络。最正统原生 Qt，性能最高，但要额外扛 C++ 与 CMake。
- **C. 纯 QML + JS（XMLHttpRequest）**：单语言最轻，但 4GiB 大文件上传与视频回放会撞墙，不推荐。

**选定：B（C++ + QML）。** 理由：用户明确希望学习正统 Qt 栈；后续如需，胶水逻辑可平滑迁移。

## 4. 架构设计

分层 MVVM：

| 层 | 语言 | 职责 |
|---|---|---|
| View | QML | 页面与组件，只做布局 / 绑定 / 触发动作，不直接碰网络 |
| ViewModel | C++ `QObject` | 每页一个，持有 UI 状态（分页 / 过滤 / 轮询 / 提交态），调用 ApiClient，发信号驱动 QML |
| Model | C++ | `QAbstractListModel` 子类（任务列表、阶段列表）+ 值对象（Task / Settings） |
| Service | C++ | `ApiClient`（`QNetworkAccessManager` + `CookieJar` + CSRF）、`AuthManager`、`MultipartUpload`、`VideoArtifact` |

C++ / QML 边界用 `QML_ELEMENT` / `QML_SINGLETON` 注册，QML 直接 `import` 使用（比 context property 更现代、更可测）。

### 4.1 项目结构（CMake + Qt6）

```
youdub-qt/
├─ CMakeLists.txt
├─ main.cpp                      # QGuiApplication + QQmlApplicationEngine + 类型注册
├─ src/
│  ├─ network/
│  │  ├─ ApiClient.{h,cpp}       # 所有端点; 持有 QNAM + CookieJar + csrf token
│  │  ├─ AuthManager.{h,cpp}     # login/session/logout; 发 authenticatedChanged
│  │  └─ CookieJar.{h,cpp}       # QNetworkCookieJar, 可选持久化到磁盘
│  ├─ models/
│  │  ├─ TaskListModel.{h,cpp}   # QAbstractListModel (当前页行)
│  │  ├─ StageModel.{h,cpp}      # QAbstractListModel (单任务阶段)
│  │  ├─ TaskSummary.h           # 值对象 / struct
│  │  └─ TaskDetail.h            # QObject 包装单任务 + stages
│  ├─ viewmodels/
│  │  ├─ TaskListVm.{h,cpp}      # 分页/过滤/排序状态 + QTimer 轮询
│  │  ├─ TaskDetailVm.{h,cpp}    # 详情/日志轮询/动作(rerun/resume/continue/redo/delete)
│  │  ├─ CreateTaskVm.{h,cpp}    # URL 创建 + 本地上传触发
│  │  └─ SettingsVm.{h,cpp}
│  ├─ upload/
│  │  └─ MultipartUpload.{h,cpp} # QHttpMultiPart 流式 + uploadProgress
│  └─ video/
│     └─ VideoArtifact.{h,cpp}   # 鉴权下载到 QTemporaryFile, 暴露 local file url
├─ qml/
│  ├─ App.qml                    # StackView 导航壳
│  ├─ pages/    Login | Home | TaskDetail | Settings
│  ├─ components/ StatusBadge | StageItem | TaskListItem | ...
│  └─ Theme.qml (singleton)
└─ i18n/ zh.ts | en.ts
```

### 4.2 关键数据流

**鉴权（Cookie + CSRF）** —— 对齐 `backend/app/auth.py`

- 启动 → `AuthManager.checkSession()` → `GET /api/auth/session`（原生请求不带 Origin；`_origin_allowed` 无 Origin 直接放行）。200 → 存 `csrf_token`，发 `authenticated`；401 → 显示 Login。
- 登录 → `POST /api/auth/login {password}` → 200：`CookieJar` 自动存 session cookie（HttpOnly 对原生端无效），存 csrf，跳转 Home。
- 不安全方法 → `ApiClient` 统一加 `X-CSRF-Token` 头（对齐 `apps/web/src/lib/api.ts` 的 `requestHeaders`）。
- 任意 401 → `AuthManager` 发 `unauthorized` → `StackView` 弹回 Login，清 csrf。

**大文件上传（4GiB multipart）**

- `CreateTaskVm.uploadLocal()` → `MultipartUpload` 用 `QHttpMultiPart`，文件部分用 `QHttpPart::setBodyDevice(QFile*)` 流式（不读入内存），POST `/api/tasks/upload`。`uploadProgress` 信号驱动进度条。201 → 跳转详情。

**最终视频回放**

- `VideoArtifact` 鉴权 GET `/api/tasks/{id}/artifact/final-video` → 写 `QTemporaryFile` → 把 `file://` 本地 URL 给 `MediaPlayer` 播放（绕过播放器不共享 CookieJar）。下载按钮用 `QFileDialog` 另存。

**轮询**

- `TaskListVm` 持 `QTimer`（2–3 s），过滤 / 分页变化立即刷新（对齐 `useSerialPolling`）；`TaskDetailVm` 同理轮询详情与日志。

**错误处理**

- `ApiClient` 把非 2xx 统一成 `ApiError{status, detail}` 信号；ViewModel 转成 `errorMessage` 属性给 QML 显示 toast / 横幅（对齐 `api.ts` 的 `errorMessage()`）。

### 4.3 后端契约对照（C++ 数据结构）

以 `apps/web/src/lib/api.ts` 类型为基准：

- `Task`：id, url, title, status, current_stage, session_path, final_video_path, error_message, created_at, started_at, completed_at, execution_mode, stages[]。
- `TaskStage`：task_id, name, label, status, progress, started_at, completed_at, last_message, error_message。
- `TaskSummary`：列表行子集（id, url, title, status, current_stage, final_video_path, error_message, created_at, started_at, completed_at, execution_mode?）。
- 枚举：`StageStatus`、`TaskStatus`、`ExecutionMode`、`LocalDirection`、`TaskListStatus/Sort/ExecutionMode`。

端点清单（全部 `ApiClient` 方法化）：

- `POST /api/auth/login`、`GET /api/auth/session`、`POST /api/auth/logout`
- `POST /api/tasks`、`POST /api/tasks/upload`、`GET /api/tasks`、`GET /api/tasks/current`、`GET /api/tasks/{id}`、`DELETE /api/tasks/{id}`、`POST /api/tasks/{id}/rerun`、`POST /api/tasks/{id}/resume`、`POST /api/tasks/{id}/continue`、`POST /api/tasks/{id}/stages/{stage}/redo`、`GET /api/tasks/{id}/log`、`GET /api/tasks/{id}/artifact/final-video`
- `GET|POST /api/cookies/youtube`、`GET|POST /api/settings/openai`、`POST /api/settings/openai/models`、`GET|POST /api/settings/ytdlp`

## 5. 分阶段路线图

| 阶段 | 目标 | 交付物 |
|---|---|---|
| 1 | 脚手架 + 鉴权 | CMake/Qt6 工程、`ApiClient`+`AuthManager`+`CookieJar`、Login 页、会话检查、导航壳 |
| 2 | 任务列表 + URL 创建 | `TaskListModel`+`TaskListVm`、分页/过滤/排序/轮询、首页创建表单 |
| 3 | 任务详情 + 动作 | `TaskDetailVm`+`StageModel`、阶段进度/日志轮询、rerun/resume/continue/redo/delete |
| 4 | 本地上传 | `MultipartUpload`、视频+字幕上传、进度、方向选择 |
| 5 | 视频回放 + 下载 | `VideoArtifact` 鉴权下载+本地播放、下载另存 |
| 6 | 设置页 | YouTube Cookie、OpenAI(含 get models)、yt-dlp 代理 |
| 7 | i18n + 打磨 + 打包 | zh/en `qsTr`、主题、`windeployqt`/CPack 出 Windows 包 |

## 6. 测试策略

- C++：`QtTest` 测 `TaskListModel` / `StageModel` / `AuthManager` 状态机（用假 `ApiClient` 接口隔离网络）。
- QML：`QtQuickTest`（`TestCase` / `SignalSpy`）测页面绑定与交互。
- 契约对齐：以 `apps/web/src/lib/api.ts` 的类型定义为 C++ 数据结构对照基准。

## 7. 风险与备注

- **视频回放鉴权**：`MediaPlayer` 与 `CookieJar` 共享不确定，采用下载到临时文件再播放的稳妥路径；大视频需关注磁盘与首播延迟。
- **大文件上传内存**：必须用 `setBodyDevice` 流式，禁止 `setBody(QByteArray)`。
- **CSRF 与会话失效**：401 全局捕获回登录页；Cookie 过期由后端 `credential_version` 校验，客户端按 401 处理即可。
- **打包体积**：C++ 路线二进制小，但 Qt 运行时需 `windeployqt` 收集；学习项目可接受。
- **后端不动**：本设计不修改 `backend/`；如后续要让 `MediaPlayer` 直连鉴权流，再评估后端加临时签名 URL 的可行性（本期不做）。
