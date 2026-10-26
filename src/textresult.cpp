#include "textresult.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <cmath>
namespace
{
QJsonDocument jsonDocument(QString input, QJsonParseError* error)
{
    input = input.trimmed();
    if (input.startsWith("```")) {
        const auto newline = input.indexOf('\n'), closing = input.lastIndexOf("```");
        if (newline >= 0 && closing > newline)
            input = input.mid(newline + 1, closing - newline - 1).trimmed();
    }
    return QJsonDocument::fromJson(input.toUtf8(), error);
}
QRectF position(const QJsonObject& block, const QJsonObject& root, const QSize& imageSize)
{
    auto array = block.value("bbox").toArray();
    const auto object = block.value("bbox").toObject();
    if (!object.isEmpty())
        array = { object.value("x"), object.value("y"), object.value("width"), object.value("height") };
    if (array.size() != 4)
        return { };
    for (const auto& value : array)
        if (!value.isDouble() || !std::isfinite(value.toDouble()))
            return { };
    const auto units
        = block.value("coordinate_system").toString(root.value("coordinate_system").toString("normalized"));
    const auto format = block.value("box_format").toString(root.value("box_format").toString("xywh"));
    double sx = 1, sy = 1;
    if (units == "pixel" || units == "pixels") {
        if (imageSize.isEmpty())
            return { };
        sx = imageSize.width();
        sy = imageSize.height();
    } else if (units == "1000")
        sx = sy = 1000;
    else if (units == "percent")
        sx = sy = 100;
    else if (units != "normalized")
        return { };
    double x = array[0].toDouble() / sx, y = array[1].toDouble() / sy;
    double right = array[2].toDouble() / sx, bottom = array[3].toDouble() / sy;
    if (format == "xywh") {
        right += x;
        bottom += y;
    } else if (format != "xyxy")
        return { };
    // Correct rounding at the edge, but never invent units for an ambiguous model response.
    const double ex = imageSize.width() > 0 ? qBound(.002, 2.0 / imageSize.width(), .01) : .002;
    const double ey = imageSize.height() > 0 ? qBound(.002, 2.0 / imageSize.height(), .01) : .002;
    if (right <= x || bottom <= y || x < -ex || y < -ey || right > 1 + ex || bottom > 1 + ey)
        return { };
    return QRectF(QPointF(qBound(0., x, 1.), qBound(0., y, 1.)),
        QPointF(qBound(0., right, 1.), qBound(0., bottom, 1.)));
}
}
QList<TextBlock> TextResult::parseBlocks(const QString& input, QString* error, const QSize& imageSize)
{
    error->clear();
    QJsonParseError parse;
    const auto document = jsonDocument(input, &parse);
    if (parse.error != QJsonParseError::NoError || !document.isObject()
        || !document.object().value("blocks").isArray()) {
        *error = "OCR 未返回有效的文字 JSON，请检查视觉模型的输出格式。";
        return { };
    }
    const auto root = document.object();
    const auto values = root.value("blocks").toArray();
    if (values.size() > 2000) {
        *error = "文字块过多（上限 2000）。";
        return { };
    }
    QList<TextBlock> blocks;
    for (const auto& value : values) {
        const auto block = value.toObject();
        if (!block.value("text").isString() || block.value("text").toString().size() > 100000) {
            *error = "OCR 文字块缺少有效文字或内容过长。";
            return { };
        }
        const auto text = block.value("text").toString();
        if (text.trimmed().isEmpty())
            continue;
        // An empty box means readable text without reliable placement. Keep it for copying/translation.
        blocks.append({ position(block, root, imageSize), text, { } });
    }
    return blocks;
}
QList<TextBlock> TextResult::parseTranslations(
    const QString& input, const QList<TextBlock>& source, QString* error)
{
    error->clear();
    QJsonParseError parse;
    const auto document = jsonDocument(input, &parse);
    const auto value = document.object().value("translations");
    if (parse.error != QJsonParseError::NoError || !value.isArray()
        || value.toArray().size() != source.size()) {
        *error = "译文格式或数量不完整，保留原文；请重试或更换翻译模型。";
        return { };
    }
    auto result = source;
    QSet<int> seen;
    for (const auto& entry : value.toArray()) {
        const auto object = entry.toObject();
        const auto id = object.value("id");
        const auto text = object.value("text");
        const int index = id.toInt(-1);
        if (!id.isDouble() || id.toDouble() != index || index < 0 || index >= source.size()
            || seen.contains(index) || !text.isString() || text.toString().trimmed().isEmpty()
            || text.toString().size() > 100000) {
            *error = "译文标识或内容无效，保留原文；请重试或更换翻译模型。";
            return { };
        }
        seen.insert(index);
        result[index].translation = text.toString();
    }
    return result;
}
