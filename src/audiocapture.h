#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
class AudioCapture : public QObject
{
    Q_OBJECT
public:
    explicit AudioCapture(QObject* parent = nullptr);
    ~AudioCapture() override;
    bool busy() const
    {
        return phase != Phase::Idle;
    }
    void start(const QString& output, bool systemSound, bool microphone);
    void stop();
    void cancel();
signals:
    void started();
    void finished();
    void failed(const QString& reason);

private:
    enum class Phase { Idle, Querying, Starting, Recording, Stopping };
    Phase phase = Phase::Idle;
    QProcess query, encoder;
    QTimer deadline;
    QString output;
    bool systemSound = false, microphone = false;
    void launchEncoder(const QString& sink = { });
    void fail(const QString& reason);
    void finish();
};
