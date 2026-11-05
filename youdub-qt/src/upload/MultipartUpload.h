#pragma once

#include <QObject>
#include <QString>
#include <qqmlintegration.h>

namespace youdub {

class ApiClient;

// 一次性 multipart 上传器。POST /api/tasks/upload，流式发送视频与可选字幕。
// 对齐 apps/web/src/lib/api.ts uploadLocalTask 与后端 main.py upload_local_video。
// 用 QHttpPart::setBodyDevice(QFile*) 流式上传，避免把 4GiB 视频读入内存。
//
// 用法：new MultipartUpload(this); 连接信号; start(...); 完成后自删。
class MultipartUpload : public QObject {
    Q_OBJECT
public:
    explicit MultipartUpload(ApiClient *client, QObject *parent = nullptr);

    // direction: "en-zh" | "zh-en"。subtitlePath 可空。executionMode: "auto"|"manual"。
    void start(const QString &videoPath, const QString &subtitlePath,
               const QString &direction, const QString &executionMode);

signals:
    void progress(qint64 sent, qint64 total);
    // 成功：taskId 非空，error 空。失败：taskId 空，error 有内容。
    void finished(const QString &taskId, const QString &error);

private:
    ApiClient *client_;
};

} // namespace youdub
