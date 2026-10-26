#include "ocr.h"
#include "overlay.h"
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QScreen>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
class OcrCheck : public QObject
{
    Q_OBJECT
    void runtime(const QString& root, const QByteArray& script)
    {
        QVERIFY(QDir().mkpath(root + "/venv/bin"));
        QVERIFY(QFile::link(QString::fromUtf8(MOKOU_TEST_PYTHON), root + "/venv/bin/python"));
        QFile file(root + "/worker.py");
        QVERIFY(file.open(QIODevice::WriteOnly));
        QVERIFY(file.write(script) > 0);
    }
    QImage image()
    {
        QImage result(1000, 340, QImage::Format_RGB32);
        result.fill(Qt::white);
        QPainter painter(&result);
        QFont font("Noto Sans CJK JP");
        font.setPixelSize(32);
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(QPoint(30, 80), "本地截图：快速识别文字");
        painter.drawText(QPoint(30, 160), "Hello world, local OCR 2026");
        painter.drawText(QPoint(30, 240), "こんにちは、日本語のテスト");
        return result;
    }
private slots:
    void reusesWorker()
    {
        QTemporaryDir directory;
        runtime(directory.path(),
            "import sys,json\nn=0\nfor line in sys.stdin:\n n+=1\n "
            "print(json.dumps({'blocks':[{'text':str(n),'bbox':[0.1,0.1,0.5,0.1]}]}),flush=True)\n");
        OcrClient client(nullptr, directory.path());
        QSignalSpy ready(&client, &OcrClient::textReady), failed(&client, &OcrClient::failed);
        const auto sample = image();
        for (int i = 1; i <= 2; ++i) {
            client.request(sample);
            QTRY_COMPARE_WITH_TIMEOUT(ready.size(), i, 2000);
            const auto blocks = qvariant_cast<QList<TextBlock>>(ready.last()[0]);
            QCOMPARE(blocks.first().source, QString::number(i));
        }
        QCOMPARE(failed.size(), 0);
        QVERIFY(!client.busy());
    }
    void cancelDiscardsOldResult()
    {
        QTemporaryDir directory;
        runtime(directory.path(),
            "import sys,json,time\nfor line in sys.stdin:\n time.sleep(.15)\n "
            "print(json.dumps({'blocks':[]}),flush=True)\n");
        OcrClient client(nullptr, directory.path());
        QSignalSpy ready(&client, &OcrClient::textReady), failed(&client, &OcrClient::failed);
        const auto sample = image();
        client.request(sample);
        client.cancel();
        QVERIFY(!client.busy());
        client.request(sample);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 2000);
        QTest::qWait(200);
        QCOMPARE(ready.size(), 1);
        QCOMPARE(failed.size(), 0);
    }
    void realLanguage_data()
    {
        QTest::addColumn<QString>("language");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("expected");
        QTest::newRow("French") << QString("french") << QString("Bonjour, français été à Paris")
                                << QString("français");
        QTest::newRow("Korean") << QString("korean") << QString("안녕하세요 한국어 테스트")
                                << QString("한국어");
        QTest::newRow("Russian") << QString("russian") << QString("Привет мир, русский текст")
                                 << QString("русский");
    }
    void realLanguage()
    {
        if (!qEnvironmentVariableIsSet("MOKOU_REAL_OCR"))
            QSKIP("Opt-in local model integration");
        QFETCH(QString, language);
        QFETCH(QString, text);
        QFETCH(QString, expected);
        QImage sample(1100, 180, QImage::Format_RGB32);
        sample.fill(Qt::white);
        {
            QPainter painter(&sample);
            QFont font("Noto Sans CJK KR");
            font.setPixelSize(38);
            painter.setFont(font);
            painter.setPen(Qt::black);
            painter.drawText(QPoint(30, 90), text);
        }
        OcrClient client;
        QSignalSpy ready(&client, &OcrClient::textReady), failed(&client, &OcrClient::failed);
        client.request(sample, language);
        QTRY_VERIFY_WITH_TIMEOUT(ready.size() + failed.size() > 0, 60000);
        QCOMPARE(failed.size(), 0);
        const auto blocks = qvariant_cast<QList<TextBlock>>(ready.first()[0]);
        QString result;
        for (const auto& block : blocks)
            result += block.source;
        QVERIFY(result.contains(expected));
        qInfo("Local OCR language integration passed: %s, %lld ms", qPrintable(language),
            ready.first()[1].toLongLong());
    }
    void realMultilingual()
    {
        if (!qEnvironmentVariableIsSet("MOKOU_REAL_OCR"))
            QSKIP("Opt-in local model integration");
        OcrClient client;
        QSignalSpy ready(&client, &OcrClient::textReady), failed(&client, &OcrClient::failed);
        const auto sample = image();
        for (int i = 1; i <= 2; ++i) {
            client.request(sample);
            QTRY_VERIFY_WITH_TIMEOUT(ready.size() + failed.size() >= i, 60000);
            QCOMPARE(failed.size(), 0);
            const auto blocks = qvariant_cast<QList<TextBlock>>(ready.last()[0]);
            QString text;
            for (const auto& block : blocks)
                text += block.source;
            QVERIFY(text.contains("本地截图"));
            QVERIFY(text.contains("Hello world"));
            QVERIFY(text.contains("こんにちは"));
            for (const auto& block : blocks) {
                QVERIFY(block.box.left() >= 0 && block.box.top() >= 0);
                QVERIFY(block.box.right() <= 1.001 && block.box.bottom() <= 1.001);
            }
            const auto preview = qEnvironmentVariable("MOKOU_OCR_PREVIEW");
            if (i == 2 && !preview.isEmpty()) {
                Document document;
                document.reset(sample, sample.size());
                document.setSelection(QRectF(QPointF(), sample.size()));
                document.setTextBlocks(blocks);
                auto overlay = new Overlay(&document, QGuiApplication::primaryScreen(), true);
                overlay->resize(1200, 800);
                overlay->show();
                QVERIFY(overlay->grab().save(preview));
                overlay->close();
                QTest::qWait(5);
            }
            qInfo("Local OCR %s: %lld ms, %lld lines", i == 1 ? "cold" : "warm", ready.last()[1].toLongLong(),
                qlonglong(blocks.size()));
        }
    }
};
QTEST_MAIN(OcrCheck)
#include "ocr_check.moc"
