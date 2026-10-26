#include "panels.h"
#include "ai.h"
#include "document.h"
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QUrl>
SettingsDialog::SettingsDialog(Settings& s, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Mokou Screenshot · 设置");
    resize(680, 600);
    auto main = new QVBoxLayout(this);
    main->setContentsMargins(20, 18, 20, 16);
    main->setSpacing(14);
    auto heading = new QLabel("偏好设置");
    heading->setStyleSheet("font-size:22px;font-weight:600;");
    main->addWidget(heading);
    auto tabs = new QTabWidget;
    tabs->setObjectName("settingsTabs");
    main->addWidget(tabs, 1);
    const auto page = [&](const QString& name) {
        auto scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        auto widget = new QWidget;
        auto form = new QFormLayout(widget);
        form->setContentsMargins(18, 20, 18, 18);
        form->setVerticalSpacing(16);
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        form->setRowWrapPolicy(QFormLayout::WrapLongRows);
        scroll->setWidget(widget);
        tabs->addTab(scroll, name);
        return form;
    };
    auto general = page("常规与录屏");
    const auto directory = [&](const QString& name, const QString& value) {
        auto line = new QLineEdit(value);
        auto row = new QHBoxLayout;
        row->addWidget(line);
        auto choose = new QPushButton("选择…");
        row->addWidget(choose);
        general->addRow(name, row);
        connect(choose, &QPushButton::clicked, this, [this, line] {
            const auto value = QFileDialog::getExistingDirectory(this, "选择保存目录", line->text());
            if (!value.isEmpty())
                line->setText(value);
        });
        return line;
    };
    auto pictures = directory("截图目录", s.imageDirectory);
    auto videos = directory("录屏目录", s.videoDirectory);
    auto format = new QComboBox;
    format->addItems({ "MP4", "WebM" });
    format->setCurrentIndex(s.videoFormat == "webm" ? 1 : 0);
    general->addRow("录屏格式", format);
    auto fps = new QSpinBox;
    fps->setRange(5, 60);
    fps->setValue(s.frameRate);
    fps->setSuffix(" fps");
    general->addRow("录屏帧率", fps);
    auto shortcutNote = new QLabel("截图快捷键为 PrtScn。需要调整快捷键时，请使用 KDE 系统设置。\n录屏时再按 "
                                   "PrtScn 即可停止；声音开关默认关闭。");
    shortcutNote->setWordWrap(true);
    general->addRow(shortcutNote);
    auto kcm = new QPushButton("打开 KDE 快捷键设置");
    general->addRow(kcm);
    connect(kcm, &QPushButton::clicked, this,
        [] { QDesktopServices::openUrl(QUrl("systemsettings://kcm_keys")); });
    auto recognition = page("OCR");
    auto backend = new QComboBox;
    backend->setObjectName("ocrBackend");
    backend->addItems({ "RapidOCR（本地）", "视觉 AI（API）" });
    backend->setCurrentIndex(s.localOcr ? 0 : 1);
    recognition->addRow("OCR 后端", backend);
    auto language = new QComboBox;
    language->setObjectName("rapidLanguage");
    language->addItem("中文／英文／日文混合", "multi");
    language->addItem("法语", "french");
    language->addItem("韩语", "korean");
    language->addItem("俄语", "russian");
    language->setCurrentIndex(qMax(0, language->findData(s.rapidLanguage)));
    recognition->addRow("RapidOCR 语言", language);
    language->setEnabled(s.localOcr);
    connect(backend, &QComboBox::currentIndexChanged, language,
        [language](int n) { language->setEnabled(n == 0); });
    auto ocrNote = new QLabel("RapidOCR 在本机识别，不发送图片，模型在安装时准备。视觉 AI 使用 AI 页 API "
                              "的视觉模型。\n无法定位的文字显示在选区旁，仍可跨段选择、复制。");
    ocrNote->setWordWrap(true);
    recognition->addRow(ocrNote);
    auto apiPage = page("AI");
    auto nested = new QTabWidget;
    nested->setObjectName("aiTabs");
    apiPage->addRow(nested);
    auto connection = new QWidget;
    auto connectionForm = new QFormLayout(connection);
    connectionForm->setRowWrapPolicy(QFormLayout::WrapLongRows);
    nested->addTab(connection, "连接");
    auto base = new QLineEdit(s.api.baseUrl);
    base->setPlaceholderText("http://127.0.0.1:8000/v1");
    connectionForm->addRow("API Base URL", base);
    auto model = new QLineEdit(s.api.model);
    connectionForm->addRow("视觉模型", model);
    auto translator = new QLineEdit(s.api.translationModel);
    translator->setPlaceholderText("留空沿用视觉模型，可填同一 API 下的文本模型");
    connectionForm->addRow("翻译模型", translator);
    auto key = new QLineEdit(s.api.key);
    key->setEchoMode(QLineEdit::Password);
    key->setPlaceholderText("只存本次会话内存");
    connectionForm->addRow("API Key", key);
    auto target = new QLineEdit(s.targetLanguage);
    connectionForm->addRow("译文／回答语言", target);
    auto apiNote = new QLabel("翻译只发送 OCR 原文；识图和视觉 OCR 只发送当前选区图片。Key "
                              "不写普通配置。提示词会保存，请勿在提示词中填写 Key。");
    apiNote->setWordWrap(true);
    connectionForm->addRow(apiNote);
    auto prompts = new QWidget;
    auto promptLayout = new QVBoxLayout(prompts);
    nested->addTab(prompts, "提示词");
    auto promptNote = new QLabel("自定义系统提示词会作为 system 消息发送；任务提示词用于各自功能。程序仍附加 "
                                 "JSON、文字 ID 和坐标协议要求。留空系统提示词时不发送 system 消息。");
    promptNote->setWordWrap(true);
    promptLayout->addWidget(promptNote);
    auto promptTabs = new QTabWidget;
    promptLayout->addWidget(promptTabs);
    const auto prompt = [&](const QString& title, const QString& value) {
        auto edit = new QPlainTextEdit(value);
        edit->setMinimumHeight(180);
        promptTabs->addTab(edit, title);
        return edit;
    };
    auto system = prompt("系统", s.api.systemPrompt);
    system->setObjectName("systemPrompt");
    auto ocr = prompt("OCR", s.api.ocrPrompt);
    auto translation = prompt("翻译", s.api.translationPrompt);
    auto describe = prompt("识图", s.api.describePrompt);
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    main->addWidget(buttons);
    buttons->button(QDialogButtonBox::Save)->setText("保存");
    buttons->button(QDialogButtonBox::Cancel)->setText("取消");
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [=, &s, this] {
        if (!QDir::isAbsolutePath(pictures->text()) || !QDir::isAbsolutePath(videos->text())) {
            QMessageBox::warning(this, "目录需要绝对路径", "请选择完整的本地目录路径。");
            return;
        }
        if (!base->text().trimmed().isEmpty() && AiClient::endpoint(base->text()).isEmpty()) {
            QMessageBox::warning(
                this, "API 地址无效", "请输入有效 http/https 地址，不包含账号、密码或查询参数。");
            return;
        }
        for (auto edit : { system, ocr, translation, describe })
            if (edit->toPlainText().size() > 16000) {
                QMessageBox::warning(this, "提示词过长", "每个提示词最多 16000 字符。");
                return;
            }
        if (target->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "请填写语言", "译文／回答语言不能为空。");
            return;
        }
        Settings candidate = s;
        candidate.imageDirectory = pictures->text();
        candidate.videoDirectory = videos->text();
        candidate.frameRate = fps->value();
        candidate.videoFormat = format->currentIndex() == 1 ? "webm" : "mp4";
        candidate.localOcr = backend->currentIndex() == 0;
        candidate.rapidLanguage = language->currentData().toString();
        candidate.api.baseUrl = base->text().trimmed();
        candidate.api.model = model->text().trimmed();
        candidate.api.translationModel = translator->text().trimmed();
        candidate.api.key = key->text();
        candidate.targetLanguage = target->text().trimmed();
        candidate.api.systemPrompt = system->toPlainText();
        candidate.api.ocrPrompt = ocr->toPlainText();
        candidate.api.translationPrompt = translation->toPlainText();
        candidate.api.describePrompt = describe->toPlainText();
        QString error;
        if (!candidate.save(&error)) {
            QMessageBox::warning(this, "设置未保存", error);
            return;
        }
        s = std::move(candidate);
        accept();
    });
}
CollageDialog::CollageDialog(const QImage& current, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Mokou Screenshot · 拼成长图");
    resize(940, 650);
    auto main = new QVBoxLayout(this);
    main->addWidget(new QLabel("添加图片后拖动列表调整顺序。预览缩放显示，保存时使用原始分辨率。"));
    auto controls = new QHBoxLayout;
    main->addLayout(controls);
    auto add = new QPushButton("添加图片…");
    controls->addWidget(add);
    auto remove = new QPushButton("移除");
    controls->addWidget(remove);
    direction = new QComboBox;
    direction->addItems({ "纵向拼接", "横向拼接" });
    controls->addWidget(direction);
    gap = new QSpinBox;
    gap->setRange(0, 200);
    gap->setSuffix(" px 间距");
    controls->addWidget(gap);
    controls->addStretch();
    auto body = new QHBoxLayout;
    main->addLayout(body, 1);
    list = new QListWidget;
    list->setMaximumWidth(240);
    list->setDragDropMode(QAbstractItemView::InternalMove);
    body->addWidget(list);
    auto scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    preview = new QLabel;
    preview->setAlignment(Qt::AlignCenter);
    scroll->setWidget(preview);
    body->addWidget(scroll, 1);
    auto addImage = [this](const QString& label, const QImage& image) {
        auto item = new QListWidgetItem(label, list);
        item->setData(Qt::UserRole, image);
        item->setIcon(QPixmap::fromImage(image.scaled(80, 60, Qt::KeepAspectRatio)));
    };
    if (!current.isNull())
        addImage("当前截图", current);
    connect(add, &QPushButton::clicked, this, [this, addImage] {
        const auto paths = QFileDialog::getOpenFileNames(this, "添加要拼接的图片",
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
            "图片 (*.png *.jpg *.jpeg *.webp *.bmp)");
        for (const auto& path : paths) {
            QImage i(path);
            if (!i.isNull())
                addImage(QFileInfo(path).fileName(), i);
            else
                QMessageBox::warning(this, "无法打开图片", path);
        }
        refresh();
    });
    connect(remove, &QPushButton::clicked, this, [this] {
        delete list->takeItem(list->currentRow());
        refresh();
    });
    connect(list->model(), &QAbstractItemModel::rowsMoved, this, [this] { refresh(); });
    connect(direction, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(gap, &QSpinBox::valueChanged, this, [this] { refresh(); });
    auto buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText("保存长图并复制");
    main->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        if (resultImage().isNull())
            QMessageBox::warning(
                this, "无法拼接", "请添加图片，并确保结果不超过 1 亿像素、单边不超过 32768 像素。");
        else
            accept();
    });
    refresh();
}
QImage CollageDialog::resultImage() const
{
    QList<QImage> images;
    for (int i = 0; i < list->count(); ++i)
        images << list->item(i)->data(Qt::UserRole).value<QImage>();
    return Document::stitch(images, direction->currentIndex() == 1, gap->value());
}
void CollageDialog::refresh()
{
    QList<QImage> images;
    for (int i = 0; i < list->count(); ++i)
        images << list->item(i)->data(Qt::UserRole).value<QImage>();
    const auto image
        = Document::stitch(images, direction->currentIndex() == 1, gap->value(), QSize(620, 480));
    if (image.isNull()) {
        preview->setText("添加图片以预览；拼接结果过大时请减少图片。");
        return;
    }
    preview->setPixmap(QPixmap::fromImage(image));
}
