#include "readingpanel.h"
#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextDocument>
#include <QVBoxLayout>
#include <cmath>
ReadingPanel::ReadingPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName("readingPanel");
    setStyleSheet(
        "QFrame#readingPanel{background:rgba(25,37,41,240);border:1px solid #52675d;border-radius:10px;}"
        "QLabel{color:#e5f3eb;background:transparent;border:0;}"
        "QPushButton{background:#344c42;color:#eff9f2;border:0;border-radius:5px;padding:5px 10px;}"
        "QPlainTextEdit{background:transparent;color:#eff9f2;border:0;selection-background-color:#427b60;}"
        "QScrollBar:vertical{background:transparent;width:8px;}QScrollBar::handle:vertical{background:#"
        "617c72;border-radius:4px;min-height:24px;}QScrollBar::add-line:vertical,QScrollBar::sub-line:"
        "vertical{height:0;}QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:"
        "transparent;}");
    auto main = new QVBoxLayout(this);
    main->setContentsMargins(12, 10, 12, 12);
    auto header = new QHBoxLayout;
    auto title = new QLabel("识图");
    header->addWidget(title, 1);
    auto copy = new QPushButton("复制");
    header->addWidget(copy);
    main->addLayout(header);
    content = new QPlainTextEdit;
    content->setObjectName("readingContent");
    content->setReadOnly(true);
    content->setMinimumWidth(0);
    auto font = content->font();
    font.setPixelSize(14);
    content->setFont(font);
    main->addWidget(content, 1);
    connect(copy, &QPushButton::clicked, this,
        [this] { QApplication::clipboard()->setText(content->toPlainText()); });
    hide();
}
void ReadingPanel::present(const QString& text)
{
    if (content->toPlainText() != text)
        content->setPlainText(text);
    show();
}

void ReadingPanel::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        parentWidget()->setFocus();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}

void ReadingPanel::fitNear(const QRect& toolbar, const QRect& available)
{
    const int width = qMin(460, available.width());
    QTextDocument layout;
    layout.setDefaultFont(content->font());
    layout.setDocumentMargin(0);
    layout.setPlainText(content->toPlainText());
    layout.setTextWidth(qMax(1, width - 36));
    const int wanted = qBound(96, int(std::ceil(layout.size().height())) + 62, 340);
    const int below = available.bottom() - toolbar.bottom() - 8;
    const int above = toolbar.top() - available.top() - 8;
    const bool bottom = below >= wanted || below > above;
    const int height = qMax(1, qMin(wanted, bottom ? below : above));
    const int x = qBound(available.left(), toolbar.right() - width + 1, available.right() - width + 1);
    const int y = bottom ? toolbar.bottom() + 9 : toolbar.top() - height - 8;
    setGeometry(x, y, width, height);
}
