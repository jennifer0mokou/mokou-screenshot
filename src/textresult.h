#pragma once
#include "document.h"
namespace TextResult
{
QList<TextBlock> parseBlocks(const QString& json, QString* error, const QSize& imageSize = { });
QList<TextBlock> parseTranslations(const QString& json, const QList<TextBlock>& source, QString* error);
}
