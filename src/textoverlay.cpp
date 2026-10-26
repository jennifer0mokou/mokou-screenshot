#include "textoverlay.h"
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QTextBoundaryFinder>
#include <QTextLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <vector>
namespace
{
struct TextLayout {
    std::unique_ptr<QTextLayout> text;
    qreal width = 0, height = 0, top = 0;
};
TextLayout layoutText(const QString& input, const QFont& font, qreal width)
{
    // QTextLayout uses a line separator within one paragraph; offsets still match the source.
    auto display = input;
    display.replace('\n', QChar::LineSeparator);
    TextLayout result { std::make_unique<QTextLayout>(display, font) };
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    result.text->setTextOption(option);
    result.text->beginLayout();
    qreal y = 0;
    while (true) {
        auto line = result.text->createLine();
        if (!line.isValid())
            break;
        line.setLineWidth(qMax(1.0, width));
        line.setPosition(QPointF(0, y));
        const auto fragment = display.mid(line.textStart(), line.textLength()).trimmed();
        const auto glyph = QFontMetricsF(font).tightBoundingRect(fragment.isEmpty() ? "M" : fragment);
        if (result.text->lineCount() == 1)
            result.top = line.ascent() + glyph.top();
        result.height = y + line.ascent() + glyph.bottom() - result.top;
        result.width = qMax(result.width, line.naturalTextWidth());
        y += line.height();
    }
    result.text->endLayout();
    return result;
}
bool fits(const TextLayout& layout, const QSizeF& size)
{
    return layout.width <= size.width() + .5 && layout.height <= size.height() + .5;
}
}
struct TextOverlay::State {
    struct Item {
        QString text;
        QRectF bounds, collapsed;
        QFont font;
        TextLayout layout;
        int start = 0;
        bool overflow = false;
    };
    std::vector<Item> items;
    QList<TextBlock> source;
    QRectF region;
    bool translated = false;
    QString all;
    QRect area;
    int anchor = 0, cursor = 0, expanded = -1, opacity = 220;
    qreal scroll = 0;
    bool dragging = false;
    QRectF moreRect(int i) const
    {
        const auto& r = items[i].bounds;
        return QRectF(r.right() - qMin(20.0, r.width()), r.bottom() - qMin(18.0, r.height()),
            qMin(20.0, r.width()), qMin(18.0, r.height()));
    }
    QPointF origin(int i) const
    {
        return items[i].bounds.topLeft() - QPointF(0, items[i].layout.top + (i == expanded ? scroll : 0));
    }
};
TextOverlay::TextOverlay(QWidget* parent)
    : QWidget(parent)
    , state(std::make_unique<State>())
{
    setObjectName("ocrReadingLayer");
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::IBeamCursor);
    setMouseTracking(true);
    hide();
}
TextOverlay::~TextOverlay() = default;
void TextOverlay::setResults(const QList<TextBlock>& blocks, const QRectF& region, bool translated,
    const QRect& available, int opacity)
{
    const auto area = available.adjusted(8, 8, -8, -8);
    if (state->source == blocks && state->region == region && state->translated == translated
        && state->area == area) {
        state->opacity = opacity;
        update();
        return;
    }
    state = std::make_unique<State>();
    auto& s = *state;
    s.source = blocks;
    s.region = region;
    s.translated = translated;
    s.area = area;
    s.opacity = opacity;
    setGeometry(parentWidget()->rect());
    qreal fallbackY = qMax(qreal(s.area.top()), region.top());
    const qreal fallbackWidth = qMin(360, s.area.width());
    qreal fallbackX = region.right() + 12;
    if (fallbackX + fallbackWidth > s.area.right())
        fallbackX = qMax(qreal(s.area.left()), region.left() - fallbackWidth - 12);
    QList<TextBlock> displayed;
    for (auto block : blocks) {
        if (block.box.isEmpty() && block.translation.isEmpty())
            block.translation = block.source;
        if (!displayed.isEmpty() && block.box.isEmpty() && displayed.last().box.isEmpty()) {
            displayed.last().source += '\n' + block.source;
            displayed.last().translation += '\n' + block.translation;
        } else
            displayed.append(block);
    }
    for (qsizetype i = 0; i < displayed.size(); ++i) {
        const auto& block = displayed[i];
        if (!s.all.isEmpty())
            s.all += '\n';
        const int start = s.all.size();
        const QString text = translated && !block.translation.isEmpty() ? block.translation : block.source;
        s.all += text;
        QRectF bounds(region.x() + block.box.x() * region.width(),
            region.y() + block.box.y() * region.height(), block.box.width() * region.width(),
            block.box.height() * region.height());
        const bool unplaced = bounds.width() < 1 || bounds.height() < 1;
        if (unplaced) {
            bounds = QRectF(fallbackX, fallbackY, fallbackWidth, 50).intersected(s.area);
            fallbackY = qMin(qreal(s.area.bottom() - 50), bounds.bottom() + 8);
        } else {
            for (qsizetype j = 0; j < displayed.size(); ++j) {
                const auto b = displayed[j].box;
                const QRectF next(region.x() + b.x() * region.width(), region.y() + b.y() * region.height(),
                    b.width() * region.width(), b.height() * region.height());
                if (j != i && next.center().y() > bounds.center().y() && next.top() > bounds.top()
                    && next.left() < bounds.right() && next.right() > bounds.left())
                    bounds.setBottom(qMin(bounds.bottom(), next.top()));
            }
        }
        QFont textFont = font();
        int best = 14;
        if (!unplaced) {
            int low = 4, high = qBound(4, int(bounds.height() * 1.5), 160);
            best = 4;
            while (low <= high) {
                const int candidate = (low + high) / 2;
                textFont.setPixelSize(candidate);
                if (fits(layoutText(block.source, textFont, bounds.width()), bounds.size())) {
                    best = candidate;
                    low = candidate + 1;
                } else
                    high = candidate - 1;
            }
        }
        textFont.setPixelSize(best);
        auto layout = layoutText(text, textFont, bounds.width());
        // A translation can use taller glyphs than the Latin source at the same font size.
        while (textFont.pixelSize() > 4 && layout.text->lineCount() > 0) {
            const auto line = layout.text->lineAt(0);
            const auto first = text.left(line.textLength()).trimmed();
            if (QFontMetricsF(textFont).tightBoundingRect(first).height() <= bounds.height() + .5)
                break;
            textFont.setPixelSize(textFont.pixelSize() - 1);
            layout = layoutText(text, textFont, bounds.width());
        }
        const bool overflow = !fits(layout, bounds.size());
        bounds.setWidth(qMax(1.0, qMin(bounds.width(), std::ceil(layout.width))));
        bounds.setHeight(qMax(1.0, qMin(bounds.height(), std::ceil(layout.height))));
        s.items.push_back({ text, bounds, bounds, textFont, std::move(layout), start, overflow });
    }
    updateMask();
    update();
}
QString TextOverlay::plainText() const
{
    return state->all;
}
QString TextOverlay::selectedText() const
{
    return state->all.mid(qMin(state->anchor, state->cursor), qAbs(state->cursor - state->anchor));
}
QRectF TextOverlay::blockBounds(int i) const
{
    return state->items.at(i).bounds;
}
bool TextOverlay::truncated(int i) const
{
    return state->items.at(i).overflow;
}
void TextOverlay::selectAll()
{
    state->anchor = 0;
    state->cursor = state->all.size();
    update();
}
void TextOverlay::setReadingVisible(bool visible)
{
    const bool hasVisibleText = std::any_of(state->items.begin(), state->items.end(),
        [this](const auto& item) { return item.bounds.intersects(rect()); });
    setVisible(visible && hasVisibleText);
}
void TextOverlay::updateMask()
{
    QRegion mask;
    for (const auto& item : state->items)
        mask += item.bounds.toAlignedRect().intersected(rect());
    // An empty QWidget mask means the entire widget, so hide an empty layer explicitly.
    setMask(mask);
    setVisible(!mask.isEmpty());
}
void TextOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    const auto& s = *state;
    const auto draw = [&](int i) {
        const auto& item = s.items[i];
        painter.save();
        painter.setClipRect(item.bounds);
        painter.fillRect(item.bounds, QColor(241, 248, 244, s.opacity));
        QList<QTextLayout::FormatRange> formats;
        const int first = qMax(item.start, qMin(s.anchor, s.cursor));
        const int last = qMin(item.start + int(item.text.size()), qMax(s.anchor, s.cursor));
        if (last > first) {
            QTextCharFormat format;
            format.setBackground(QColor("#83cdb0"));
            format.setForeground(QColor("#10261e"));
            formats << QTextLayout::FormatRange { first - item.start, last - first, format };
        }
        painter.setPen(QColor("#10261e"));
        item.layout.text->draw(&painter, s.origin(i), formats);
        if (item.overflow) {
            painter.fillRect(s.moreRect(i), QColor(224, 242, 233, 240));
            auto f = font();
            f.setPixelSize(12);
            painter.setFont(f);
            painter.drawText(s.moreRect(i), Qt::AlignCenter, i == s.expanded ? "−" : "…");
        }
        painter.restore();
    };
    for (int i = 0; i < int(s.items.size()); ++i)
        if (i != s.expanded)
            draw(i);
    if (s.expanded >= 0)
        draw(s.expanded);
}
int TextOverlay::positionAt(const QPointF& point) const
{
    const auto& s = *state;
    int nearest = -1;
    qreal distance = std::numeric_limits<qreal>::max();
    for (int i = 0; i < int(s.items.size()); ++i) {
        const auto& r = s.items[i].bounds;
        if (!r.intersects(rect()))
            continue;
        const qreal dx = std::max({ r.left() - point.x(), 0.0, point.x() - r.right() });
        const qreal dy = std::max({ r.top() - point.y(), 0.0, point.y() - r.bottom() });
        const qreal d = dx * dx + dy * dy;
        if (d < distance || (i == s.expanded && r.contains(point))) {
            distance = d;
            nearest = i;
        }
        if (nearest == s.expanded && distance == 0)
            break;
    }
    if (nearest < 0)
        return 0;
    const auto& item = s.items[nearest];
    const auto local = point - s.origin(nearest);
    auto* layout = item.layout.text.get();
    for (int i = 0; i < layout->lineCount(); ++i) {
        const auto line = layout->lineAt(i);
        if (local.y() < line.y() + line.height() || i == layout->lineCount() - 1)
            return item.start + line.xToCursor(local.x());
    }
    return item.start;
}
void TextOverlay::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton)
        return;
    auto& s = *state;
    const bool inExpanded = s.expanded >= 0 && s.items[s.expanded].bounds.contains(e->position());
    for (int i = int(s.items.size()) - 1; i >= 0; --i)
        if ((!inExpanded || i == s.expanded) && s.items[i].overflow
            && s.moreRect(i).contains(e->position())) {
            expand(i == s.expanded ? -1 : i);
            return;
        }
    setFocus();
    s.cursor = positionAt(e->position());
    if (!(e->modifiers() & Qt::ShiftModifier))
        s.anchor = s.cursor;
    s.dragging = true;
    update();
}
void TextOverlay::mouseMoveEvent(QMouseEvent* e)
{
    if (state->dragging) {
        state->cursor = positionAt(e->position());
        update();
    }
}
void TextOverlay::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && state->dragging) {
        state->cursor = positionAt(e->position());
        state->dragging = false;
        update();
    }
}
void TextOverlay::mouseDoubleClickEvent(QMouseEvent* e)
{
    const int position = positionAt(e->position());
    QTextBoundaryFinder boundary(QTextBoundaryFinder::Word, state->all);
    boundary.setPosition(position);
    state->anchor = boundary.toPreviousBoundary();
    boundary.setPosition(position);
    state->cursor = boundary.toNextBoundary();
    state->anchor = qMax(0, state->anchor);
    if (state->cursor < 0)
        state->cursor = state->all.size();
    update();
}
void TextOverlay::expand(int index)
{
    auto& s = *state;
    if (s.expanded >= 0) {
        auto& item = s.items[s.expanded];
        item.bounds = item.collapsed;
        item.layout = layoutText(item.text, item.font, item.bounds.width());
    }
    s.expanded = index;
    s.scroll = 0;
    if (index >= 0) {
        auto& item = s.items[index];
        const int width = qMin(s.area.width(), qMax(320, int(item.collapsed.width())));
        item.layout = layoutText(item.text, item.font, width);
        const int height = qMin(s.area.height(), qMax(40, int(std::ceil(item.layout.height))));
        item.bounds = QRectF(qBound(s.area.left(), int(item.collapsed.x()), s.area.right() - width + 1),
            qBound(s.area.top(), int(item.collapsed.y()), s.area.bottom() - height + 1), width, height);
        setFocus();
    }
    updateMask();
    update();
}
void TextOverlay::keyPressEvent(QKeyEvent* e)
{
    auto& s = *state;
    if (e->matches(QKeySequence::Copy)) {
        if (!selectedText().isEmpty())
            QApplication::clipboard()->setText(selectedText());
    } else if (e->matches(QKeySequence::SelectAll)) {
        selectAll();
    } else if (e->key() == Qt::Key_Escape && (s.anchor != s.cursor || s.expanded >= 0)) {
        s.anchor = s.cursor;
        if (s.expanded >= 0)
            expand(-1);
        update();
    } else if (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right || e->key() == Qt::Key_Home
        || e->key() == Qt::Key_End) {
        const bool extend = e->modifiers() & Qt::ShiftModifier;
        int next = s.cursor;
        if (e->key() == Qt::Key_Home)
            next = 0;
        else if (e->key() == Qt::Key_End)
            next = s.all.size();
        else if (!extend && s.anchor != s.cursor)
            next = e->key() == Qt::Key_Left ? qMin(s.anchor, s.cursor) : qMax(s.anchor, s.cursor);
        else {
            QTextBoundaryFinder boundary(QTextBoundaryFinder::Grapheme, s.all);
            boundary.setPosition(s.cursor);
            const int position
                = e->key() == Qt::Key_Left ? boundary.toPreviousBoundary() : boundary.toNextBoundary();
            if (position >= 0)
                next = position;
        }
        s.cursor = qBound(0, next, int(s.all.size()));
        if (!(e->modifiers() & Qt::ShiftModifier))
            s.anchor = s.cursor;
        update();
    } else {
        QWidget::keyPressEvent(e);
        return;
    }
    e->accept();
}
void TextOverlay::wheelEvent(QWheelEvent* e)
{
    auto& s = *state;
    if (s.expanded < 0) {
        e->ignore();
        return;
    }
    const auto& item = s.items[s.expanded];
    const qreal delta = e->pixelDelta().isNull() ? e->angleDelta().y() / 3.0 : e->pixelDelta().y();
    s.scroll = qBound(0.0, s.scroll - delta, qMax(0.0, item.layout.height - item.bounds.height()));
    update();
    e->accept();
}
void TextOverlay::contextMenuEvent(QContextMenuEvent* e)
{
    QMenu menu(this);
    auto copy = menu.addAction("复制", this, [this] { QApplication::clipboard()->setText(selectedText()); });
    copy->setEnabled(!selectedText().isEmpty());
    menu.addAction("全选", this, &TextOverlay::selectAll);
    menu.exec(e->globalPos());
}
bool TextOverlay::event(QEvent* e)
{
    if (e->type() == QEvent::ShortcutOverride) {
        auto key = static_cast<QKeyEvent*>(e);
        if (key->matches(QKeySequence::Copy) || key->matches(QKeySequence::SelectAll)) {
            e->accept();
            return true;
        }
    }
    return QWidget::event(e);
}
