#pragma once
#include "document.h"
#include <QElapsedTimer>
#include <QProcess>
#include <QTimer>
class OcrClient : public QObject
{
    Q_OBJECT
public:
    explicit OcrClient(QObject* parent = nullptr, const QString& runtime = { });
    ~OcrClient() override;
    bool busy() const
    {
        return active;
    }
    void request(const QImage& image, const QString& language = "multi");
    void cancel();
signals:
    void textReady(const QList<TextBlock>& blocks, qint64 milliseconds);
    void failed(const QString& message);
    void busyChanged(bool busy);

private:
    QString root;
    QProcess worker;
    QTimer deadline, idle;
    QElapsedTimer clock;
    QByteArray pending, incoming;
    bool active = false;
    void fail(const QString& message);
    void shutdown();
};
