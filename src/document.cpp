#include "document.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <algorithm>
Document::Document(QObject* parent)
    : QObject(parent)
{
    textFont.setPixelSize(24);
}
void Document::reset(const QImage& source, const QSizeF& size, const QPoint& origin)
{
    ++revision;
    ++contentRevision;
    composite = { };
    image = source;
    image.setDevicePixelRatio(1);
    logicalSize = size;
    desktopOrigin = origin;
    selection = { };
    marks.clear();
    undone.clear();
    clearResultData();
    tool = Select;
    emit resultsChanged();
    emit selectionChanged();
    emit changed();
}
QRect Document::pixelRect(const QRectF& logical) const
{
    if (image.isNull() || logicalSize.isEmpty())
        return { };
    const auto r = logical.normalized().intersected(QRectF(QPointF(), logicalSize));
    const qreal sx = image.width() / logicalSize.width(), sy = image.height() / logicalSize.height();
    return QRectF(r.x() * sx, r.y() * sy, r.width() * sx, r.height() * sy)
        .toAlignedRect()
        .intersected(image.rect());
}
namespace
{
QBrush pixelatedBrush(const QImage& base, const QSizeF& size)
{
    if (base.isNull() || size.isEmpty())
        return { };
    constexpr qreal cellSize = 12; // Logical pixels; the grid stays fixed across strokes and display scales.
    const auto sample = base.scaled(qMax(1, qCeil(size.width() / cellSize)),
        qMax(1, qCeil(size.height() / cellSize)), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QBrush brush(sample);
    brush.setTransform(QTransform::fromScale(size.width() / sample.width(), size.height() / sample.height()));
    return brush;
}
}
void paintMark(QPainter& p, const Mark& m, const QBrush& mosaic)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    if (!m.clip.isEmpty())
        p.setClipRect(m.clip, Qt::IntersectClip);
    p.setPen(QPen(m.kind == Mark::Mosaic ? mosaic : QBrush(m.color), m.width, Qt::SolidLine, Qt::RoundCap,
        Qt::RoundJoin));
    if (m.kind == Mark::Mosaic)
        p.setRenderHint(QPainter::SmoothPixmapTransform, false);
    const QRectF box = QRectF(m.start, m.end).normalized();
    switch (m.kind) {
    case Mark::Pen:
    case Mark::Mosaic:
        if (m.path.elementCount() <= 1)
            p.drawPoint(m.start);
        else
            p.drawPath(m.path);
        break;
    case Mark::Line:
        p.drawLine(m.start, m.end);
        break;
    case Mark::Redact:
        p.fillRect(box, Qt::black);
        break;
    case Mark::Text: {
        auto f = m.font;
        if (f.pixelSize() <= 0)
            f.setPixelSize(24);
        p.setFont(f);
        p.drawText(m.start, m.text);
        break;
    }
    }
    p.restore();
}
QImage Document::rendered() const
{
    if (image.isNull() || logicalSize.isEmpty())
        return { };
    if (cachedRevision != contentRevision || composite.isNull()) {
        QImage result = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        // A mosaic samples the already composited pixels, including earlier redactions.
        for (const auto& mark : marks) {
            const auto brush = mark.kind == Mark::Mosaic ? pixelatedBrush(result, logicalSize) : QBrush();
            QPainter p(&result);
            p.scale(image.width() / logicalSize.width(), image.height() / logicalSize.height());
            paintMark(p, mark, brush);
        }
        composite = std::move(result);
        cachedRevision = contentRevision;
    }
    return composite;
}
QBrush Document::mosaicBrush() const
{
    return pixelatedBrush(rendered(), logicalSize);
}
QImage Document::crop() const
{
    const auto pixels = pixelRect(selection);
    if (selection.isEmpty() || pixels.isEmpty())
        return { };
    return rendered().copy(pixels);
}
void Document::invalidateContent()
{
    ++revision;
    ++contentRevision;
    clearResultData();
    emit resultsChanged();
    emit changed();
}
void Document::addMark(const Mark& m)
{
    marks.append(m);
    undone.clear();
    invalidateContent();
}
void Document::undo()
{
    if (!marks.isEmpty()) {
        undone.append(marks.takeLast());
        invalidateContent();
    }
}
void Document::redo()
{
    if (!undone.isEmpty()) {
        marks.append(undone.takeLast());
        invalidateContent();
    }
}
void Document::clearResultData()
{
    blocks.clear();
    descriptionText.clear();
    textRegion = { };
    showTranslation = false;
}
bool Document::hasTranslation() const
{
    return std::any_of(
        blocks.begin(), blocks.end(), [](const TextBlock& block) { return !block.translation.isEmpty(); });
}
void Document::setTextBlocks(const QList<TextBlock>& value, bool translated)
{
    blocks = value;
    textRegion = blocks.isEmpty() ? QRectF() : selection;
    showTranslation = translated && hasTranslation();
    tool = blocks.isEmpty() ? Select : ReadText;
    emit resultsChanged();
    emit changed();
}
void Document::setDescription(const QString& value)
{
    descriptionText = value;
    emit resultsChanged();
    emit changed();
}
void Document::clearDescription()
{
    if (descriptionText.isEmpty())
        return;
    setDescription({ });
}
void Document::clearTranslations()
{
    if (!hasTranslation())
        return;
    for (auto& block : blocks)
        block.translation.clear();
    showTranslation = false;
    emit resultsChanged();
    emit changed();
}
void Document::setTranslationVisible(bool translated)
{
    if (translated && !hasTranslation())
        return;
    showTranslation = translated;
    tool = ReadText;
    emit resultsChanged();
    emit changed();
}
QString Document::readableText() const
{
    QStringList lines;
    for (const auto& block : blocks)
        lines << (showTranslation && !block.translation.isEmpty() ? block.translation : block.source);
    return lines.join('\n');
}
void Document::setSelection(const QRectF& r)
{
    const auto bounded = r.normalized().intersected(QRectF(QPointF(), logicalSize));
    if (bounded == selection)
        return;
    ++revision;
    selection = bounded;
    clearResultData();
    emit resultsChanged();
    emit selectionChanged();
    emit changed();
}
void Document::setTool(Tool value)
{
    if (tool == value)
        return;
    tool = value;
    emit changed();
}
QImage Document::stitch(const QList<QImage>& images, bool horizontal, int gap, const QSize& maximumSize)
{
    if (images.isEmpty())
        return { };
    qint64 w = 0, h = 0;
    for (const auto& i : images) {
        if (i.isNull())
            return { };
        if (horizontal) {
            w += i.width();
            h = qMax(h, qint64(i.height()));
        } else {
            h += i.height();
            w = qMax(w, qint64(i.width()));
        }
    }
    if (horizontal)
        w += qint64(qMax(0, gap)) * (images.size() - 1);
    else
        h += qint64(qMax(0, gap)) * (images.size() - 1);
    if (w < 1 || h < 1 || w > 32768 || h > 32768 || w * h > 100000000)
        return { };
    const QSize fullSize { int(w), int(h) };
    auto outputSize = maximumSize.isEmpty() ? fullSize : fullSize.scaled(maximumSize, Qt::KeepAspectRatio);
    outputSize.setWidth(qMax(1, outputSize.width()));
    outputSize.setHeight(qMax(1, outputSize.height()));
    QImage result(outputSize, QImage::Format_ARGB32_Premultiplied);
    if (result.isNull())
        return { };
    result.fill(Qt::white);
    QPainter p(&result);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.scale(qreal(outputSize.width()) / w, qreal(outputSize.height()) / h);
    QPoint pos;
    for (const auto& i : images) {
        p.drawImage(QRectF(pos, i.size()), i, i.rect());
        pos += horizontal ? QPoint(i.width() + qMax(0, gap), 0) : QPoint(0, i.height() + qMax(0, gap));
    }
    return result;
}
