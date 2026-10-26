#include "recognition.h"
RecognitionController::RecognitionController(Document& doc, Settings& preferences, QObject* parent)
    : QObject(parent)
    , document(doc)
    , settings(preferences)
{
    const auto failed = [this](const QString& message) {
        translateAfterOcr = false;
        emit statusChanged(message);
    };
    connect(&ai, &AiClient::failed, this, failed);
    connect(&ocr, &OcrClient::failed, this, failed);
    const auto updateBusy = [this] { emit busyChanged(busy()); };
    connect(&ai, &AiClient::busyChanged, this, updateBusy);
    connect(&ocr, &OcrClient::busyChanged, this, updateBusy);
    connect(&document, &Document::changed, this, [this] {
        if (busy() && document.revision != revision)
            cancel();
    });
    connect(&ocr, &OcrClient::textReady, this, &RecognitionController::receiveOcr);
    connect(&ai, &AiClient::textReady, this, [this](AiClient::Task task, const QList<TextBlock>& blocks) {
        if (document.revision != revision)
            return;
        if (task == AiClient::Ocr) {
            receiveOcr(blocks);
            return;
        }
        document.setTextBlocks(blocks, true);
        emit statusChanged("译文已显示 · 可跨行拖选复制 · … 就地展开长译文");
    });
    connect(&ai, &AiClient::descriptionReady, this, [this](const QString& text) {
        if (document.revision != revision)
            return;
        document.setDescription(text);
        emit statusChanged("识图完成；结果可在当前截图内阅读、选择或复制。");
    });
}
RecognitionController::~RecognitionController()
{
    cancel();
}
bool RecognitionController::configured(AiClient::Task task) const
{
    const bool api = !AiClient::endpoint(settings.api.baseUrl).isEmpty();
    const bool vision = api && !settings.api.model.trimmed().isEmpty();
    if (task == AiClient::Describe)
        return vision;
    if (task == AiClient::Ocr)
        return settings.localOcr || vision;
    const bool translator = api
        && (!settings.api.translationModel.trimmed().isEmpty() || !settings.api.model.trimmed().isEmpty());
    return translator && (!document.textBlocks().isEmpty() || settings.localOcr || vision);
}
void RecognitionController::request(AiClient::Task task)
{
    if (busy())
        return;
    if (document.selection.isEmpty()) {
        emit statusChanged("请先框选一个区域。");
        return;
    }
    if (!configured(task)) {
        emit statusChanged("请先在设置中填写当前任务所需的模型与 API 地址。");
        return;
    }
    revision = document.revision;
    if (task == AiClient::Describe) {
        emit statusChanged("正在识图…");
        ai.request(task, document.crop(), settings);
    } else if (task == AiClient::Translate && !document.textBlocks().isEmpty()) {
        translate();
    } else {
        translateAfterOcr = task == AiClient::Translate;
        emit statusChanged("正在识别文字…");
        if (settings.localOcr)
            ocr.request(document.crop(), settings.rapidLanguage);
        else
            ai.request(AiClient::Ocr, document.crop(), settings);
    }
}
void RecognitionController::cancel()
{
    translateAfterOcr = false;
    ai.cancel();
    ocr.cancel();
}
void RecognitionController::receiveOcr(const QList<TextBlock>& blocks, qint64 milliseconds)
{
    if (document.revision != revision)
        return;
    document.setTextBlocks(blocks);
    if (blocks.isEmpty()) {
        translateAfterOcr = false;
        emit statusChanged("没有识别到文字。");
        return;
    }
    if (translateAfterOcr) {
        translateAfterOcr = false;
        translate();
    } else {
        emit statusChanged(milliseconds > 0
                ? QString("本地 OCR %1 ms · 跨行拖选后 Ctrl+C 复制").arg(milliseconds)
                : "OCR 完成 · 跨行拖选后 Ctrl+C 复制");
    }
}
void RecognitionController::translate()
{
    revision = document.revision;
    emit statusChanged("正在翻译 OCR 原文（只发送文字）…");
    ai.translate(document.textBlocks(), settings);
}
void RecognitionController::settingsChanged(const Settings& before)
{
    cancel();
    const auto& old = before.api;
    const auto& now = settings.api;
    const bool ocrChanged = before.localOcr != settings.localOcr
        || before.rapidLanguage != settings.rapidLanguage
        || (!settings.localOcr
            && (old.baseUrl != now.baseUrl || old.model != now.model || old.ocrPrompt != now.ocrPrompt
                || old.systemPrompt != now.systemPrompt));
    if (ocrChanged) {
        document.clearText();
        document.setTool(Document::Select);
        return;
    }
    if (before.targetLanguage != settings.targetLanguage || old.translationModel != now.translationModel
        || old.baseUrl != now.baseUrl || old.model != now.model || old.systemPrompt != now.systemPrompt
        || old.translationPrompt != now.translationPrompt)
        document.clearTranslations();
    if (before.targetLanguage != settings.targetLanguage || old.baseUrl != now.baseUrl
        || old.model != now.model || old.systemPrompt != now.systemPrompt
        || old.describePrompt != now.describePrompt)
        document.clearDescription();
}
