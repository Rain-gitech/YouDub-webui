#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QSettings>
#include <QTranslator>
#include <QUrl>

int main(int argc, char *argv[]) {
    // 高 DPI 支持。
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("YouDub"));
    QGuiApplication::setOrganizationName(QStringLiteral("YouDub"));

    // i18n：源串为中文（默认）。若用户选 en，加载英文翻译 .qm。
    // 优先嵌入资源 ":/i18n/youdub-qt_en.qm"，其次 exe 同级 translations/ 目录。
    QSettings settings;
    const QString lang = settings.value(QStringLiteral("ui/language"), QStringLiteral("zh")).toString();
    if (lang == QStringLiteral("en")) {
        auto *translator = new QTranslator(&app);
        bool loaded = translator->load(QStringLiteral(":/i18n/youdub-qt_en.qm"));
        if (!loaded) {
            loaded = translator->load(QStringLiteral("youdub-qt_en"),
                                      QStringLiteral("translations"));
        }
        if (loaded) QCoreApplication::installTranslator(translator);
    }

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
