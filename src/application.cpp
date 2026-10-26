#include "application.h"
#include "imageoutput.h"
#include "overlay.h"
#include "panels.h"
#include "shortcuts.h"
#include <QAction>
#include <QApplication>
#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QDesktopServices>
#include <QDir>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QScreen>
#include <QUrl>
namespace
{
const QString service = "io.github.fedokou.MokouScreenshot";
QIcon recordingIcon(const QColor& color)
{
    QPixmap icon = QIcon(":/mokou-screenshot.svg").pixmap(64, 64);
    QPainter painter(&icon);
    painter.setPen(QPen(Qt::white, 2));
    painter.setBrush(color);
    painter.drawEllipse(QRect(43, 43, 18, 18));
    painter.end();
    return QIcon(icon);
}
QRect desktopGeometry()
{
    QRect r;
    for (auto screen : QGuiApplication::screens())
        r = r.united(screen->geometry());
    return r;
}
}
Application::Application(QObject* parent)
    : QObject(parent)
    , recognition(document, settings)
{
    auto bus = QDBusConnection::sessionBus();
    bus.registerObject("/", this, QDBusConnection::ExportScriptableSlots);
    tray.setIcon(QIcon(":/mokou-screenshot.svg"));
    tray.setToolTip("Mokou Screenshot · PrtScn");
    trayMenu = std::make_unique<QMenu>();
    auto menu = trayMenu.get();
    captureAction = menu->addAction("区域截图");
    captureAction->setObjectName("capture");
    captureAction->setProperty("componentName", service);
    captureAction->setProperty("componentDisplayName", "Mokou Screenshot");
    connect(captureAction, &QAction::triggered, this, &Application::Capture);
    const auto registerShortcuts = [this] {
        QString error;
        if (!registerPrintShortcut(captureAction, &error))
            tray.setToolTip("Mokou Screenshot · " + error);
        else
            tray.setToolTip("Mokou Screenshot · PrtScn");
    };
    auto shortcutWatcher = new QDBusServiceWatcher(
        "org.kde.kglobalaccel", bus, QDBusServiceWatcher::WatchForRegistration, this);
    connect(shortcutWatcher, &QDBusServiceWatcher::serviceRegistered, this, registerShortcuts);
    registerShortcuts();
    menu->addAction("设置", this, &Application::ShowSettings);
    menu->addAction("打开截图目录", this, [this] {
        QDir().mkpath(settings.imageDirectory);
        QDesktopServices::openUrl(QUrl::fromLocalFile(settings.imageDirectory));
    });
    stopAction = menu->addAction("停止录屏", this, &Application::Stop);
    stopAction->setEnabled(false);
    menu->addSeparator();
    menu->addAction("退出", this, &Application::Quit);
    tray.setContextMenu(menu);
    tray.show();
    connect(&tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger)
            Capture();
    });
    connect(&capture, &Capture::ready, this, [this](const QImage& i) {
        if (captureGeometry != desktopGeometry()) {
            announce("显示布局已变化，请重新截图。");
            return;
        }
        openEditor(i);
    });
    connect(&capture, &Capture::failed, this, [this](const QString& s) { announce(s, true); });
    connect(&recognition, &RecognitionController::statusChanged, this, &Application::setEditorStatus);
    connect(&recognition, &RecognitionController::busyChanged, this, [this](bool busy) {
        for (auto o : overlays)
            if (o)
                o->setRecognitionBusy(busy);
    });
    connect(&recordingClock, &QTimer::timeout, this, [this] {
        const auto seconds = elapsed.elapsed() / 1000;
        tray.setToolTip(QString("Mokou Screenshot · 正在录屏 %1:%2 · 再按截图快捷键停止")
                .arg(seconds / 60, 2, 10, QChar('0'))
                .arg(seconds % 60, 2, 10, QChar('0')));
    });
    for (auto screen : QGuiApplication::screens())
        watchScreen(screen);
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this](QScreen* screen) {
        watchScreen(screen);
        screenLayoutChanged();
    });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, &Application::screenLayoutChanged);
    connect(&recorder, &Recorder::started, this, [this] {
        elapsed.start();
        tray.setIcon(recordingIcon(QColor("#ff4560")));
        recordingClock.start(500);
        stopAction->setEnabled(true);
        tray.setToolTip("Mokou Screenshot · 正在录屏，再按截图快捷键停止");
    });
    connect(&recorder, &Recorder::canceled, this, [this] {
        recordingClock.stop();
        stopAction->setEnabled(false);
        tray.setIcon(QIcon(":/mokou-screenshot.svg"));
        tray.setToolTip("Mokou Screenshot");
        if (quitting)
            qApp->quit();
        else
            announce("已取消录屏启动。");
    });
    connect(&recorder, &Recorder::finishing, this, [this] {
        recordingClock.stop();
        stopAction->setEnabled(false);
        tray.setIcon(recordingIcon(QColor("#e8b35e")));
        tray.setToolTip("Mokou Screenshot · 已停止录制，正在保存文件");
    });
    connect(&recorder, &Recorder::saved, this, [this](const QString& path) {
        recordingClock.stop();
        tray.setIcon(QIcon(":/mokou-screenshot.svg"));
        tray.setToolTip("Mokou Screenshot");
        announce("录屏已保存：" + path);
        if (quitting)
            qApp->quit();
    });
    connect(&recorder, &Recorder::failed, this, [this](const QString& s) {
        recordingClock.stop();
        tray.setIcon(QIcon(":/mokou-screenshot.svg"));
        stopAction->setEnabled(false);
        tray.setToolTip("Mokou Screenshot");
        announce(s, true);
        if (quitting)
            qApp->quit();
    });
}
Application::~Application()
{
    capture.cancel();
    closeEditor();
}
void Application::Capture()
{
    if (recorder.busy()) {
        Stop();
        return;
    }
    if (recordScheduled) {
        Stop();
        return;
    }
    if (quitting || capture.busy() || modalOpen)
        return;
    if (!overlays.isEmpty())
        return;
    captureGeometry = desktopGeometry();
    capture.start();
}
void Application::openEditor(const QImage& image, bool demoMode)
{
    closeEditor();
    const auto sessionId = editorGeneration;
    const auto geometry = desktopGeometry();
    document.reset(image, geometry.size(), geometry.topLeft());
    applyEditorPreferences();
    for (auto screen : QGuiApplication::screens()) {
        auto o = new Overlay(&document, screen, demoMode);
        overlays.append(o);
        connect(o, &QObject::destroyed, this, [this, sessionId] {
            if (sessionId == editorGeneration && !overlays.isEmpty())
                closeEditor();
        });
        connect(o, &Overlay::canceled, this, &Application::closeEditor);
        connect(o, &Overlay::accepted, this, [this] {
            if (saveImage(document.crop()))
                closeEditor();
        });
        connect(o, &Overlay::ocrRequested, this, [this] { runRecognition(AiClient::Ocr); });
        connect(o, &Overlay::translateRequested, this, [this] { runRecognition(AiClient::Translate); });
        connect(o, &Overlay::describeRequested, this, [this] { runRecognition(AiClient::Describe); });
        connect(o, &Overlay::preferencesChanged, this, &Application::saveEditorPreferences);
        connect(o, &Overlay::settingsRequested, this, &Application::ShowSettings);
        connect(o, &Overlay::collageRequested, this, &Application::collage);
        connect(o, &Overlay::recordRequested, this, &Application::record);
        connect(o, &Overlay::cancelRecognitionRequested, &recognition, &RecognitionController::cancel);
        o->show();
        o->activateWindow();
        o->setFocus();
    }
}
void Application::closeEditor()
{
    ++editorGeneration;
    cancelScheduledRecording();
    recognition.cancel();
    const auto old = overlays;
    overlays.clear();
    for (auto o : old)
        if (o)
            o->close();
    document.reset({ }, { });
}
void Application::showEditor(bool show)
{
    for (auto o : overlays)
        if (o)
            o->setVisible(show);
}
void Application::setEditorStatus(const QString& s)
{
    for (auto o : overlays)
        if (o)
            o->setStatus(s);
}
void Application::announce(const QString& text, bool error)
{
    tray.showMessage("Mokou Screenshot", text, error ? QSystemTrayIcon::Warning : QSystemTrayIcon::Information, 5000);
    if (error && overlays.isEmpty() && !quitting && !modalOpen) {
        modalOpen = true;
        QMessageBox::warning(nullptr, "Mokou Screenshot", text);
        modalOpen = false;
    }
}
bool Application::saveImage(const QImage& image)
{
    QString path, error;
    if (!ImageOutput::saveAndCopy(image, settings.imageDirectory, &path, &error)) {
        setEditorStatus(error);
        return false;
    }
    announce("已保存并复制到剪切板：" + path);
    return true;
}
void Application::runRecognition(AiClient::Task task)
{
    if (recognition.busy())
        return;
    if (document.selection.isEmpty()) {
        recognition.request(task);
        return;
    }
    if (!recognition.configured(task)) {
        ShowSettings();
        if (!recognition.configured(task))
            return;
    }
    recognition.request(task);
}
void Application::ShowSettings()
{
    if (modalOpen || recorder.busy())
        return;
    capture.cancel();
    cancelScheduledRecording();
    recognition.cancel();
    modalOpen = true;
    showEditor(false);
    const Settings before = settings;
    SettingsDialog dialog(settings);
    if (dialog.exec() == QDialog::Accepted) {
        applyEditorPreferences();
        recognition.settingsChanged(before);
    }
    modalOpen = false;
    showEditor(true);
}
void Application::collage()
{
    cancelScheduledRecording();
    recognition.cancel();
    modalOpen = true;
    showEditor(false);
    CollageDialog dialog(document.crop());
    if (dialog.exec() == QDialog::Accepted) {
        if (saveImage(dialog.resultImage()))
            closeEditor();
    }
    modalOpen = false;
    showEditor(true);
}
void Application::record(bool sys, bool mic)
{
    if (document.selection.isEmpty())
        return;
    const auto region = document.selection.toAlignedRect().translated(document.desktopOrigin);
    recognition.cancel();
    recordScheduled = true;
    const auto requestId = ++recordingRequest;
    const auto sessionId = editorGeneration;
    showEditor(false);
    // Let the compositor remove the frozen overlay before opening the live stream.
    QTimer::singleShot(180, this, [this, region, sys, mic, requestId, sessionId] {
        if (!recordScheduled || requestId != recordingRequest || sessionId != editorGeneration)
            return;
        recordScheduled = false;
        recorder.start(region, settings, sys, mic);
        if (recorder.busy())
            closeEditor();
        else
            showEditor(true);
    });
}
void Application::Stop()
{
    if (recordScheduled) {
        cancelScheduledRecording();
        showEditor(true);
        return;
    }
    recorder.stop();
}
QString Application::Status() const
{
    if (recordScheduled || recorder.state() == Recorder::State::Starting)
        return "starting";
    if (recorder.state() == Recorder::State::Recording)
        return "recording";
    if (recorder.state() == Recorder::State::Finishing)
        return "finishing";
    if (modalOpen)
        return "dialog";
    return capture.busy() ? "capturing" : !overlays.isEmpty() ? "editing" : "idle";
}
void Application::Quit()
{
    if (quitting)
        return;
    quitting = true;
    capture.cancel();
    closeEditor();
    if (recorder.busy())
        recorder.stop();
    else
        qApp->quit();
}
void Application::Demo()
{
    demo();
}
void Application::watchScreen(QScreen* screen)
{
    connect(screen, &QScreen::geometryChanged, this, &Application::screenLayoutChanged);
    connect(screen, &QScreen::logicalDotsPerInchChanged, this, &Application::screenLayoutChanged);
}
void Application::screenLayoutChanged()
{
    const bool wasActive = capture.busy() || !overlays.isEmpty();
    capture.cancel();
    closeEditor();
    if (wasActive)
        announce("显示布局已变化，请重新截图。");
}
void Application::demo()
{
    if (quitting || modalOpen || recorder.busy() || capture.busy() || !overlays.isEmpty())
        return;
    const QSize size = desktopGeometry().size();
    QImage image(size, QImage::Format_RGB32);
    image.fill(QColor("#e8eee7"));
    QPainter p(&image);
    QLinearGradient gradient(0, 0, size.width(), size.height());
    gradient.setColorAt(0, QColor("#d9efe1"));
    gradient.setColorAt(1, QColor("#d6dee9"));
    p.fillRect(image.rect(), gradient);
    p.setPen(QColor("#203a33"));
    QFont font = p.font();
    font.setPixelSize(48);
    font.setBold(true);
    p.setFont(font);
    p.drawText(QPoint(120, 170), "Mokou Screenshot");
    font.setPixelSize(24);
    font.setBold(false);
    p.setFont(font);
    p.drawText(QPoint(120, 230), "框住这一刻，留下有用的部分。");
    p.drawText(QPoint(120, 280), "选择 · 标注 · 拼接 · OCR · 翻译 · 录屏");
    p.fillRect(QRect(120, 340, 520, 240), QColor("#fcfaf2"));
    p.drawText(QRect(148, 360, 464, 190), Qt::TextWordWrap,
        "这是演示画布。拖动框选后可继续调整边框，使用画笔、直线、遮挡和取色。确认会保存并复制。");
    p.end();
    openEditor(image, true);
}

void Application::applyEditorPreferences()
{
    document.textFont = settings.textFont;
    document.textColor = settings.textColor;
    document.pickerZoom = settings.pickerZoom;
    document.textBackgroundOpacity = settings.textBackgroundOpacity;
    emit document.changed();
}
void Application::saveEditorPreferences()
{
    settings.textFont = document.textFont;
    settings.textColor = document.textColor;
    settings.pickerZoom = document.pickerZoom;
    settings.textBackgroundOpacity = document.textBackgroundOpacity;
    QString error;
    if (!settings.save(&error))
        setEditorStatus(error);
}
void Application::cancelScheduledRecording()
{
    recordScheduled = false;
    ++recordingRequest;
}
