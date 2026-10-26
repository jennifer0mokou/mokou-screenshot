#pragma once
#include <QBrush>
#include <QColor>
#include <QFont>
#include <QImage>
#include <QObject>
#include <QPainterPath>
#include <QRectF>
#include <limits>
struct Mark {
    enum Kind { Pen, Line, Mosaic, Redact, Text } kind = Pen;
    QPointF start, end;
    QPainterPath path;
    QRectF clip;
    QColor color = QColor("#ff647c");
    qreal width = 3;
    QString text;
    QFont font;
};
struct TextBlock {
    QRectF box; // 0..1 relative to the OCR input image
    QString source;
    QString translation;
    bool operator==(const TextBlock&) const = default;
};
class Document : public QObject
{
    Q_OBJECT
public:
    explicit Document(QObject* parent = nullptr);
    quint64 revision = 0;
    QSizeF logicalSize;
    QPoint desktopOrigin;
    QRectF selection;
    enum Tool { Select, Pen, Line, Mosaic, Redact, Text, Picker, ReadText };
    Tool tool = Select;
    QColor color = QColor("#ff647c");
    qreal penWidth = 3;
    qreal mosaicWidth = 32;
    QFont textFont;
    QColor textColor = QColor("#ff647c");
    int pickerZoom = 12;
    int textBackgroundOpacity = 220;
    void reset(const QImage& source, const QSizeF& size, const QPoint& origin = { });
    QRect pixelRect(const QRectF& logical) const;
    QImage rendered() const;
    QBrush mosaicBrush() const;
    qsizetype markCount() const
    {
        return marks.size();
    }
    bool canRedo() const
    {
        return !undone.isEmpty();
    }
    QImage crop() const;
    void addMark(const Mark& mark);
    void undo();
    void redo();
    const QList<TextBlock>& textBlocks() const
    {
        return blocks;
    }
    QRectF recognitionRegion() const
    {
        return textRegion;
    }
    const QString& description() const
    {
        return descriptionText;
    }
    bool translationVisible() const
    {
        return showTranslation;
    }
    QString readableText() const;
    void setTextBlocks(const QList<TextBlock>& value, bool translated = false);
    void setDescription(const QString& value);
    void setTranslationVisible(bool translated);
    void clearTranslations();
    void clearDescription();
    void clearText();
    void setSelection(const QRectF& rect);
    void setTool(Tool value);
    static QImage stitch(
        const QList<QImage>& images, bool horizontal, int gap = 0, const QSize& maximumSize = { });

private:
    QList<TextBlock> blocks;
    QRectF textRegion;
    QString descriptionText;
    bool showTranslation = false;
    void clearResultData();
    QImage image;
    QList<Mark> marks, undone;
    quint64 contentRevision = 0;
    mutable quint64 cachedRevision = std::numeric_limits<quint64>::max();
    mutable QImage composite;
signals:
    void changed();
    void resultsChanged();
    void selectionChanged();
};
void paintMark(QPainter& painter, const Mark& mark, const QBrush& mosaic = { });
