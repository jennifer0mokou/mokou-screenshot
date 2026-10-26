#pragma once
#include <QFrame>
class QLabel;
class QPlainTextEdit;
class ReadingPanel : public QFrame
{
    Q_OBJECT
public:
    explicit ReadingPanel(QWidget* parent);
    void fitNear(const QRect& toolbar, const QRect& available);
    void present(const QString& text);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPlainTextEdit* content;
};
