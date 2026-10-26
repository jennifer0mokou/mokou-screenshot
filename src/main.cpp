#include "application.h"
#include "unixsignals.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QScreen>
#include <QTextStream>
int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName("MushroomShot");
    app.setApplicationDisplayName("蘑菇截图");
    app.setOrganizationName("fedokou");
    app.setDesktopFileName("io.github.fedokou.MushroomShot");
    app.setApplicationVersion(QStringLiteral(MUSHROOM_VERSION));
    app.setWindowIcon(QIcon(":/mushroom-shot.svg"));
    app.setQuitOnLastWindowClosed(false);
    UnixSignals terminationSignals;
    QCommandLineParser parser;
    parser.setApplicationDescription("Fedora KDE Wayland 截图、编辑与区域录屏");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOptions({ { { "c", "capture" }, "唤起区域截图" }, { { "s", "settings" }, "打开设置" },
        { "background", "在托盘待命" }, { "stop", "停止录屏并保存" },
        { "quit", "正常退出，完成正在录制的文件" }, { "demo", "打开演示画布" },
        { "capture-check", "诊断：捕获桌面到指定 PNG 后退出", "file" },
        { "record-check", "诊断：录制主屏幕中心区域两秒后退出", "directory" },
        { "check-audio", "诊断录屏时加入系统声音" }, { "check-webm", "诊断录屏使用 WebM" },
        { "check-cancel-start", "诊断录屏启动取消" } });
    parser.process(app);
    if (parser.isSet("capture-check")) {
        class Capture capture;
        QObject::connect(
            &terminationSignals, &UnixSignals::terminationRequested, &app, &QCoreApplication::quit);
        QObject::connect(&capture, &Capture::ready, &app, [&](const QImage& image) {
            const bool ok = image.save(parser.value("capture-check"), "PNG");
            QTextStream(stdout) << "Capture " << image.width() << "x" << image.height() << " saved=" << ok
                                << Qt::endl;
            app.exit(ok ? 0 : 1);
        });
        QObject::connect(&capture, &Capture::failed, &app, [&](const QString& s) {
            QTextStream(stderr) << s << Qt::endl;
            app.exit(1);
        });
        QTimer::singleShot(0, &capture, &Capture::start);
        return app.exec();
    }
    if (parser.isSet("record-check")) {
        Settings settings;
        settings.videoDirectory = parser.value("record-check");
        Recorder recorder;
        if (parser.isSet("check-webm"))
            settings.videoFormat = "webm";
        QObject::connect(&terminationSignals, &UnixSignals::terminationRequested, &app, [&] {
            if (recorder.busy())
                recorder.stop();
            else
                app.quit();
        });
        QObject::connect(&recorder, &Recorder::canceled, &app, [&] {
            QTextStream(stdout) << "Recording startup canceled" << Qt::endl;
            app.exit(0);
        });
        QObject::connect(&recorder, &Recorder::started, &app, [&] {
            QTextStream(stdout) << "Recording started" << Qt::endl;
            QTimer::singleShot(2000, &recorder, &Recorder::stop);
        });
        QObject::connect(&recorder, &Recorder::saved, &app, [&](const QString& s) {
            QTextStream(stdout) << s << Qt::endl;
            app.exit(0);
        });
        QObject::connect(&recorder, &Recorder::failed, &app, [&](const QString& s) {
            QTextStream(stderr) << s << Qt::endl;
            app.exit(1);
        });
        QTimer::singleShot(400, &app, [&] {
            const auto r = app.primaryScreen()->geometry();
            recorder.start(QRect(r.center() - QPoint(319, 179), QSize(639, 359)), settings,
                parser.isSet("check-audio"), false);
            if (parser.isSet("check-cancel-start"))
                recorder.stop();
        });
        return app.exec();
    }
    auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        QTextStream(stderr) << "无法连接用户 D-Bus 会话。请在 KDE 桌面会话中启动。" << Qt::endl;
        return 1;
    }
    const QString service = "io.github.fedokou.MushroomShot";
    if (!bus.registerService(service)) {
        QDBusInterface remote(service, "/", service, bus);
        const QString method = parser.isSet("settings") ? "ShowSettings"
            : parser.isSet("stop")                      ? "Stop"
            : parser.isSet("quit")                      ? "Quit"
            : parser.isSet("demo")                      ? "Demo"
                                                        : "Capture";
        if (parser.isSet("background"))
            return 0;
        const auto reply = remote.call(QDBus::NoBlock, method);
        if (reply.type() == QDBusMessage::ErrorMessage) {
            QTextStream(stderr) << reply.errorMessage() << Qt::endl;
            return 1;
        }
        return 0;
    }
    Application controller;
    QObject::connect(
        &terminationSignals, &UnixSignals::terminationRequested, &controller, &Application::Quit);
    QTimer::singleShot(0, &app, [&] {
        if (parser.isSet("settings"))
            controller.ShowSettings();
        else if (parser.isSet("stop"))
            controller.Stop();
        else if (parser.isSet("quit"))
            controller.Quit();
        else if (parser.isSet("demo"))
            controller.demo();
        else if (!parser.isSet("background"))
            controller.Capture();
    });
    return app.exec();
}
