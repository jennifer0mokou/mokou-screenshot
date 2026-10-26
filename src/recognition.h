#pragma once
#include "ai.h"
#include "document.h"
#include "ocr.h"
#include "settings.h"
class RecognitionController : public QObject
{
    Q_OBJECT
public:
    RecognitionController(Document& document, Settings& settings, QObject* parent = nullptr);
    ~RecognitionController() override;
    bool busy() const
    {
        return ai.busy() || ocr.busy();
    }
    bool configured(AiClient::Task task) const;
    void request(AiClient::Task task);
    void cancel();
    void settingsChanged(const Settings& before);
signals:
    void busyChanged(bool busy);
    void statusChanged(const QString& status);

private:
    Document& document;
    Settings& settings;
    AiClient ai;
    OcrClient ocr;
    quint64 revision = 0;
    bool translateAfterOcr = false;
    void receiveOcr(const QList<TextBlock>& blocks, qint64 milliseconds = 0);
    void translate();
};
