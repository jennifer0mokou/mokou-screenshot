#include "application.h"
#include "panels.h"
#include "textoverlay.h"
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFontComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScopeGuard>
#include <QSettings>
#include <QSpinBox>
#include <QTabWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QToolButton>
#include <QtTest>
class LifecycleCheck : public QObject
{
    Q_OBJECT
private:
    QWidget* visibleOverlay() const
    {
        for (auto widget : QApplication::topLevelWidgets())
            if (widget->objectName() == "captureOverlay" && widget->isVisible())
                return widget;
        return nullptr;
    }
    void cancel(QWidget* widget)
    {
        QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
        QCoreApplication::sendEvent(widget, &escape);
    }
private slots:
    void settingsRoundTrip()
    {
        QSettings config;
        const auto clear = qScopeGuard([&] {
            config.clear();
            config.sync();
        });
        Settings settings;
        SettingsDialog dialog(settings);
        dialog.show();
        auto tabs = dialog.findChild<QTabWidget*>("settingsTabs");
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 3);
        dialog.findChild<QComboBox*>("rapidLanguage")->setCurrentIndex(2);
        dialog.findChild<QComboBox*>("ocrBackend")->setCurrentIndex(1);
        dialog.findChild<QPlainTextEdit*>("systemPrompt")->setPlainText("custom system");
        dialog.findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Save)->click();
        QCOMPARE(dialog.result(), int(QDialog::Accepted));
        Settings loaded;
        QCOMPARE(loaded.rapidLanguage, QString("korean"));
        QVERIFY(!loaded.localOcr);
        QCOMPARE(loaded.api.systemPrompt, QString("custom system"));
        const auto preview = qEnvironmentVariable("MOKOU_SETTINGS_PREVIEW");
        if (!preview.isEmpty()) {
            SettingsDialog view(loaded);
            view.show();
            view.findChild<QTabWidget*>("settingsTabs")->setCurrentIndex(1);
            QVERIFY(view.grab().save(preview));
            QVERIFY(view.grab().save(preview + "-ocr.png"));
            view.findChild<QTabWidget*>("settingsTabs")->setCurrentIndex(2);
            view.findChild<QTabWidget*>("aiTabs")->setCurrentIndex(1);
            QVERIFY(view.grab().save(preview + "-prompts.png"));
        }
    }
    void translationUsesOcrCache_data()
    {
        QTest::addColumn<bool>("recognizeFirst");
        QTest::addColumn<bool>("unplaced");
        QTest::newRow("OCR then translate") << true << false;
        QTest::newRow("translate starts OCR") << false << false;
        QTest::newRow("unplaced text still translates") << true << true;
    }
    void translationUsesOcrCache()
    {
        QFETCH(bool, recognizeFirst);
        QFETCH(bool, unplaced);
        QTemporaryDir directory;
        QVERIFY(QDir().mkpath(directory.path() + "/venv/bin"));
        QVERIFY(QFile::link(QString::fromUtf8(MOKOU_TEST_PYTHON), directory.path() + "/venv/bin/python"));
        QFile script(directory.path() + "/worker.py");
        QVERIFY(script.open(QIODevice::WriteOnly));
        QByteArray workerScript(
            "import sys,json\nn=0\nfor line in sys.stdin:\n n+=1\n "
            "print(json.dumps({'blocks':[{'text':'line'+str(n),'bbox':[.1,.1,.5,.08]}]}),flush=True)\n");
        if (unplaced)
            workerScript.replace("[.1,.1,.5,.08]", "[10,20,30,40]");
        script.write(workerScript);
        script.close();
        const auto previous = qgetenv("MOKOU_OCR_DIR");
        const auto restore = qScopeGuard([&] {
            if (previous.isNull())
                qunsetenv("MOKOU_OCR_DIR");
            else
                qputenv("MOKOU_OCR_DIR", previous);
        });
        qputenv("MOKOU_OCR_DIR", directory.path().toUtf8());
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray incoming;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [&, socket] {
                incoming += socket->readAll();
                const int end = incoming.indexOf("\r\n\r\n");
                if (end < 0)
                    return;
                int length = 0;
                for (const auto& header : incoming.left(end).split('\n'))
                    if (header.toLower().startsWith("content-length:"))
                        length = header.mid(15).trimmed().toInt();
                if (incoming.size() - end - 4 < length)
                    return;
                const QByteArray content
                    = R"({"translations":[{"id":0,"text":"缓存翻译","bbox":[20,30,40,50]}]})";
                const QByteArray body = QJsonDocument(
                    QJsonObject { { "choices",
                        QJsonArray { QJsonObject {
                            { "message", QJsonObject { { "content", QString::fromUtf8(content) } } } } } } })
                                            .toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        QSettings settings;
        settings.setValue("api/baseUrl", QString("http://127.0.0.1:%1/v1").arg(server.serverPort()));
        settings.setValue("api/translationModel", "mock-text");
        settings.setValue("ocr/local", true);
        settings.sync();
        const auto clear = qScopeGuard([&] {
            settings.clear();
            settings.sync();
        });
        Application controller;
        controller.demo();
        auto overlay = visibleOverlay();
        QVERIFY(overlay);
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(40, 100));
        QTest::mouseMove(overlay, QPoint(500, 350));
        QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(500, 350));
        const auto click = [&](const QString& name) {
            for (auto button : overlay->findChildren<QPushButton*>())
                if (button->text() == name) {
                    button->click();
                    return true;
                }
            return false;
        };
        if (recognizeFirst) {
            QVERIFY(click("OCR"));
            QTRY_COMPARE_WITH_TIMEOUT(
                overlay->findChild<TextOverlay*>()->plainText(), QString("line1"), 2000);
            QVERIFY(overlay->findChild<TextOverlay*>()->isVisible());
        }
        QVERIFY(click("翻译"));
        QTRY_VERIFY_WITH_TIMEOUT(!incoming.isEmpty(), 3000);
        QTRY_COMPARE_WITH_TIMEOUT(overlay->findChild<TextOverlay*>()->plainText(), QString("缓存翻译"), 3000);
        QVERIFY(incoming.contains("line1"));
        QVERIFY(!incoming.contains("line2"));
        QVERIFY(!incoming.contains("image_url"));
        QVERIFY(!incoming.contains("data:image"));
        QCOMPARE(controller.Status(), QString("editing"));
        cancel(overlay);
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void inlinePreferencesPersist()
    {
        QSettings config;
        const auto clear = qScopeGuard([&] {
            config.clear();
            config.sync();
        });
        Application controller;
        controller.demo();
        auto overlay = visibleOverlay();
        QVERIFY(overlay);
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(40, 100));
        QTest::mouseMove(overlay, QPoint(500, 350));
        QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(500, 350));
        overlay->findChild<QToolButton*>("textOptionsButton")->click();
        overlay->findChild<QSpinBox*>("textSize")->setValue(36);
        overlay->findChild<QToolButton*>("pickerOptionsButton")->click();
        auto zoom = overlay->findChild<QComboBox*>("pickerZoom");
        zoom->setCurrentIndex(zoom->findData(24));
        Settings loaded;
        QCOMPARE(loaded.textFont.pixelSize(), 36);
        QCOMPARE(loaded.pickerZoom, 24);
        cancel(overlay); // Close the inline options first.
        cancel(overlay);
        QCOMPARE(controller.Status(), QString("idle"));
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void oldWindowsCannotCloseNewSession()
    {
        Application controller;
        controller.demo();
        auto first = visibleOverlay();
        QVERIFY(first);
        cancel(first);
        QCOMPARE(controller.Status(), QString("idle"));
        controller.demo();
        QVERIFY(visibleOverlay());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCOMPARE(controller.Status(), QString("editing"));
        QVERIFY(visibleOverlay());
        cancel(visibleOverlay());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void settingsCancelPendingRecording()
    {
        Application controller;
        controller.demo();
        auto overlay = visibleOverlay();
        QVERIFY(overlay);
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(40, 100));
        QTest::mouseMove(overlay, QPoint(500, 350));
        QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(500, 350));
        for (auto button : overlay->findChildren<QPushButton*>())
            if (button->text() == "录屏 ▾")
                button->click();
        for (auto button : overlay->findChildren<QPushButton*>())
            if (button->text() == "● 开始录屏") {
                button->click();
                break;
            }
        QCOMPARE(controller.Status(), QString("starting"));
        QTimer::singleShot(0, this, [&] { QCOMPARE(controller.Status(), QString("dialog")); });
        // Always close the nested event loop, including on a failed assertion.
        QTimer::singleShot(240, this, [&] {
            for (auto widget : QApplication::topLevelWidgets())
                if (auto dialog = qobject_cast<SettingsDialog*>(widget))
                    dialog->reject();
        });
        controller.ShowSettings();
        QCOMPARE(controller.Status(), QString("editing"));
        cancel(visibleOverlay());
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void canceledDelayCannotStartRecording()
    {
        Application controller;
        controller.demo();
        auto overlay = visibleOverlay();
        QVERIFY(overlay);
        QTest::mousePress(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(40, 100));
        QTest::mouseMove(overlay, QPoint(500, 350));
        QTest::mouseRelease(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(500, 350));
        bool requested = false;
        for (auto button : overlay->findChildren<QPushButton*>())
            if (button->text() == "录屏 ▾")
                button->click();
        for (auto button : overlay->findChildren<QPushButton*>())
            if (button->text() == "● 开始录屏") {
                button->click();
                requested = true;
                break;
            }
        QVERIFY(requested);
        QCOMPARE(controller.Status(), QString("starting"));
        cancel(overlay);
        QCOMPARE(controller.Status(), QString("idle"));
        QTest::qWait(250);
        QCOMPARE(controller.Status(), QString("idle"));
    }
};
int main(int argc, char** argv)
{
    QTemporaryDir sandbox;
    qputenv("XDG_CONFIG_HOME", (sandbox.path() + "/config").toUtf8());
    qputenv("XDG_DATA_HOME", (sandbox.path() + "/data").toUtf8());
    qputenv("XDG_CACHE_HOME", (sandbox.path() + "/cache").toUtf8());
    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("fedokou");
    app.setApplicationName("MokouScreenshot");
    LifecycleCheck checks;
    return QTest::qExec(&checks, argc, argv);
}
#include "lifecycle_check.moc"
