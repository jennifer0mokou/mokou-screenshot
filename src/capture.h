#pragma once
#include <QImage>
#include <QObject>
#include <QTimer>
#include <QVariantMap>
class QSocketNotifier;
class Capture : public QObject
{
    Q_OBJECT
public:
    explicit Capture(QObject* parent = nullptr);
    ~Capture() override;
    void start();
    void cancel();
    bool busy() const
    {
        return fd >= 0;
    }
signals:
    void ready(const QImage& image);
    void failed(const QString& reason);

private:
    quint64 generation = 0;
    int fd = -1;
    bool eof = false;
    QByteArray bytes;
    QVariantMap meta;
    QSocketNotifier* notifier = nullptr;
    QTimer timeout;
    void readAvailable();
    void finishIfReady();
    void fail(const QString& reason);
    void cleanup();
};
