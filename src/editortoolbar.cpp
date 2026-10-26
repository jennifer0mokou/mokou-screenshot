#include "editortoolbar.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFontComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QSpinBox>
#include <QToolButton>
#include <algorithm>
#include <functional>
namespace
{
const char* toolbarStyle
    = "QFrame#editorBar,QFrame#toolOptions{background:rgba(25,33,37,248);border:1px solid "
      "#43514f;border-radius:12px;}"
      "QPushButton,QToolButton{color:#e6eeea;background:transparent;border:0;border-radius:6px;padding:7px "
      "9px;}"
      "QPushButton:hover,QToolButton:hover{background:#34443f;}"
      "QPushButton:checked,QToolButton:checked{background:#38594d;color:#c7f3df;}"
      "QPushButton:disabled,QToolButton:disabled{color:#687b73;}"
      "QPushButton#save{background:#a7e4c6;color:#142e23;font-weight:600;padding:8px 14px;}"
      "QPushButton#save:hover{background:#c1f1d9;}"
      "QLabel,QCheckBox{color:#e0eae5;background:transparent;border:0;}"
      "QLabel#toolbarStatus{color:#a9bcb1;}"
      "QComboBox,QSpinBox{background:#2c3b35;color:#e6eeea;border:1px solid "
      "#4c6257;border-radius:5px;padding:5px;}"
      "QComboBox QAbstractItemView{background:#25372f;color:#e6eeea;selection-background-color:#426951;}"
      "QCheckBox::indicator{width:16px;height:16px;}";
QPushButton* action(const QString& text, QBoxLayout* row, const std::function<void()>& fn = { })
{
    auto b = new QPushButton(text);
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(Qt::ArrowCursor);
    row->addWidget(b);
    if (fn)
        QObject::connect(b, &QPushButton::clicked, b, fn);
    return b;
}
void divider(QBoxLayout* row)
{
    auto line = new QFrame;
    line->setFixedSize(1, 20);
    line->setStyleSheet("background:#43514b;border:0;");
    row->addSpacing(4);
    row->addWidget(line);
    row->addSpacing(4);
}
}
EditorToolbar::EditorToolbar(Document* document, QWidget* overlay)
    : QFrame()
    , doc(document)
    , surface(overlay)
{
    setObjectName("editorBar");
    setStyleSheet(toolbarStyle);
    auto main = new QVBoxLayout(this);
    main->setContentsMargins(10, 8, 10, 7);
    main->setSpacing(4);
    auto tools = new QHBoxLayout;
    tools->setSpacing(2);
    main->addLayout(tools);
    const QList<QPair<QString, Document::Tool>> names { { "选区 S", Document::Select },
        { "画笔 B", Document::Pen }, { "直线 L", Document::Line }, { "马赛克 M", Document::Mosaic },
        { "遮挡 D", Document::Redact }, { "文字 T", Document::Text }, { "取色 C", Document::Picker } };
    for (const auto& entry : names) {
        auto b = action(entry.first, tools, [this, entry] { emit toolRequested(entry.second); });
        b->setCheckable(true);
        b->setToolTip(entry.first);
        connect(doc, &Document::changed, b, [this, b, entry] { b->setChecked(doc->tool == entry.second); });
        b->setChecked(doc->tool == entry.second);
        if (entry.second == Document::Text || entry.second == Document::Picker) {
            auto arrow = new QToolButton;
            arrow->setObjectName(
                entry.second == Document::Text ? "textOptionsButton" : "pickerOptionsButton");
            arrow->setArrowType(Qt::DownArrow);
            arrow->setFixedWidth(22);
            arrow->setToolTip(entry.second == Document::Text ? "文字样式" : "取色放大倍数");
            tools->addWidget(arrow);
            connect(arrow, &QToolButton::clicked, this, [this, arrow, entry] {
                if (options->isVisible() && optionAnchor == arrow) {
                    closeOptions();
                    return;
                }
                if (entry.second == Document::Text)
                    textOptions(arrow);
                else
                    pickerOptions(arrow);
            });
        }
    }
    auto brush = action("● 3 px ▾", tools);
    brush->setObjectName("brushOptionsButton");

    connect(brush, &QPushButton::clicked, this, [this, brush] { brushOptions(brush); });
    const auto syncBrush = [this, brush] {
        const bool mosaic = doc->tool == Document::Mosaic;
        brush->setText(QString("● %1 px ▾").arg(qRound(mosaic ? doc->mosaicWidth : doc->penWidth)));
        brush->setStyleSheet("color:" + (mosaic ? QString("#c7f3df") : doc->color.name()) + ";");
        brush->setToolTip(mosaic ? "马赛克涂抹直径" : "画笔、直线的颜色和粗细");
        brush->setVisible(
            doc->tool == Document::Pen || doc->tool == Document::Line || doc->tool == Document::Mosaic);
    };
    connect(doc, &Document::changed, brush, syncBrush);
    syncBrush();
    tools->addStretch();
    divider(tools);
    auto undo = action("撤销", tools, [this] { doc->undo(); });
    auto redo = action("重做", tools, [this] { doc->redo(); });
    undo->setToolTip("撤销 Ctrl+Z");
    redo->setToolTip("重做 Ctrl+Shift+Z");
    const auto syncHistory = [this, undo, redo] {
        undo->setEnabled(doc->markCount() > 0);
        redo->setEnabled(doc->canRedo());
    };
    connect(doc, &Document::changed, this, syncHistory);
    syncHistory();
    auto row = new QHBoxLayout;
    row->setSpacing(2);
    main->addLayout(row);
    for (const auto& name : { QString("OCR"), QString("翻译"), QString("识图") }) {
        auto b = action(name, row, [this, name] {
            closeOptions();
            if (name == "OCR")
                emit ocrRequested();
            else if (name == "翻译")
                emit translateRequested();
            else
                emit describeRequested();
        });
        connect(this, &EditorToolbar::layoutChanged, b, [this, b] { b->setEnabled(!busy); });
    }
    divider(row);
    auto original = action("原文", row, [this] { doc->setTranslationVisible(false); });
    auto translated = action("译文", row, [this] { doc->setTranslationVisible(true); });
    auto copy = action("复制文字", row, [this] { emit copyTextRequested(); });
    copy->setToolTip("复制所选文字；没有选择时复制全部");
    original->setCheckable(true);
    translated->setCheckable(true);
    const auto syncReading = [this, original, translated, copy] {
        const bool hasText = !doc->textBlocks().isEmpty();
        const bool hasTranslation = std::any_of(doc->textBlocks().begin(), doc->textBlocks().end(),
            [](const TextBlock& b) { return !b.translation.isEmpty(); });
        original->setEnabled(hasText);
        translated->setEnabled(hasTranslation);
        copy->setEnabled(hasText);
        original->setChecked(hasText && !doc->translationVisible() && doc->tool == Document::ReadText);
        translated->setChecked(
            hasTranslation && doc->translationVisible() && doc->tool == Document::ReadText);
    };
    connect(doc, &Document::changed, this, syncReading);
    syncReading();
    row->addStretch();
    auto record = action("录屏 ▾", row);
    connect(record, &QPushButton::clicked, this, [this, record] { recordingOptions(record); });
    auto more = action("更多 ···", row);
    connect(more, &QPushButton::clicked, this, [this, more] { moreOptions(more); });
    divider(row);
    action("取消", row, [this] { emit canceled(); });
    action("保存 ↵", row, [this] {
        closeOptions();
        emit accepted();
    })->setObjectName("save");
    auto feedback = new QHBoxLayout;
    main->addLayout(feedback);
    status = new QLabel("拖动边缘调整 · Enter 保存并复制 · Esc 取消");
    status->setObjectName("toolbarStatus");
    status->setTextFormat(Qt::PlainText);
    status->setMinimumWidth(0);
    status->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto small = status->font();
    small.setPixelSize(12);
    status->setFont(small);
    feedback->addWidget(status, 1);
    cancelTask = new QToolButton;
    cancelTask->setText("停止识别");
    feedback->addWidget(cancelTask);
    cancelTask->hide();
    connect(cancelTask, &QToolButton::clicked, this, &EditorToolbar::cancelRecognitionRequested);
    options = new QFrame(surface);
    options->setObjectName("toolOptions");
    options->setStyleSheet(toolbarStyle);
    optionsLayout = new QVBoxLayout(options);
    optionsLayout->setContentsMargins(14, 12, 14, 12);
    optionsLayout->setSpacing(10);
    options->hide();
    options->installEventFilter(this);
    qApp->installEventFilter(this);
}
EditorToolbar::~EditorToolbar()
{
    qApp->removeEventFilter(this);
    delete options.data();
}
void EditorToolbar::setStatus(const QString& text)
{
    status->setText(text);
    status->setToolTip(text);
}
void EditorToolbar::setBusy(bool value)
{
    busy = value;
    cancelTask->setVisible(busy);
    emit layoutChanged();
}
void EditorToolbar::closeOptions()
{
    options->hide();
    optionAnchor = nullptr;
}
void EditorToolbar::openOptions(QWidget* anchor, const QString& title)
{
    closeOptions();
    while (auto item = optionsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    optionAnchor = anchor;
    auto heading = new QLabel(title);
    heading->setStyleSheet("font-weight:600;color:#c2e7d3;");
    optionsLayout->addWidget(heading);
}
void EditorToolbar::positionOptions()
{
    if (!optionAnchor || !isVisible()) {
        closeOptions();
        return;
    }
    options->adjustSize();
    auto at = optionAnchor->mapTo(surface, QPoint(0, optionAnchor->height() + 8));
    const auto barAt = mapTo(surface, QPoint());
    at.setY(barAt.y() + height() + 8);
    if (at.y() + options->height() > surface->height() - 8)
        at.setY(barAt.y() - options->height() - 8);
    at.setX(qBound(8, at.x(), qMax(8, surface->width() - options->width() - 8)));
    at.setY(qBound(8, at.y(), qMax(8, surface->height() - options->height() - 8)));
    options->move(at);
    options->show();
    options->raise();
}
void EditorToolbar::styleChanged()
{
    emit doc->changed();
    emit preferencesChanged();
}
void EditorToolbar::palette(QVBoxLayout* layout, bool text)
{
    auto holder = new QWidget;
    auto row = new QHBoxLayout(holder);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(7);
    for (const QString color : { "#ff647c", "#ffd479", "#9ee0bf", "#80bbff", "#ffffff", "#181c20" }) {
        auto b = action("", row, [this, text, color] {
            if (text)
                doc->textColor = QColor(color);
            else
                doc->color = QColor(color);
            if (text)
                styleChanged();
            else
                emit doc->changed();
        });
        b->setObjectName("colorSwatch");
        b->setAccessibleName(color);
        b->setFixedSize(26, 26);
        b->setCheckable(true);
        const auto sync = [this, b, text, color] {
            const bool selected = (text ? doc->textColor : doc->color) == QColor(color);
            b->setChecked(selected);
            b->setStyleSheet("background:" + color
                + ";color:" + (QColor(color).lightness() > 140 ? QString("#142e23") : QString("#ffffff"))
                + ";border:2px solid " + (selected ? QString("#e6f7ed") : QString("#65766e"))
                + ";border-radius:13px;");
            b->setText(selected ? "✓" : "");
        };
        connect(doc, &Document::changed, b, sync);
        sync();
    }
    layout->addWidget(holder);
}
void EditorToolbar::textOptions(QWidget* anchor)
{
    openOptions(anchor, "文字样式");
    auto family = new QFontComboBox;
    family->setObjectName("textFamily");
    family->setFixedWidth(230);
    family->setCurrentFont(doc->textFont);
    optionsLayout->addWidget(family);
    auto size = new QSpinBox;
    size->setObjectName("textSize");
    size->setRange(8, 160);
    size->setSuffix(" px");
    size->setValue(doc->textFont.pixelSize());
    optionsLayout->addWidget(size);
    palette(optionsLayout, true);
    auto label = new QLabel("阅读与输入背景");
    optionsLayout->addWidget(label);
    auto opacity = new QSlider(Qt::Horizontal);
    opacity->setObjectName("textOpacity");
    opacity->setRange(32, 100);
    opacity->setValue(qRound(doc->textBackgroundOpacity * 100.0 / 255));
    optionsLayout->addWidget(opacity);
    connect(family, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
        doc->textFont.setFamily(f.family());
        styleChanged();
    });
    connect(size, &QSpinBox::valueChanged, this, [this](int n) {
        doc->textFont.setPixelSize(n);
        styleChanged();
    });
    connect(opacity, &QSlider::valueChanged, this, [this](int n) {
        doc->textBackgroundOpacity = qRound(n * 255.0 / 100);
        styleChanged();
    });
    positionOptions();
}
void EditorToolbar::pickerOptions(QWidget* anchor)
{
    openOptions(anchor, "取色放大倍数");
    auto zoom = new QComboBox;
    zoom->setObjectName("pickerZoom");
    for (const int value : { 4, 8, 12, 16, 24 })
        zoom->addItem(QString("%1 ×").arg(value), value);
    if (zoom->findData(doc->pickerZoom) < 0)
        zoom->addItem(QString("%1 ×").arg(doc->pickerZoom), doc->pickerZoom);
    zoom->setCurrentIndex(zoom->findData(doc->pickerZoom));
    optionsLayout->addWidget(zoom);
    connect(zoom, &QComboBox::currentIndexChanged, this, [this, zoom] {
        doc->pickerZoom = zoom->currentData().toInt();
        styleChanged();
    });
    positionOptions();
}
void EditorToolbar::brushOptions(QWidget* anchor)
{
    const bool mosaic = doc->tool == Document::Mosaic;
    openOptions(anchor, mosaic ? "马赛克画笔" : "线条样式");
    auto size = new QSpinBox;
    size->setObjectName("brushWidth");
    size->setRange(mosaic ? 8 : 1, mosaic ? 128 : 24);
    size->setSuffix(" px");
    const auto sync = [this, size, mosaic] {
        const QSignalBlocker block(size);
        size->setValue(qRound(mosaic ? doc->mosaicWidth : doc->penWidth));
    };
    sync();
    optionsLayout->addWidget(size);
    if (!mosaic)
        palette(optionsLayout, false);
    connect(size, &QSpinBox::valueChanged, this, [this, mosaic](int n) {
        if (mosaic)
            doc->mosaicWidth = n;
        else
            doc->penWidth = n;
        emit doc->changed();
    });
    connect(doc, &Document::changed, size, sync);
    positionOptions();
}
void EditorToolbar::recordingOptions(QWidget* anchor)
{
    openOptions(anchor, "录制当前选区");
    auto audio = new QCheckBox("系统声音");
    auto mic = new QCheckBox("麦克风");
    optionsLayout->addWidget(audio);
    optionsLayout->addWidget(mic);
    action("● 开始录屏", optionsLayout, [this, audio, mic] {
        const bool system = audio->isChecked(), microphone = mic->isChecked();
        closeOptions();
        emit recordRequested(system, microphone);
    });
    positionOptions();
}
void EditorToolbar::moreOptions(QWidget* anchor)
{
    openOptions(anchor, "更多");
    action("拼长图", optionsLayout, [this] {
        closeOptions();
        emit collageRequested();
    });
    action("设置", optionsLayout, [this] {
        closeOptions();
        emit settingsRequested();
    });
    positionOptions();
}
bool EditorToolbar::eventFilter(QObject* watched, QEvent* event)
{
    if (!options || !options->isVisible() || QApplication::activePopupWidget())
        return false;
    if (watched == options && event->type() == QEvent::KeyPress) {
        const auto key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter)
            return true;
    }
    if (event->type() == QEvent::MouseButtonPress) {
        auto widget = qobject_cast<QWidget*>(watched);
        if (widget && widget != options && !options->isAncestorOf(widget) && widget != optionAnchor)
            closeOptions();
    } else if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        closeOptions();
        surface->setFocus();
        return true;
    }
    return QFrame::eventFilter(watched, event);
}
