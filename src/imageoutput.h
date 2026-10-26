#pragma once
#include <QImage>
#include <QString>
// Saves the exact same PNG bytes that are offered to clipboard consumers.
namespace ImageOutput
{
bool saveAndCopy(const QImage& image, const QString& directory, QString* savedPath, QString* error);
}
