#include "annotationinput.h"
#include "editortoolbar.h"
#include "overlay.h"
#include "readingpanel.h"
#include "textoverlay.h"
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFontComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QToolButton>
#include <QtTest>
class EditorCheck : public QObject
{
    Q_OBJECT
private slots:
    void pickerAndPendingText()
    {
        // Offscreen regression checks do not acquire the user's desktop clipboard.
        if (QGuiApplication::platformName() != "offscreen")
            QSKIP("Offscreen-only regression case");
        QImage image(1680, 1000, QImage::Format_RGB32);
        image.fill(Qt::white);
        Document document;
        document.reset(image, image.size());
        document.setSelection(QRectF(100, 100, 1200, 500));
        auto overlay = new Overlay(&document, QGuiApplication::primaryScreen(), true);
        overlay->resize(image.size());
        overlay->show();
        QVERIFY(QTest::qWaitForWindowExposed(overlay));
        Mark redact;
        redact.kind = Mark::Redact;
        redact.start = { 150, 150 };
        redact.end = { 400, 400 };
        document.addMark(redact);
        document.setTool(Document::Picker);
        QTest::mouseMove(overlay, QPoint(250, 250));
        const auto view = overlay->grab().toImage();
        QCOMPARE(view.pixelColor(285, 373), QColor(Qt::black));
        QTest::mouseClick(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(250, 250));
        QCOMPARE(QApplication::clipboard()->text(), QString("#000000"));
        document.setTool(Document::Text);
        QTest::mouseClick(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(500, 350));
        auto input = overlay->findChild<QLineEdit*>();
        QVERIFY(input);
        input->setText("pending annotation");
        QSignalSpy requested(overlay, &Overlay::ocrRequested);
        for (auto button : overlay->findChildren<QPushButton*>())
            if (button->text() == "OCR")
                button->click();
        QCOMPARE(requested.size(), 1);
        QCOMPARE(document.markCount(), 2);
        overlay->resize(420, 800);
        QCoreApplication::processEvents();
        auto viewport = overlay->findChild<QScrollArea*>("toolbarViewport");
        QVERIFY(viewport);
        QVERIFY(viewport->geometry().right() < overlay->width());
        QVERIFY(viewport->horizontalScrollBar()->maximum() > 0);
        overlay->close();
        QTest::qWait(5);
    }
    void textEnterDoesNotAlsoSave()
    {
        if (QGuiApplication::platformName() != "offscreen")
            QSKIP("Offscreen-only regression case");
        QImage image(1680, 1000, QImage::Format_RGB32);
        image.fill(Qt::white);
        Document document;
        document.reset(image, image.size());
        document.setSelection(QRectF(100, 100, 1200, 500));
        auto overlay = new Overlay(&document, QGuiApplication::primaryScreen(), true);
        overlay->resize(image.size());
        overlay->show();
        QVERIFY(QTest::qWaitForWindowExposed(overlay));
        document.setTool(Document::Text);
        QTest::mouseClick(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(300, 300));
        auto input = overlay->findChild<QLineEdit*>();
        QVERIFY(input);
        input->setText("annotation");
        QSignalSpy saved(overlay, &Overlay::accepted);
        QTest::keyClick(input, Qt::Key_Return);
        QCOMPARE(document.markCount(), 1);
        QCOMPARE(saved.size(), 0);
        QTest::keyClick(overlay, Qt::Key_Return);
        QCOMPARE(saved.size(), 1);
        overlay->close();
        QTest::qWait(5);
    }
    void brushWidthMatchesSharedDocument()
    {
        if (QGuiApplication::platformName() != "offscreen")
            QSKIP("Offscreen-only regression case");
        QImage image(800, 600, QImage::Format_RGB32);
        image.fill(Qt::white);
        Document document;
        document.reset(image, image.size());
        document.penWidth = 11;
        auto first = new Overlay(&document, QGuiApplication::primaryScreen(), true);
        auto second = new Overlay(&document, QGuiApplication::primaryScreen(), true);
        document.setSelection(QRectF(100, 100, 400, 300));
        document.setTool(Document::Pen);
        first->show();
        second->show();
        first->findChild<QPushButton*>("brushOptionsButton")->click();
        second->findChild<QPushButton*>("brushOptionsButton")->click();
        auto a = first->findChild<QSpinBox*>("brushWidth");
        auto b = second->findChild<QSpinBox*>("brushWidth");
        QVERIFY(a);
        QVERIFY(b);
        QCOMPARE(a->value(), 11);
        QCOMPARE(b->value(), 11);
        a->setValue(8);
        QCOMPARE(document.penWidth, 8.0);
        QCOMPARE(b->value(), 8);
        first->close();
        second->close();
        QTest::qWait(5);
    }
    void textInputUsesOwnStyleAndFitsContent()
    {
        if (QGuiApplication::platformName() != "offscreen")
            QSKIP("Offscreen-only annotation case");
        QImage image(1200, 800, QImage::Format_RGB32);
        image.fill(Qt::white);
        Document document;
        document.reset(image, image.size());
        document.setSelection(QRectF(100, 100, 800, 400));
        document.textFont.setPixelSize(32);
        document.textColor = QColor("#123456");
        document.penWidth = 2;
        document.setTool(Document::Text);
        auto overlay = new Overlay(&document, QGuiApplication::primaryScreen(), true);
        const auto cleanup = qScopeGuard([&] {
            overlay->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        });
        overlay->resize(image.size());
        overlay->show();
        QTest::mouseClick(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(200, 200));
        auto input = overlay->findChild<AnnotationInput*>();
        QVERIFY(input);
        QCOMPARE(input->font().pixelSize(), 32);
        QVERIFY(!input->hasFrame());
        input->setText("Hi");
        const int shortWidth = input->width();
        input->setText("Hello world");
        QVERIFY(input->width() > shortWidth);
        QVERIFY(input->width() < 300);
        QVERIFY(document.selection.contains(input->geometry()));
        const auto preview = qEnvironmentVariable("MOKOU_INPUT_PREVIEW");
        if (!preview.isEmpty())
            QVERIFY(overlay->grab().save(preview));
        const auto baseline = input->baseline();
        QTest::keyClick(input, Qt::Key_Return);
        QCOMPARE(document.markCount(), 1);
        QImage expected = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        Mark mark;
        mark.kind = Mark::Text;
        mark.font = document.textFont;
        mark.color = document.textColor;
        mark.text = "Hello world";
        const QFontMetricsF metrics(document.textFont);
        mark.start = baseline;
        {
            QPainter painter(&expected);
            paintMark(painter, mark);
        }
        QCOMPARE(document.rendered(), expected);
    }
    void compactTextAndInlineResults()
    {
        if (QGuiApplication::platformName() != "offscreen")
            QSKIP("Offscreen-only layout case");
        QImage image(1200, 800, QImage::Format_RGB32);
        image.fill(Qt::white);
        Document document;
        document.reset(image, image.size());
        document.setSelection(QRectF(100, 100, 800, 400));
        auto overlay = new Overlay(&document, QGuiApplication::primaryScreen(), true);
        overlay->resize(image.size());
        overlay->show();
        document.setTextBlocks({ { QRectF(0, 0, .45, .045), "A short line", QString(300, QChar(u'文')) },
                                   { QRectF(0, .05, .45, .045), "Another line", "另一行" } },
            true);
        auto layer = overlay->findChild<TextOverlay*>();
        QVERIFY(layer->truncated(0));
        QVERIFY(layer->blockBounds(0).height() <= 18);
        QVERIFY(!layer->blockBounds(0).intersects(layer->blockBounds(1)));
        QCOMPARE(layer->findChildren<QScrollBar*>().size(), 0);
        const auto layoutPreview = qEnvironmentVariable("MOKOU_LAYOUT_PREVIEW");
        if (!layoutPreview.isEmpty())
            QVERIFY(overlay->grab().save(layoutPreview));
        const auto more = layer->blockBounds(0).bottomRight() - QPointF(3, 3);
        QTest::mouseClick(layer, Qt::LeftButton, Qt::NoModifier, more.toPoint());
        QVERIFY(layer->blockBounds(0).height() > 18);
        QTest::keyClick(layer, Qt::Key_A, Qt::ControlModifier);
        QTest::keyClick(layer, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QString(300, QChar(u'文')) + "\n另一行");
        QTest::keyClick(layer, Qt::Key_Escape);
        QVERIFY(layer->blockBounds(0).height() <= 18);
        document.setDescription("inline description");
        auto panel = overlay->findChild<ReadingPanel*>();
        QVERIFY(panel->isVisible());
        QCOMPARE(panel->findChild<QPlainTextEdit*>()->toPlainText(), QString("inline description"));
        QVERIFY(!panel->isWindow());
        auto viewport = overlay->findChild<QScrollArea*>("toolbarViewport");
        QVERIFY(!panel->geometry().intersects(viewport->geometry()));
        const auto resultPreview = qEnvironmentVariable("MOKOU_RESULT_PREVIEW");
        if (!resultPreview.isEmpty())
            QVERIFY(overlay->grab().save(resultPreview));
        overlay->close();
        QTest::qWait(5);
    }
    void expandedTextOwnsPointer()
    {
        if (QGuiApplication::platformName() != "offscreen")
            QSKIP("Offscreen reading hit-test");
        QWidget surface;
        surface.resize(800, 600);
        auto layer = new TextOverlay(&surface);
        layer->setResults({ { QRectF(.1, .1, .4, .04), "First line", QString(300, QChar(u'文')) },
                              { QRectF(.1, .2, .4, .04), "Second line", QString(300, QChar(u'另')) } },
            QRectF(0, 0, 800, 600), true, surface.rect(), 220);
        surface.show();
        const auto second = layer->blockBounds(1);
        QTest::mouseClick(layer, Qt::LeftButton, Qt::NoModifier,
            (layer->blockBounds(0).bottomRight() - QPointF(3, 3)).toPoint());
        const auto expanded = layer->blockBounds(0);
        const auto coveredButton = second.bottomRight() - QPointF(3, 3);
        QVERIFY(expanded.contains(coveredButton));
        QTest::mouseClick(layer, Qt::LeftButton, Qt::NoModifier, coveredButton.toPoint());
        QCOMPARE(layer->blockBounds(1), second);
        QCOMPARE(layer->blockBounds(0), expanded);
    }
    void crossBlockSelectionAndResultUpdates()
    {
        if (QGuiApplication::platformName() != "offscreen")
            QSKIP("Offscreen-only reading state");
        QImage image(1200, 800, QImage::Format_RGB32);
        image.fill(Qt::white);
        Document document;
        document.reset(image, image.size());
        document.setSelection(QRectF(100, 100, 800, 400));
        auto overlay = new Overlay(&document, QGuiApplication::primaryScreen(), true);
        const auto close = qScopeGuard([&] {
            overlay->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        });
        overlay->resize(image.size());
        overlay->show();
        document.setTextBlocks({ { QRectF(.1, .1, .3, .06), "First line", "第一行" },
            { QRectF(.1, .25, .3, .06), "Second line", "第二行" } });
        auto layer = overlay->findChild<TextOverlay*>();
        const QPoint start = (layer->blockBounds(0).topLeft() + QPointF(0, 6)).toPoint();
        const QPoint end = (layer->blockBounds(1).topRight() + QPointF(1, 6)).toPoint();
        QTest::mousePress(layer, Qt::LeftButton, Qt::NoModifier, start);
        QTest::mouseMove(layer, end);
        QTest::mouseRelease(layer, Qt::LeftButton, Qt::NoModifier, end);
        QCOMPARE(layer->selectedText(), QString("First line\nSecond line"));
        QTest::keyClick(layer, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), layer->plainText());
        QTest::keyClick(layer, Qt::Key_Right);
        QTest::keyClick(layer, Qt::Key_Right); // Stay at the end instead of wrapping to the beginning.
        QTest::keyClick(layer, Qt::Key_Left, Qt::ShiftModifier);
        QCOMPARE(layer->selectedText(), QString("e"));
        QTest::keyClick(layer, Qt::Key_Home);
        QTest::keyClick(layer, Qt::Key_Left);
        QTest::keyClick(layer, Qt::Key_Right, Qt::ShiftModifier);
        QCOMPARE(layer->selectedText(), QString("F"));
        QTest::mousePress(layer, Qt::LeftButton, Qt::NoModifier, end);
        QTest::mouseMove(layer, start);
        QTest::mouseRelease(layer, Qt::LeftButton, Qt::NoModifier, start);
        QCOMPARE(layer->selectedText(), layer->plainText());
        document.setTranslationVisible(true);
        QCOMPARE(layer->plainText(), QString("第一行\n第二行"));
        QVERIFY(layer->selectedText().isEmpty());
        QCOMPARE(document.crop(),
            image.copy(document.pixelRect(document.selection))
                .convertToFormat(QImage::Format_ARGB32_Premultiplied));
        document.setDescription("description");
        QTest::keyClick(layer, Qt::Key_A, Qt::ControlModifier);
        document.setDescription("updated description");
        QCOMPARE(layer->selectedText(), layer->plainText());
        auto panel = overlay->findChild<ReadingPanel*>();
        QVERIFY(panel->isVisible());
        document.clearTranslations();
        QVERIFY(panel->isVisible());
        document.clearDescription();
        QVERIFY(!panel->isVisible());
        // No text positions on this monitor must not create an invisible full-screen input mask.
        document.setTextBlocks({ { QRectF(2, 2, .3, .1), "Elsewhere", "" } });
        QVERIFY(!layer->isVisible());
        QList<TextBlock> unplaced;
        QStringList expected;
        for (int i = 0; i < 30; ++i) {
            const auto text = QString("unplaced line %1").arg(i);
            unplaced << TextBlock { { }, text, { } };
            expected << text;
        }
        document.setTextBlocks(unplaced);
        QCOMPARE(layer->plainText(), expected.join('\n'));
        QVERIFY(layer->isVisible());
        QVERIFY(layer->truncated(0));
        QTest::keyClick(layer, Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(layer->selectedText(), document.readableText());
    }
    void compactOptionsDoNotAcceptCapture()
    {
        if (QGuiApplication::platformName() != "offscreen")
            QSKIP("Offscreen-only options");
        QImage image(1200, 800, QImage::Format_RGB32);
        image.fill(Qt::white);
        Document document;
        document.reset(image, image.size());
        document.setSelection(QRectF(100, 100, 800, 400));
        auto overlay = new Overlay(&document, QGuiApplication::primaryScreen(), true);
        const auto close = qScopeGuard([&] {
            overlay->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        });
        overlay->resize(image.size());
        overlay->show();
        QSignalSpy saved(overlay, &Overlay::accepted), prefs(overlay, &Overlay::preferencesChanged);
        overlay->findChild<QToolButton*>("textOptionsButton")->click();
        auto options = overlay->findChild<QFrame*>("toolOptions");
        QVERIFY(options->isVisible());
        auto size = options->findChild<QSpinBox*>("textSize");
        size->setValue(36);
        QCOMPARE(document.textFont.pixelSize(), 36);
        QCOMPARE(prefs.size(), 1);
        QTest::keyClick(size, Qt::Key_Return);
        QCOMPARE(saved.size(), 0);
        QTest::keyClick(size, Qt::Key_Escape);
        QVERIFY(!options->isVisible());
        QVERIFY(overlay->isVisible());
        overlay->findChild<QToolButton*>("pickerOptionsButton")->click();
        auto zoom = options->findChild<QComboBox*>("pickerZoom");
        zoom->setCurrentIndex(zoom->findData(24));
        QCOMPARE(document.pickerZoom, 24);
        QTest::mouseClick(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(50, 50));
        QVERIFY(!options->isVisible());
    }
    void cursorUsesDesktopShapes()
    {
        if (QGuiApplication::platformName() != "offscreen")
            QSKIP("Offscreen-only cursor interaction case");
        QImage image(1200, 800, QImage::Format_RGB32);
        image.fill(Qt::white);
        Document document;
        document.reset(image, image.size());
        document.setSelection(QRectF(100, 100, 400, 300));
        auto overlay = new Overlay(&document, QGuiApplication::primaryScreen(), true);
        overlay->resize(image.size());
        overlay->show();
        QVERIFY(QTest::qWaitForWindowExposed(overlay));
        QCOMPARE(overlay->cursor().shape(), Qt::ArrowCursor);
        QTest::mouseMove(overlay, QPoint(100, 250));
        QCOMPARE(overlay->cursor().shape(), Qt::SizeHorCursor);
        for (const auto tool : { Document::Pen, Document::Line, Document::Mosaic, Document::Redact,
                 Document::Text, Document::Picker }) {
            document.setTool(tool);
            QCOMPARE(overlay->cursor().shape(), Qt::ArrowCursor);
        }
        document.setTool(Document::Select);
        QTest::mouseMove(overlay, QPoint(250, 250));
        QCOMPARE(overlay->cursor().shape(), Qt::ArrowCursor);
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(250, 250));
        QTest::mouseMove(overlay, QPoint(300, 280));
        QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(300, 280));
        QTest::mouseMove(overlay, QPoint(150, 250));
        QCOMPARE(overlay->cursor().shape(), Qt::SizeHorCursor);
        auto toolbar = overlay->findChild<QScrollArea*>("toolbarViewport");
        QVERIFY(toolbar);
        QCOMPARE(toolbar->cursor().shape(), Qt::ArrowCursor);
        document.setTool(Document::Picker);
        QApplication::clipboard()->setText("unchanged");
        QTest::mouseClick(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(1200, 500));
        QCOMPARE(QApplication::clipboard()->text(), QString("unchanged"));
        overlay->close();
        QTest::qWait(5);
    }
    void selectAdjustAnnotateAndRead()
    {
        const bool native = qEnvironmentVariableIsSet("MOKOU_NATIVE_CHECK");
        auto oldClipboard = std::make_unique<QMimeData>();
        if (native) {
            const auto old = QApplication::clipboard()->mimeData();
            for (const auto& format : old->formats())
                oldClipboard->setData(format, old->data(format));
            if (old->hasImage())
                oldClipboard->setImageData(old->imageData());
        }
        const auto restoreClipboard = qScopeGuard([&] {
            if (native)
                QApplication::clipboard()->setMimeData(oldClipboard.release());
        });
        QImage image(
            native ? QGuiApplication::primaryScreen()->size() : QSize(1680, 1000), QImage::Format_RGB32);
        image.fill(QColor("#e4efe9"));
        QPainter p(&image);
        p.setPen(QColor("#203a33"));
        QFont f = p.font();
        f.setPixelSize(44);
        p.setFont(f);
        p.drawText(QPoint(160, 270), "Mokou Screenshot · 框住这一刻");
        f.setPixelSize(24);
        p.setFont(f);
        p.drawText(QPoint(160, 330), "Capture. Annotate. Make it yours.");
        p.fillRect(QRect(160, 380, 720, 130), QColor("#fdfbf1"));
        p.drawText(QRect(180, 400, 680, 90), Qt::TextWordWrap,
            "拖动选区边框调整大小，使用画笔、直线和遮挡。识别出的文字可以在原位置拖选复制。");
        p.end();
        Document document;
        document.reset(image, image.size());
        auto overlay = new Overlay(&document, QGuiApplication::primaryScreen(), !native);
        overlay->resize(image.size());
        overlay->show();
        QVERIFY(QTest::qWaitForWindowExposed(overlay));
        if (native)
            QVERIFY(QTest::qWaitForWindowActive(overlay));
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(120, 200));
        QTest::mouseMove(overlay, QPoint(1020, 600));
        QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(1020, 600));
        QCOMPARE(document.selection, QRectF(120, 200, 900, 400));
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(1020, 600));
        QTest::mouseMove(overlay, QPoint(1240, 660));
        QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(1240, 660));
        QCOMPARE(document.selection, QRectF(120, 200, 1120, 460));
        document.setTool(Document::Line);
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(920, 300));
        QTest::mouseMove(overlay, QPoint(1060, 450));
        QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(1060, 450));
        QCOMPARE(document.markCount(), 1);
        QTest::keyClick(overlay, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(document.markCount(), 0);
        document.redo();
        QCOMPARE(document.markCount(), 1);
        QTest::keyClick(overlay, Qt::Key_M);
        QCOMPARE(document.tool, Document::Mosaic);
        overlay->findChild<QPushButton*>("brushOptionsButton")->click();
        auto mosaicOptions = overlay->findChild<QFrame*>("toolOptions");
        QVERIFY(mosaicOptions->findChildren<QPushButton*>("colorSwatch").isEmpty());
        mosaicOptions->findChild<QSpinBox*>("brushWidth")->setValue(40);
        QCOMPARE(document.mosaicWidth, 40.0);
        QCOMPARE(document.penWidth, 3.0);
        QTest::keyClick(overlay, Qt::Key_Escape); // Close brush options, keep editing.
        const auto beforeMosaic = document.rendered();
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(200, 400));
        QTest::mouseMove(overlay, QPoint(800, 400));
        QTest::mouseMove(overlay, QPoint(800, 480));
        QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(800, 480));
        QCOMPARE(document.markCount(), 2);
        QVERIFY(document.rendered() != beforeMosaic);
        const auto mosaicPreview = qEnvironmentVariable("MOKOU_MOSAIC_PREVIEW");
        if (!mosaicPreview.isEmpty())
            QVERIFY(overlay->grab().save(mosaicPreview));
        document.undo();
        QCOMPARE(document.rendered(), beforeMosaic);
        document.setTool(Document::Text);
        QTest::mouseClick(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(650, 530));
        auto annotation = overlay->findChild<AnnotationInput*>();
        QVERIFY(annotation);
        annotation->setText("文字标注");
        const auto annotationPreview = qEnvironmentVariable("MOKOU_NATIVE_INPUT_PREVIEW");
        if (!annotationPreview.isEmpty())
            QVERIFY(overlay->grab().save(annotationPreview));
        QTest::keyClick(annotation, Qt::Key_Return);
        QCOMPARE(document.markCount(), 2);
        document.undo();
        document.setTextBlocks(
            { { QRectF(.04, .2, .55, .09), "Capture. Annotate. Make it yours.",
                  "捕获、标注，留下属于你的画面。" },
                { QRectF(.04, .34, .45, .07), "Select across lines.", "可以跨行选择文字。" } },
            true);
        auto texts = overlay->findChild<TextOverlay*>();
        QCOMPARE(texts->plainText(), QString("捕获、标注，留下属于你的画面。\n可以跨行选择文字。"));
        QTest::mousePress(texts, Qt::LeftButton, Qt::NoModifier,
            (texts->blockBounds(0).topLeft() + QPointF(0, 6)).toPoint());
        QTest::mouseMove(texts, (texts->blockBounds(1).topRight() + QPointF(1, 6)).toPoint());
        QTest::mouseRelease(texts, Qt::LeftButton, Qt::NoModifier,
            (texts->blockBounds(1).topRight() + QPointF(1, 6)).toPoint());
        QCOMPARE(texts->selectedText(), texts->plainText());
        QTest::keyClick(texts, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), texts->plainText());
        const auto preview = qEnvironmentVariable("MOKOU_PREVIEW");
        if (!preview.isEmpty()) {
            QDir().mkpath(QFileInfo(preview).absolutePath());
            QVERIFY(overlay->grab().save(preview));
        }
        overlay->findChild<QToolButton*>("textOptionsButton")->click();
        auto options = overlay->findChild<QFrame*>("toolOptions");
        QVERIFY(options->isVisible());
        auto family = options->findChild<QFontComboBox*>();
        family->showPopup();
        QTRY_VERIFY_WITH_TIMEOUT(QApplication::activePopupWidget(), 1000);
        auto fontPopup = QApplication::activePopupWidget();
        QTest::keyClick(fontPopup, Qt::Key_Escape);
        QVERIFY(!QApplication::activePopupWidget());
        QVERIFY(options->isVisible());
        options->findChild<QSpinBox*>("textSize")->setValue(30);
        QCOMPARE(document.textFont.pixelSize(), 30);
        const auto optionsPreview = qEnvironmentVariable("MOKOU_OPTIONS_PREVIEW");
        if (!optionsPreview.isEmpty())
            QVERIFY(overlay->grab().save(optionsPreview));
        QTest::keyClick(options->findChild<QSpinBox*>("textSize"), Qt::Key_Escape);
        QVERIFY(!options->isVisible());
        overlay->findChild<QToolButton*>("pickerOptionsButton")->click();
        auto zoom = options->findChild<QComboBox*>("pickerZoom");
        zoom->setCurrentIndex(zoom->findData(16));
        QCOMPARE(document.pickerZoom, 16);
        QTest::keyClick(zoom, Qt::Key_Escape);
        document.setTool(Document::Picker);
        QTest::mouseMove(overlay, QPoint(1050, 500));
        const auto pickerPreview = qEnvironmentVariable("MOKOU_PICKER_PREVIEW");
        if (!pickerPreview.isEmpty())
            QVERIFY(overlay->grab().save(pickerPreview));
        document.setDescription("这是截图内的识图结果示例，可选择和复制。 ");
        auto panel = overlay->findChild<ReadingPanel*>();
        QVERIFY(panel->isVisible());
        const auto readingPreview = qEnvironmentVariable("MOKOU_READING_PREVIEW");
        if (!readingPreview.isEmpty())
            QVERIFY(overlay->grab().save(readingPreview));
        QTest::keyClick(panel->findChild<QPlainTextEdit*>(), Qt::Key_Escape);
        QVERIFY(panel->isVisible());
        QVERIFY(overlay->isVisible());
        QSignalSpy accepted(overlay, &Overlay::accepted);
        QTest::keyClick(overlay, Qt::Key_Return);
        QCOMPARE(accepted.size(), 1);
        QCOMPARE(document.crop().size(), QSize(1120, 460));
        overlay->close();
        QTest::qWait(10);
    }
};
QTEST_MAIN(EditorCheck)
#include "editor_check.moc"
