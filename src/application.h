#pragma once
#include "capture.h"
#include "document.h"
#include "recognition.h"
#include "recorder.h"
#include "settings.h"
#include <QElapsedTimer>
#include <QPointer>
#include <QSystemTrayIcon>
#include <memory>
class QAction;
class QMenu;
class Overlay;
class QScreen;
class Application : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.github.fedokou.MushroomShot")
public:
    explicit Application(QObject* parent = nullptr);
    ~Application() override;
    void demo();
public slots:
    Q_SCRIPTABLE void Capture();
    Q_SCRIPTABLE void ShowSettings();
    Q_SCRIPTABLE void Stop();
    Q_SCRIPTABLE void Quit();
    Q_SCRIPTABLE void Demo();
    Q_SCRIPTABLE QString Status() const;

private:
    Settings settings;
    Document document;
    class Capture capture;
    RecognitionController recognition;
    Recorder recorder;
    QList<QPointer<Overlay>> overlays;
    QAction *captureAction, *stopAction;
    std::unique_ptr<QMenu> trayMenu;
    QSystemTrayIcon tray;
    QTimer recordingClock;
    QElapsedTimer elapsed;
    quint64 editorGeneration = 0, recordingRequest = 0;
    bool recordScheduled = false;
    QRect captureGeometry;
    bool quitting = false, modalOpen = false;
    void applyEditorPreferences();
    void saveEditorPreferences();
    void openEditor(const QImage& image, bool demo = false);
    void closeEditor();
    void showEditor(bool show);
    bool saveImage(const QImage& image);
    void runRecognition(AiClient::Task task);
    void cancelScheduledRecording();
    void collage();
    void record(bool systemAudio, bool microphone);
    void announce(const QString& message, bool error = false);
    void setEditorStatus(const QString& text);
    void watchScreen(QScreen* screen);
    void screenLayoutChanged();
};
