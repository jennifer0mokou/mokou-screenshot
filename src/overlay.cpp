#include "overlay.h"
#include "annotationinput.h"
#include "editortoolbar.h"
#include "readingpanel.h"
#include "textoverlay.h"
#include <LayerShellQt/Window>
#include <QApplication>
#include <QClipboard>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QWindow>
#include <algorithm>
#include <functional>
namespace
{
QList<QPointF> handles(const QRectF& r)
{
    return { r.topLeft(), { r.center().x(), r.top() }, r.topRight(), { r.right(), r.center().y() },
        r.bottomRight(), { r.center().x(), r.bottom() }, r.bottomLeft(), { r.left(), r.center().y() } };
}
}
Overlay::Overlay(Document* d, QScreen* screen, bool demo)
    : QWidget(nullptr, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , doc(d)
{
    setWindowTitle("Mokou Screenshot · 框选与编辑");
    setObjectName("captureOverlay");
    setMouseTracking(true);
    setCursor(Qt::ArrowCursor);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_DeleteOnClose);
    offset = screen->geometry().topLeft() - doc->desktopOrigin;
    resize(screen->size());
    winId();
    windowHandle()->setScreen(screen);
    if (!demo && QGuiApplication::platformName().startsWith("wayland")) {
        auto layer = LayerShellQt::Window::get(windowHandle());
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)
            | LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft
            | LayerShellQt::Window::AnchorRight);
        layer->setExclusiveZone(-1);
        layer->setScope("mokou-screenshot-capture");
        layer->setScreen(screen);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityExclusive);
    }
    hint = new QLabel("Mokou Screenshot  /  拖动框选 · 选完可继续调整 · Enter 保存并复制 · Esc 取消", this);
    hint->setStyleSheet("background:#1b252d;color:#dfe9e6;padding:12px 20px;border-radius:12px;");
    hint->adjustSize();
    hint->move(qMax(8, (width() - hint->width()) / 2), 28);
    buildToolbar();
    reading = new ReadingPanel(this);
    textLayer = new TextOverlay(this);
    connect(doc, &Document::resultsChanged, this, &Overlay::syncResults);
    connect(doc, &Document::changed, this, [this] {
        if (readingOpacity != doc->textBackgroundOpacity)
            syncResults();
        textLayer->setReadingVisible(doc->tool == Document::ReadText && !doc->textBlocks().isEmpty());
        if (textInput)
            textInput->configure(doc->textFont, doc->textColor, doc->textBackgroundOpacity);
        layoutToolbar();
        updateCursor();
        update();
    });
    connect(doc, &Document::selectionChanged, this, &Overlay::layoutToolbar);
    auto shortcut = [this](QKeySequence key, std::function<void()> fn) {
        auto s = new QShortcut(key, this);
        connect(s, &QShortcut::activated, this, fn);
    };
    shortcut(QKeySequence::Undo, [d] { d->undo(); });
    shortcut(QKeySequence::Redo, [d] { d->redo(); });
    const QList<QPair<int, Document::Tool>> keys { { Qt::Key_S, Document::Select },
        { Qt::Key_B, Document::Pen }, { Qt::Key_L, Document::Line }, { Qt::Key_M, Document::Mosaic },
        { Qt::Key_D, Document::Redact }, { Qt::Key_T, Document::Text }, { Qt::Key_C, Document::Picker } };
    for (const auto& k : keys)
        shortcut(QKeySequence(k.first), [this, k] { chooseTool(k.second); });
    syncResults();
}
void Overlay::buildToolbar()
{
    toolbarViewport = new QScrollArea(this);
    toolbarViewport->setObjectName("toolbarViewport");
    toolbarViewport->setCursor(Qt::ArrowCursor);
    toolbarViewport->setFrameShape(QFrame::NoFrame);
    toolbarViewport->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    toolbarViewport->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    toolbarViewport->viewport()->setAutoFillBackground(false);
    toolbarViewport->setStyleSheet(
        "QScrollArea{background:transparent;border:0;}"
        "QScrollBar:horizontal{background:#1b252d;height:6px;border:0;}"
        "QScrollBar::handle:horizontal{background:#617c72;min-width:24px;border-radius:3px;}"
        "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0;}"
        "QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{background:transparent;}");
    bar = new EditorToolbar(doc, this);
    toolbarViewport->setWidget(bar);
    bar->setAutoFillBackground(false);
    connect(bar, &EditorToolbar::toolRequested, this, &Overlay::chooseTool);
    connect(bar, &EditorToolbar::ocrRequested, this, [this] {
        finishText();
        emit ocrRequested();
    });
    connect(bar, &EditorToolbar::translateRequested, this, [this] {
        finishText();
        emit translateRequested();
    });
    connect(bar, &EditorToolbar::describeRequested, this, [this] {
        finishText();
        emit describeRequested();
    });
    connect(bar, &EditorToolbar::collageRequested, this, [this] {
        finishText();
        emit collageRequested();
    });
    connect(bar, &EditorToolbar::settingsRequested, this, [this] {
        finishText();
        emit settingsRequested();
    });
    connect(bar, &EditorToolbar::recordRequested, this, [this](bool system, bool microphone) {
        finishText();
        emit recordRequested(system, microphone);
    });
    connect(bar, &EditorToolbar::copyTextRequested, this, [this] {
        const auto selected = textLayer->selectedText();
        QApplication::clipboard()->setText(selected.isEmpty() ? doc->readableText() : selected);
        setStatus(selected.isEmpty() ? "已复制全部文字" : "已复制所选文字");
    });
    connect(bar, &EditorToolbar::accepted, this, &Overlay::acceptCapture);
    connect(bar, &EditorToolbar::canceled, this, &Overlay::canceled);
    connect(bar, &EditorToolbar::cancelRecognitionRequested, this, &Overlay::cancelRecognitionRequested);
    connect(bar, &EditorToolbar::preferencesChanged, this, &Overlay::preferencesChanged);
    connect(bar, &EditorToolbar::layoutChanged, this, &Overlay::layoutToolbar);
    toolbarViewport->hide();
}
void Overlay::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    if (hint) {
        hint->adjustSize();
        hint->move(qMax(8, (width() - hint->width()) / 2), 28);
    }
    if (bar)
        layoutToolbar();
    if (textLayer)
        syncResults();
}
void Overlay::layoutToolbar()
{
    const QRectF local = doc->selection.translated(-offset);
    QScreen* owner = nullptr;
    qreal largestArea = -1;
    for (auto screen : QGuiApplication::screens()) {
        const auto overlap
            = doc->selection.intersected(QRectF(screen->geometry().translated(-doc->desktopOrigin)));
        const qreal area = overlap.width() * overlap.height();
        if (area > largestArea) {
            largestArea = area;
            owner = screen;
        }
    }
    const bool visible
        = !doc->selection.isEmpty() && owner == windowHandle()->screen() && !dragging && !drawing;
    toolbarViewport->setVisible(visible);
    hint->setVisible(!visible);
    if (!visible) {
        bar->closeOptions();
        if (reading)
            reading->hide();
        return;
    }
    bar->adjustSize();
    const int available = qMax(1, width() - 16);
    const int scrollHeight
        = bar->width() > available ? toolbarViewport->horizontalScrollBar()->sizeHint().height() : 0;
    toolbarViewport->resize(qMin(available, bar->width()), bar->height() + scrollHeight);
    const auto visiblePart = local.intersected(QRectF(rect()));
    const int x = qBound(8, qRound(visiblePart.right()) - toolbarViewport->width(),
        qMax(8, width() - toolbarViewport->width() - 8));
    int y = qRound(visiblePart.bottom()) + 16;
    if (y + toolbarViewport->height() > height() - 8)
        y = qRound(visiblePart.top()) - toolbarViewport->height() - 16;
    if (y < 8)
        y = height() - toolbarViewport->height() - 12;
    toolbarViewport->move(x, qMax(8, y));
    toolbarViewport->raise();
    layoutReadingPanel();
    bar->positionOptions();
}
void Overlay::setStatus(const QString& s)
{
    bar->setStatus(s);
}
void Overlay::setRecognitionBusy(bool busy)
{
    recognitionBusy = busy;
    bar->setBusy(busy);
}
QPointF Overlay::docPoint(const QPointF& p) const
{
    auto value = p + offset;
    return { qBound(0.0, value.x(), doc->logicalSize.width()),
        qBound(0.0, value.y(), doc->logicalSize.height()) };
}
int Overlay::hitEdges(const QPointF& p) const
{
    const auto& r = doc->selection;
    if (r.isEmpty() || !r.adjusted(-9, -9, 9, 9).contains(p))
        return 0;
    int mask = 0;
    if (qAbs(p.x() - r.left()) < 9)
        mask |= 1;
    if (qAbs(p.x() - r.right()) < 9)
        mask |= 2;
    if (qAbs(p.y() - r.top()) < 9)
        mask |= 4;
    if (qAbs(p.y() - r.bottom()) < 9)
        mask |= 8;
    return mask;
}
void Overlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), QColor("#131a21"));
    p.save();
    p.translate(-offset);
    const QRectF whole(QPointF(), doc->logicalSize);
    const auto image = doc->rendered();
    p.drawImage(whole, image);
    p.setClipRect(doc->selection);
    if (drawing)
        paintMark(p, preview, mosaicPreview);
    p.setClipping(false);
    QPainterPath shade;
    shade.addRect(whole);
    if (!doc->selection.isEmpty())
        shade.addRect(doc->selection);
    p.fillPath(shade, QColor(5, 13, 20, 145));
    if (!doc->selection.isEmpty()) {
        p.setPen(QPen(QColor("#9ee0bf"), 1));
        p.drawRect(doc->selection);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#c7f3df"));
        for (const auto& pt : handles(doc->selection))
            p.drawRoundedRect(QRectF(pt - QPointF(3, 3), QSizeF(6, 6)), 1, 1);
        const auto pixels = doc->pixelRect(doc->selection);
        const QString size = QString("%1 × %2 px").arg(pixels.width()).arg(pixels.height());
        const QPointF at(doc->selection.left(), qMax(30.0, doc->selection.top() - 12));
        const QRectF badge(at - QPointF(0, 22), QSizeF(160, 28));
        p.setBrush(QColor("#1b252d"));
        p.drawRoundedRect(badge, 6, 6);
        p.setPen(QColor("#e7f1ed"));
        p.drawText(badge, Qt::AlignCenter, size);
    }
    p.restore();
    if (doc->tool == Document::Picker && rect().contains(pointer.toPoint())) {
        const auto at = docPoint(pointer);
        const auto pixelBounds = doc->pixelRect(QRectF(at, QSizeF(1, 1)));
        const auto pixel = pixelBounds.topLeft();
        if (!pixelBounds.isEmpty() && image.rect().contains(pixel)) {
            const auto color = image.pixelColor(pixel);
            const int zoom = qMin(doc->pickerZoom, qMax(2, qMin((width() - 32) / 15, (height() - 62) / 15)));
            const int side = 15 * zoom, popupWidth = qMax(160, side + 16), popupHeight = side + 46;
            QPointF pos;
            const QList<QPointF> positions { pointer + QPointF(24, 24),
                pointer + QPointF(24, -popupHeight - 24),
                pointer + QPointF(-popupWidth - 24, -popupHeight - 24),
                pointer + QPointF(-popupWidth - 24, 24) };
            for (auto candidate : positions) {
                candidate.setX(qBound(0.0, candidate.x(), qMax(0.0, double(width() - popupWidth - 8))));
                candidate.setY(qBound(0.0, candidate.y(), qMax(0.0, double(height() - popupHeight - 8))));
                pos = candidate;
                const QRectF popup(pos, QSizeF(popupWidth, popupHeight));
                if ((!toolbarViewport->isVisible() || !popup.intersects(toolbarViewport->geometry()))
                    && (!reading->isVisible() || !popup.intersects(reading->geometry())))
                    break;
            }
            p.fillRect(QRectF(pos, QSizeF(popupWidth, popupHeight)), QColor("#1b252d"));
            QImage sample(15, 15, QImage::Format_RGB32);
            sample.fill(QColor("#53616b"));
            {
                QPainter samplePainter(&sample);
                samplePainter.drawImage(QPoint(7, 7) - pixel, image);
            }
            const QRectF view(pos + QPointF((popupWidth - side) / 2.0, 8), QSizeF(side, side));
            p.setRenderHint(QPainter::SmoothPixmapTransform, false);
            p.drawImage(view, sample);
            const auto center = view.center();
            for (const auto& pen : { QPen(Qt::black, 3), QPen(Qt::white, 1) }) {
                p.setPen(pen);
                p.drawLine(
                    QPointF(view.left(), center.y()), QPointF(center.x() - zoom / 2.0 - 1, center.y()));
                p.drawLine(
                    QPointF(center.x() + zoom / 2.0 + 1, center.y()), QPointF(view.right(), center.y()));
                p.drawLine(QPointF(center.x(), view.top()), QPointF(center.x(), center.y() - zoom / 2.0 - 1));
                p.drawLine(
                    QPointF(center.x(), center.y() + zoom / 2.0 + 1), QPointF(center.x(), view.bottom()));
                p.drawRect(QRectF(center - QPointF(zoom / 2.0, zoom / 2.0), QSizeF(zoom, zoom)));
            }
            p.fillRect(QRectF(pos + QPointF(10, side + 20), QSizeF(18, 18)), color);
            p.setPen(Qt::white);
            p.drawText(QRectF(pos + QPointF(34, side + 16), QSizeF(popupWidth - 42, 28)), Qt::AlignCenter,
                color.name().toUpper());
        }
    }
}
void Overlay::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::RightButton) {
        chooseTool(Document::Select);
        return;
    }
    if (e->button() != Qt::LeftButton)
        return;
    if (textInput)
        finishText();
    setFocus();
    pointer = e->position();
    anchor = docPoint(pointer);
    if (doc->tool == Document::Picker) {
        const auto image = doc->rendered();
        const auto pixelBounds = doc->pixelRect(QRectF(anchor, QSizeF(1, 1)));
        const auto pix = pixelBounds.topLeft();
        if (QRectF(rect()).contains(pointer) && !pixelBounds.isEmpty() && image.rect().contains(pix)) {
            doc->color = image.pixelColor(pix);
            QApplication::clipboard()->setText(doc->color.name().toUpper());
            setStatus("已复制颜色 " + doc->color.name().toUpper());
            emit doc->changed();
        }
        update();
        return;
    }
    if (doc->tool == Document::Select || doc->selection.isEmpty()) {
        dragging = true;
        initialSelection = doc->selection;
        edges = hitEdges(anchor);
        moving = !edges && doc->selection.contains(anchor);
        if (!moving && !edges)
            doc->setSelection(QRectF(anchor, anchor));
        layoutToolbar();
        updateCursor();
        return;
    }
    if (!doc->selection.contains(anchor) || doc->tool == Document::ReadText)
        return;
    if (doc->tool == Document::Text) {
        textInput = new AnnotationInput(this);
        textInput->configure(doc->textFont, doc->textColor, doc->textBackgroundOpacity);
        textInput->place(
            pointer.toPoint(), doc->selection.translated(-offset).toAlignedRect().intersected(rect()));
        textInput->installEventFilter(this);
        textInput->show();
        textInput->setFocus();
        connect(textInput, &QLineEdit::returnPressed, this, &Overlay::finishText);
        return;
    }
    drawing = true;
    preview = { };
    preview.start = anchor;
    preview.end = anchor;
    preview.color = doc->color;
    preview.width = doc->tool == Document::Mosaic ? doc->mosaicWidth : doc->penWidth;
    preview.clip = doc->selection;
    const QMap<Document::Tool, Mark::Kind> kinds { { Document::Pen, Mark::Pen },
        { Document::Line, Mark::Line }, { Document::Mosaic, Mark::Mosaic },
        { Document::Redact, Mark::Redact } };
    preview.kind = kinds.value(doc->tool, Mark::Pen);
    preview.path.moveTo(anchor);
    mosaicPreview = doc->tool == Document::Mosaic ? doc->mosaicBrush() : QBrush();
    toolbarViewport->hide();
    reading->hide();
    update();
}
void Overlay::mouseMoveEvent(QMouseEvent* e)
{
    pointer = e->position();
    const auto pt = docPoint(pointer);
    if (dragging) {
        if (moving) {
            auto r = initialSelection.translated(pt - anchor);
            r.moveLeft(qBound(0.0, r.left(), doc->logicalSize.width() - r.width()));
            r.moveTop(qBound(0.0, r.top(), doc->logicalSize.height() - r.height()));
            doc->setSelection(r);
        } else if (edges) {
            auto r = initialSelection;
            if (edges & 1)
                r.setLeft(pt.x());
            if (edges & 2)
                r.setRight(pt.x());
            if (edges & 4)
                r.setTop(pt.y());
            if (edges & 8)
                r.setBottom(pt.y());
            doc->setSelection(r);
        } else
            doc->setSelection(QRectF(anchor, pt));
    } else if (drawing) {
        preview.end = pt;
        preview.path.lineTo(preview.end);
    }
    updateCursor();
    update();
}
void Overlay::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;
    if (dragging || drawing)
        mouseMoveEvent(e);
    if (drawing) {
        drawing = false;
        doc->addMark(preview);
        mosaicPreview = QBrush();
    }
    if (dragging) {
        dragging = false;
        if (doc->selection.width() < 2 || doc->selection.height() < 2)
            doc->setSelection({ });
    }
    layoutToolbar();
    updateCursor();
    update();
}
void Overlay::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape) {
        if (recognitionBusy)
            emit cancelRecognitionRequested();
        else
            emit canceled();
        return;
    }
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        acceptCapture();
        return;
    }
    if (doc->tool == Document::Select && !doc->selection.isEmpty()) {
        QPointF delta;
        if (e->key() == Qt::Key_Left)
            delta.setX(-1);
        else if (e->key() == Qt::Key_Right)
            delta.setX(1);
        else if (e->key() == Qt::Key_Up)
            delta.setY(-1);
        else if (e->key() == Qt::Key_Down)
            delta.setY(1);
        if (!delta.isNull()) {
            if (e->modifiers() & Qt::ShiftModifier)
                delta *= 10;
            auto r = doc->selection.translated(delta);
            r.moveLeft(qBound(0.0, r.left(), doc->logicalSize.width() - r.width()));
            r.moveTop(qBound(0.0, r.top(), doc->logicalSize.height() - r.height()));
            doc->setSelection(r);
            return;
        }
    }
    QWidget::keyPressEvent(e);
}
bool Overlay::eventFilter(QObject* object, QEvent* event)
{
    if (object == textInput && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        textInput->hide();
        textInput->deleteLater();
        textInput = nullptr;
        setFocus();
        return true;
    }
    return QWidget::eventFilter(object, event);
}
void Overlay::finishText()
{
    if (!textInput)
        return;
    const QString text = textInput->text();
    const QPointF baseline = textInput->baseline() + offset;
    const auto font = textInput->font();
    textInput->hide();
    textInput->deleteLater();
    textInput = nullptr;
    if (!text.isEmpty()) {
        Mark m;
        m.kind = Mark::Text;
        m.start = baseline;
        m.color = doc->textColor;
        m.font = font;
        m.clip = doc->selection;
        m.text = text;
        doc->addMark(m);
    }
    setFocus();
}
void Overlay::syncResults()
{
    if (!textLayer)
        return;
    readingOpacity = doc->textBackgroundOpacity;
    textLayer->setResults(doc->textBlocks(), doc->recognitionRegion().translated(-offset),
        doc->translationVisible(), rect(), readingOpacity);
    textLayer->setReadingVisible(doc->tool == Document::ReadText && !doc->textBlocks().isEmpty());
    layoutReadingPanel();
    toolbarViewport->raise();
    bar->positionOptions();
}
void Overlay::layoutReadingPanel()
{
    if (!reading)
        return;
    if (doc->description().isEmpty() || !toolbarViewport->isVisible()) {
        reading->hide();
        return;
    }
    reading->present(doc->description());
    reading->fitNear(toolbarViewport->geometry(), rect().adjusted(8, 8, -8, -8));
    reading->raise();
    toolbarViewport->raise();
}

void Overlay::updateCursor()
{
    Qt::CursorShape shape = Qt::ArrowCursor;
    if (doc->tool == Document::Select && !drawing && !(dragging && moving)) {
        const int edge = dragging ? edges : hitEdges(docPoint(pointer));
        if (edge == 5 || edge == 10)
            shape = Qt::SizeFDiagCursor;
        else if (edge == 6 || edge == 9)
            shape = Qt::SizeBDiagCursor;
        else if (edge == 1 || edge == 2)
            shape = Qt::SizeHorCursor;
        else if (edge == 4 || edge == 8)
            shape = Qt::SizeVerCursor;
    }
    if (cursor().shape() != shape)
        setCursor(shape);
}
void Overlay::chooseTool(Document::Tool tool)
{
    bar->closeOptions();
    finishText();
    drawing = false;
    dragging = false;
    mosaicPreview = QBrush();
    doc->setTool(tool);
}
void Overlay::acceptCapture()
{
    if (drawing || dragging) {
        setStatus("请先释放鼠标，再确认保存。");
        return;
    }
    finishText();
    if (!doc->selection.isEmpty())
        emit accepted();
}
