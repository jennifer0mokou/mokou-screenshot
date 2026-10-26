#include "annotationinput.h"
#include <QFontMetricsF>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <cmath>
AnnotationInput::AnnotationInput(QWidget* parent)
    : QLineEdit(parent)
{
    setObjectName("annotationInput");
    setFrame(false);
    setTextMargins(0, 0, 0, 0);
    setPlaceholderText("文字");
    setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    connect(this, &QLineEdit::textChanged, this, &AnnotationInput::fit);
}
void AnnotationInput::configure(const QFont& font, const QColor& color, int opacity)
{
    setFont(font);
    setStyleSheet(QString("QLineEdit{background:rgba(241,248,244,%1);color:%2;border:0;padding:0;margin:0;"
                          "selection-background-color:#83cdb0;}")
            .arg(opacity)
            .arg(color.name()));
    fit();
}
void AnnotationInput::place(const QPoint& at, const QRect& area)
{
    anchor = at;
    bounds = area;
    fit();
}
void AnnotationInput::fit()
{
    if (bounds.isEmpty())
        return;
    const QFontMetricsF metrics(font());
    const auto visible = text() + composition;
    const int width = qMin(bounds.width(),
        qMax(12,
            int(std::ceil(metrics.horizontalAdvance(visible.isEmpty() ? placeholderText() : visible))) + 6));
    const int height = qMin(bounds.height(), int(std::ceil(metrics.height())) + 4);
    setGeometry(qBound(bounds.left(), anchor.x(), bounds.right() - width + 1),
        qBound(bounds.top(), anchor.y(), bounds.bottom() - height + 1), width, height);
}
QPointF AnnotationInput::baseline() const
{
    const QFontMetricsF metrics(font());
    return QPointF(x() + 2, y() + (height() - metrics.height()) / 2 + metrics.ascent());
}
void AnnotationInput::keyPressEvent(QKeyEvent* event)
{
    QLineEdit::keyPressEvent(event);
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
        event->accept();
}
void AnnotationInput::inputMethodEvent(QInputMethodEvent* event)
{
    composition = event->preeditString();
    QLineEdit::inputMethodEvent(event);
    fit();
}
