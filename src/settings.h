#pragma once
#include <QColor>
#include <QFont>
#include <QString>
struct ApiProfile {
    QString baseUrl;
    QString model;
    QString key; // Memory only. Never serialized.
    QString translationModel;
    QString systemPrompt;
    QString ocrPrompt = QStringLiteral("准确识别所有可读文字，按阅读顺序逐行返回；不推测模糊文字。");
    QString translationPrompt = QStringLiteral("准确、自然地翻译，结合相邻行理解上下文，保留专有名词。");
    QString describePrompt = QStringLiteral("描述截图的主要信息，简洁明确。");
};
struct Settings {
    QString imageDirectory;
    QString videoDirectory;
    QString targetLanguage = QStringLiteral("简体中文");
    QString videoFormat = QStringLiteral("mp4");
    int frameRate = 30;
    ApiProfile api;
    bool localOcr = true;
    QString rapidLanguage = QStringLiteral("multi");
    QFont textFont;
    QColor textColor = QColor("#ff647c");
    int pickerZoom = 12;
    int textBackgroundOpacity = 220;
    Settings();
    bool save(QString* error) const;
};
QString uniqueOutputPath(const QString& directory, const QString& extension);
