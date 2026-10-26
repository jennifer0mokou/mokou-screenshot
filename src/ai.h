#pragma once
#include "document.h"
#include "settings.h"
#include <QJsonValue>
#include <QNetworkAccessManager>
#include <QPointer>
class QNetworkReply;
class AiClient : public QObject
{
    Q_OBJECT
public:
    enum Task { Ocr, Translate, Describe };
    explicit AiClient(QObject* parent = nullptr);
    ~AiClient() override;
    void request(Task task, const QImage& image, const Settings& settings);
    void translate(const QList<TextBlock>& blocks, const Settings& settings);
    void cancel();
    bool busy() const
    {
        return !reply.isNull();
    }
    static QUrl endpoint(const QString& base);
signals:
    void textReady(AiClient::Task task, const QList<TextBlock>& blocks);
    void descriptionReady(const QString& description);
    void failed(const QString& reason);
    void busyChanged(bool busy);

private:
    void send(Task task, const QJsonValue& content, const Settings& settings,
        const QList<TextBlock>& source = { }, const QSize& imageSize = { });
    QNetworkAccessManager network;
    QPointer<QNetworkReply> reply;
};
