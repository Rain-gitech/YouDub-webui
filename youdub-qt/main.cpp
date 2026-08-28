#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QUrl>

int main(int argc, char *argv[]) {
    // 高 DPI 支持。
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("YouDub"));
    QGuiApplication::setOrganizationName(QStringLiteral("YouDub"));

    // Qt Quick Controls 2 风格：用 Basic（轻量、跨平台一致）。
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;

    // 单例类型在 QML 中 import YouDub 后可直接使用：
    //   import YouDub
    //   AuthManager { ... }  // 或通过 AuthManager.xxx 调用
    engine.loadFromModule("YouDub", "Main");
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    return app.exec();
}
