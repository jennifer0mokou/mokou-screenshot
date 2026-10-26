#include "imageoutput.h"
#include "settings.h"
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDir>
#include <QMimeData>
#include <QSaveFile>
namespace ImageOutput
{
bool saveAndCopy(const QImage& image, const QString& directory, QString* savedPath, QString* error)
{
    if (image.isNull()) {
        *error = "请先框选一个区域。";
        return false;
    }
    if (!QDir().mkpath(directory)) {
        *error = "无法创建截图目录：" + directory;
        return false;
    }
    const auto path = uniqueOutputPath(directory, "png");
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        *error = "PNG 编码失败，请保留编辑界面后重试。";
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(png) != png.size() || !file.commit()) {
        *error = "保存失败：" + file.errorString();
        return false;
    }
    auto mime = new QMimeData;
    mime->setImageData(image);
    mime->setData("image/png", png);
    QApplication::clipboard()->setMimeData(mime);
    *savedPath = path;
    return true;
}
}
