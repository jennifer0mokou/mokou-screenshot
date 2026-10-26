#pragma once
#include "audiocapture.h"
#include "settings.h"
#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QRect>
#include <QTemporaryDir>
#include <QTimer>
#include <memory>
class CastManager;
class CastStream;
class PipeWireRecord;
class Recorder : public QObject
{
    Q_OBJECT
public:
    explicit Recorder(QObject* parent = nullptr);
    ~Recorder() override;
    enum class State { Idle, Starting, Recording, Finishing };
    State state() const
    {
        return phase;
    }
    bool busy() const
    {
        return phase != State::Idle;
    }
    bool isRecording() const
    {
        return phase == State::Recording;
    }
    void start(const QRect& globalRegion, const Settings& settings, bool systemAudio, bool microphone);
    void stop();
    static QString holdLastFrameFilter(qint64 milliseconds);
signals:
    void started();
    void finishing();
    void canceled();
    void saved(const QString& path);
    void failed(const QString& reason);

private:
    std::unique_ptr<CastManager> manager;
    CastStream* stream = nullptr;
    std::unique_ptr<PipeWireRecord> video;
    std::unique_ptr<QTemporaryDir> temporary;
    AudioCapture audio;
    QProcess mux;
    QTimer deadline;
    QElapsedTimer recordingTime;
    qint64 durationMilliseconds = 0;
    State phase = State::Idle;
    bool audioWanted = false;
    bool finalizing = false;
    QString output, videoFile, audioFile, muxFile, format;
    void maybeFinalize();
    void error(const QString& message);
    void release();
};
