#pragma once
#include <QLineEdit>
class AnnotationInput : public QLineEdit
{
    Q_OBJECT
public:
    explicit AnnotationInput(QWidget* parent);
    void configure(const QFont& font, const QColor& color, int opacity);
    void place(const QPoint& anchor, const QRect& bounds);
    QPointF baseline() const;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;

private:
    QPoint anchor;
    QRect bounds;
    QString composition;
    void fit();
};
