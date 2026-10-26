#include "settings.h"
#include <QDateTime>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QUuid>
Settings::Settings()
{
    QSettings s;
    const auto pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const auto movies = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
    imageDirectory = s.value("storage/images", pictures + "/屏幕截图").toString();
    videoDirectory = s.value("storage/videos", movies + "/屏幕录制").toString();
    api.baseUrl = s.value("api/baseUrl").toString();
    api.model = s.value("api/model").toString();
    api.systemPrompt = s.value("api/systemPrompt", api.systemPrompt).toString();
    api.ocrPrompt = s.value("api/ocrPrompt", api.ocrPrompt).toString();
    api.translationPrompt = s.value("api/translationPrompt", api.translationPrompt).toString();
    api.describePrompt = s.value("api/describePrompt", api.describePrompt).toString();
    const auto savedFont = s.value("editor/textFont").toString();
    if (!savedFont.isEmpty())
        textFont.fromString(savedFont);
    textFont.setPixelSize(qBound(8, s.value("editor/textSize", 24).toInt(), 160));
    const QColor savedColor(s.value("editor/textColor", textColor.name()).toString());
    if (savedColor.isValid())
        textColor = savedColor;
    pickerZoom = qBound(4, s.value("editor/pickerZoom", 12).toInt(), 24);
    textBackgroundOpacity = qBound(80, s.value("editor/textBackgroundOpacity", 220).toInt(), 255);
    rapidLanguage = s.value("ocr/language", rapidLanguage).toString();
    if (!QStringList { "multi", "french", "korean", "russian" }.contains(rapidLanguage))
        rapidLanguage = "multi";
    api.translationModel = s.value("api/translationModel").toString();
    localOcr = s.value("ocr/local", true).toBool();
    api.key = qEnvironmentVariable("MUSHROOM_API_KEY");
    targetLanguage = s.value("api/targetLanguage", targetLanguage).toString();
    videoFormat = s.value("recording/format", videoFormat).toString();
    frameRate = qBound(5, s.value("recording/fps", frameRate).toInt(), 60);
}
bool Settings::save(QString* error) const
{
    QSettings s;
    s.setValue("storage/images", imageDirectory);
    s.setValue("storage/videos", videoDirectory);
    s.setValue("api/baseUrl", api.baseUrl);
    s.setValue("api/model", api.model);
    s.setValue("api/systemPrompt", api.systemPrompt);
    s.setValue("api/ocrPrompt", api.ocrPrompt);
    s.setValue("api/translationPrompt", api.translationPrompt);
    s.setValue("api/describePrompt", api.describePrompt);
    s.setValue("editor/textFont", textFont.toString());
    s.setValue("editor/textSize", textFont.pixelSize());
    s.setValue("editor/textColor", textColor.name());
    s.setValue("editor/pickerZoom", pickerZoom);
    s.setValue("editor/textBackgroundOpacity", textBackgroundOpacity);
    s.setValue("ocr/language", rapidLanguage);
    s.setValue("api/translationModel", api.translationModel);
    s.setValue("ocr/local", localOcr);
    s.setValue("api/targetLanguage", targetLanguage);
    s.setValue("recording/format", videoFormat);
    s.setValue("recording/fps", frameRate);
    s.sync();
    if (s.status() != QSettings::NoError) {
        *error = "无法保存设置，请检查配置目录是否可写。";
        return false;
    }
    error->clear();
    return true;
}
QString uniqueOutputPath(const QString& directory, const QString& extension)
{
    return QDir(directory).filePath("蘑菇_" + QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz")
        + "_" + QUuid::createUuid().toString(QUuid::Id128).left(6) + "." + extension);
}
