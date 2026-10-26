#pragma once
#include "document.h"
#include <QWidget>
#include <memory>

// One reading surface gives spatially separate OCR lines one continuous text selection.
class TextOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit TextOverlay(QWidget* parent);
    ~TextOverlay() override;
    void setResults(const QList<TextBlock>& blocks, const QRectF& region, bool translated,
        const QRect& available, int opacity);
    QString plainText() const;
    QString selectedText() const;
    QRectF blockBounds(int index) const;
    bool truncated(int index) const;
    int pixelSize(int index) const;
    void selectAll();
    void setReadingVisible(bool visible);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    bool event(QEvent*) override;

private:
    struct State;
    std::unique_ptr<State> state;
    int positionAt(const QPointF& point) const;
    void expand(int index);
    void updateMask();
};
