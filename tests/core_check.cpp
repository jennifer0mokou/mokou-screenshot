#include "ai.h"
#include "document.h"
#include "imageoutput.h"
#include "recognition.h"
#include "recorder.h"
#include "textresult.h"
#include <QClipboard>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>
class CoreCheck : public QObject
{
    Q_OBJECT
private slots:
    void cropAndScale()
    {
        QImage image(200, 100, QImage::Format_RGB32);
        image.fill(Qt::white);
        Document d;
        d.reset(image, QSize(100, 50));
        QVERIFY(d.crop().isNull());
        d.setSelection(QRectF(10, 5, 20, 10));
        QCOMPARE(d.crop().size(), QSize(40, 20));
        d.setSelection(QRectF(-10, -10, 30, 30));
        QCOMPARE(d.selection, QRectF(0, 0, 20, 20));
    }
    void redactAndUndo()
    {
        QImage image(200, 100, QImage::Format_RGB32);
        image.fill(Qt::white);
        Document d;
        d.reset(image, image.size());
        d.setSelection(QRectF(0, 0, 200, 100));
        Mark m;
        m.kind = Mark::Redact;
        m.start = { 20, 20 };
        m.end = { 80, 60 };
        d.addMark(m);
        QCOMPARE(d.crop().pixelColor(40, 40), QColor(Qt::black));
        d.undo();
        QCOMPARE(d.crop().pixelColor(40, 40), QColor(Qt::white));
        d.redo();
        QCOMPARE(d.crop().pixelColor(40, 40), QColor(Qt::black));
    }
    void stitch()
    {
        QImage a(20, 10, QImage::Format_RGB32);
        a.fill(Qt::red);
        QImage b(10, 30, QImage::Format_RGB32);
        b.fill(Qt::blue);
        auto vertical = Document::stitch({ a, b }, false, 2);
        QCOMPARE(vertical.size(), QSize(20, 42));
        QCOMPARE(vertical.pixelColor(5, 20), QColor(Qt::blue));
        QCOMPARE(Document::stitch({ a, b }, true).size(), QSize(30, 30));
        QVERIFY(Document::stitch({ }, false).isNull());
    }
    void mosaicKeepsEarlierRedaction()
    {
        QImage image(160, 100, QImage::Format_RGB32);
        image.fill(Qt::white);
        Document d;
        d.reset(image, image.size());
        d.setSelection(QRectF(image.rect()));
        Mark redact;
        redact.kind = Mark::Redact;
        redact.start = { 20, 20 };
        redact.end = { 140, 80 };
        d.addMark(redact);
        Mark mosaic;
        mosaic.kind = Mark::Mosaic;
        mosaic.start = { 40, 30 };
        mosaic.end = { 100, 70 };
        mosaic.width = 24;
        mosaic.path.moveTo(mosaic.start);
        mosaic.path.lineTo(mosaic.end);
        d.addMark(mosaic);
        QCOMPARE(d.crop().pixelColor(60, 50), QColor(Qt::black));
    }
    void mosaicBrushFollowsPathAndClip()
    {
        QImage image(320, 200, QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                image.setPixel(x, y, qRgb((x * 41) % 256, (y * 53) % 256, ((x + y) * 71) % 256));
        Document d;
        d.reset(image, QSize(160, 100));
        d.setSelection(QRectF(20, 20, 120, 60));
        Mark stroke;
        stroke.kind = Mark::Mosaic;
        stroke.start = { 30, 30 };
        stroke.width = 16;
        stroke.clip = d.selection;
        stroke.path.moveTo(stroke.start);
        stroke.path.lineTo(120, 30);
        stroke.path.lineTo(120, 70);
        auto preview = image.copy();
        {
            QPainter painter(&preview);
            painter.scale(2, 2);
            paintMark(painter, stroke, d.mosaicBrush());
        }
        d.addMark(stroke);
        const auto result = d.rendered();
        QCOMPARE(result, preview);
        QVERIFY(result.pixelColor(160, 60) != image.pixelColor(160, 60));
        // This pixel is inside the path's rectangle but outside the painted stroke.
        QCOMPARE(result.pixelColor(140, 120), image.pixelColor(140, 120));
        d.undo();
        QCOMPARE(d.rendered(), image);
        d.redo();
        QCOMPARE(d.rendered(), result);
        d.undo();
        stroke.start = { 22, 50 };
        stroke.width = 32;
        stroke.path = QPainterPath(stroke.start); // A click paints a round dab.
        d.addMark(stroke);
        QVERIFY(d.rendered().pixelColor(44, 100) != image.pixelColor(44, 100));
        QCOMPARE(d.rendered().pixelColor(38, 100), image.pixelColor(38, 100));
        d.setSelection(QRectF(0, 0, 160, 100));
        QCOMPARE(d.rendered().pixelColor(38, 100), image.pixelColor(38, 100));
    }
    void stitchUsesPhysicalPixels()
    {
        QImage highDpi(40, 20, QImage::Format_RGB32);
        highDpi.fill(Qt::red);
        highDpi.setDevicePixelRatio(2);
        const auto result = Document::stitch({ highDpi }, false);
        QCOMPARE(result.size(), QSize(40, 20));
        QCOMPARE(result.pixelColor(35, 15), QColor(Qt::red));
    }
    void boundedPreviewAndEmptyCrop()
    {
        QImage tall(1, 32768, QImage::Format_RGB32);
        tall.fill(Qt::red);
        const auto preview = Document::stitch({ tall }, false, 0, QSize(620, 480));
        QCOMPARE(preview.size(), QSize(1, 480));
        QCOMPARE(preview.pixelColor(0, 200), QColor(Qt::red));
        Document document;
        document.reset(tall, tall.size());
        document.setSelection(QRectF(10, 10, 20, 20));
        QVERIFY(document.crop().isNull());
    }
    void blocks()
    {
        QString error;
        auto blocks = TextResult::parseBlocks(
            R"({"blocks":[{"text":"hello","translation":"你好","bbox":[0.1,0.2,0.3,0.1]}]})", &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(blocks.size(), 1);
        QCOMPARE(blocks.first().box, QRectF(.1, .2, .3, .1));
        TextResult::parseBlocks(R"({"blocks":[{"text":"bad","bbox":[0,0,3,1]}]})", &error);
        QVERIFY(error.isEmpty());
        const auto unplaced
            = TextResult::parseBlocks(R"({"blocks":[{"text":"bad","bbox":[0,0,3,1]}]})", &error);
        QCOMPARE(unplaced.size(), 1);
        QVERIFY(unplaced.first().box.isEmpty());
    }
    void documentPublishesOneChangePerEdit()
    {
        Document document;
        QImage image(100, 100, QImage::Format_RGB32);
        image.fill(Qt::white);
        document.reset(image, image.size());
        QSignalSpy changed(&document, &Document::changed), results(&document, &Document::resultsChanged);
        Mark mark;
        mark.kind = Mark::Line;
        mark.start = { 10, 10 };
        mark.end = { 30, 30 };
        document.addMark(mark);
        document.undo();
        document.redo();
        QCOMPARE(changed.size(), 3);
        QCOMPARE(results.size(), 3);
        document.setSelection(QRectF(5, 5, 80, 80));
        QCOMPARE(changed.size(), 4);
        document.setSelection(document.selection);
        QCOMPARE(changed.size(), 4);
        document.setTextBlocks({ { QRectF(.1, .1, .3, .1), "original", "translated" } });
        QVERIFY(!document.translationVisible());
        QCOMPARE(document.readableText(), QString("original"));
        document.setTranslationVisible(true);
        QCOMPARE(document.readableText(), QString("translated"));
        document.clearTranslations();
        QCOMPARE(document.readableText(), QString("original"));
        document.setTranslationVisible(true);
        QVERIFY(!document.translationVisible());
    }
    void translationConfigurationIncludesOcrPrerequisite()
    {
        Document document;
        Settings settings;
        settings.api = { };
        settings.localOcr = false;
        settings.api.baseUrl = "http://127.0.0.1:1/v1";
        settings.api.translationModel = "text";
        RecognitionController controller(document, settings);
        QVERIFY(!controller.configured(AiClient::Translate));
        document.setTextBlocks({ { QRectF(.1, .1, .5, .1), "cached", { } } });
        QVERIFY(controller.configured(AiClient::Translate));
        QVERIFY(!controller.configured(AiClient::Ocr));
        QVERIFY(!controller.configured(AiClient::Describe));
        document.setTextBlocks({ });
        settings.localOcr = true;
        QVERIFY(controller.configured(AiClient::Translate));
    }
    void settingsInvalidateRelatedResults()
    {
        Document document;
        Settings settings;
        RecognitionController controller(document, settings);
        const QList<TextBlock> blocks { { QRectF(.1, .1, .5, .1), "original", "translation" } };
        document.setTextBlocks(blocks, true);
        document.setDescription("description");
        auto before = settings;
        settings.rapidLanguage = "french";
        controller.settingsChanged(before);
        QVERIFY(document.textBlocks().isEmpty());
        QCOMPARE(document.description(), QString("description"));
        document.setTextBlocks(blocks, true);
        before = settings;
        settings.api.describePrompt += " extra";
        controller.settingsChanged(before);
        QVERIFY(document.description().isEmpty());
        QCOMPARE(document.readableText(), QString("translation"));
        document.setDescription("description");
        before = settings;
        settings.api.baseUrl = "http://127.0.0.1:1/v1";
        controller.settingsChanged(before);
        QVERIFY(document.description().isEmpty());
        QCOMPARE(document.readableText(), QString("original"));
    }
    void coordinatesRecoverWithoutLosingText()
    {
        QString error;
        const auto mixed = TextResult::parseBlocks(
            R"({"blocks":[{"text":"edge","bbox":[-0.001,0.2,1.002,0.1]},{"text":"keep me","bbox":[0,0,999,999]}]})",
            &error, QSize(1000, 500));
        QVERIFY(error.isEmpty());
        QCOMPARE(mixed.size(), 2);
        QCOMPARE(mixed[0].box.left(), 0.0);
        QCOMPARE(mixed[0].box.right(), 1.0);
        QVERIFY(mixed[1].box.isEmpty());
        QCOMPARE(mixed[1].source, QString("keep me"));
        const auto pixels = TextResult::parseBlocks(
            R"({"coordinate_system":"pixels","box_format":"xyxy","blocks":[{"text":"pixel","bbox":[4,4,24,8]}]})",
            &error, QSize(40, 20));
        QVERIFY(error.isEmpty());
        QCOMPARE(pixels.first().box, QRectF(.1, .2, .5, .2));
        const auto thousand = TextResult::parseBlocks(
            R"({"coordinate_system":"1000","blocks":[{"text":"unit","bbox":[100,200,500,200]}]})", &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(thousand.first().box, QRectF(.1, .2, .5, .2));
        const auto missing = TextResult::parseBlocks(R"({"blocks":[{"text":"no box"}]})", &error);
        QVERIFY(error.isEmpty());
        QCOMPARE(missing.size(), 1);
        QVERIFY(missing.first().box.isEmpty());
        TextResult::parseBlocks("bad JSON", &error);
        QVERIFY(!error.isEmpty());
    }
    void saveAndClipboardMatch()
    {
        QTemporaryDir directory;
        QImage image(48, 32, QImage::Format_RGB32);
        image.fill(QColor("#9ee0bf"));
        QString path, error;
        QVERIFY(ImageOutput::saveAndCopy(image, directory.path(), &path, &error));
        QCOMPARE(QImage(path), image);
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QApplication::clipboard()->mimeData()->data("image/png"));
        QCOMPARE(QApplication::clipboard()->image(), image);
    }
    void compatibleApiTransport_data()
    {
        QTest::addColumn<QString>("contentText");
        QTest::addColumn<bool>("success");
        QTest::addColumn<bool>("translate");
        QTest::newRow("ocr") << QString(R"({"blocks":[{"text":"test","bbox":[0.1,0.2,0.3,0.4]}]})") << true
                             << false;
        QTest::newRow("ocr-bad-position")
            << QString(R"({"blocks":[{"text":"test","bbox":[10,20,30,40]}]})") << true << false;
        QTest::newRow("text-translation")
            << QString(R"({"translations":[{"id":0,"text":"测试","bbox":[10,20,30,40]}]})") << true << true;
        QTest::newRow("missing-translation") << QString(R"({"translations":[{"id":0}]})") << false << true;
        QTest::newRow("wrong-id") << QString(R"({"translations":[{"id":1,"text":"测试"}]})") << false << true;
        QTest::newRow("invalid-json") << QString("not json") << false << true;
    }
    void compatibleApiTransport()
    {
        QFETCH(QString, contentText);
        QFETCH(bool, success);
        QFETCH(bool, translate);
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray incoming;
        bool responded = false;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [&, socket] {
                incoming += socket->readAll();
                const auto headerEnd = incoming.indexOf("\r\n\r\n");
                if (headerEnd < 0 || responded)
                    return;
                const auto headers = incoming.left(headerEnd).split('\n');
                int length = 0;
                for (const auto& header : headers)
                    if (header.toLower().startsWith("content-length:"))
                        length = header.mid(15).trimmed().toInt();
                if (incoming.size() - headerEnd - 4 < length)
                    return;
                responded = true;
                const QJsonObject root { { "choices",
                    QJsonArray {
                        QJsonObject { { "message", QJsonObject { { "content", contentText } } } } } } };
                const auto body = QJsonDocument(root).toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: "
                    + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                socket->disconnectFromHost();
            });
        });
        Settings settings;
        settings.api
            = { QString("http://127.0.0.1:%1/v1").arg(server.serverPort()), "mock-vision", "test-token" };
        QImage image(40, 20, QImage::Format_RGB32);
        image.fill(Qt::green);
        AiClient client;
        QSignalSpy ready(&client, &AiClient::textReady);
        QSignalSpy errors(&client, &AiClient::failed);
        settings.api.translationModel = "mock-text";
        settings.api.systemPrompt = "custom system prompt";
        settings.api.translationPrompt = "custom translation style";
        const QList<TextBlock> original { { QRectF(.1, .2, .3, .4), "test", { } } };
        if (translate)
            client.translate(original, settings);
        else
            client.request(AiClient::Ocr, image, settings);
        QTRY_COMPARE_WITH_TIMEOUT(ready.size() + errors.size(), 1, 5000);
        QCOMPARE(ready.size(), success ? 1 : 0);
        QCOMPARE(errors.size(), success ? 0 : 1);
        QVERIFY(incoming.startsWith("POST /v1/chat/completions HTTP/1.1"));
        QVERIFY(incoming.contains("Bearer test-token"));
        const auto body = QJsonDocument::fromJson(incoming.mid(incoming.indexOf("\r\n\r\n") + 4)).object();
        QCOMPARE(
            body.value("messages").toArray().first().toObject().value("role").toString(), QString("system"));
        QCOMPARE(body.value("messages").toArray().first().toObject().value("content").toString(),
            QString("custom system prompt"));
        QCOMPARE(body.value("model").toString(), translate ? QString("mock-text") : QString("mock-vision"));
        if (translate) {
            const auto content = body.value("messages").toArray().last().toObject().value("content");
            QVERIFY(content.isString());
            QVERIFY(content.toString().contains("test"));
            QVERIFY(!incoming.contains("image_url"));
            QVERIFY(!incoming.contains("data:image"));
            if (success) {
                const auto result = qvariant_cast<QList<TextBlock>>(ready.first()[1]);
                QCOMPARE(result.first().box, original.first().box);
                QCOMPARE(result.first().source, original.first().source);
                QCOMPARE(result.first().translation, QString("测试"));
            }
            return;
        }
        const auto content = body.value("messages").toArray().last().toObject().value("content").toArray();
        QVERIFY(content[1]
                .toObject()
                .value("image_url")
                .toObject()
                .value("url")
                .toString()
                .startsWith("data:image/png;base64,"));
    }
    void destroyingActiveApiClientClosesRequest()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        Settings settings;
        settings.api = { QString("http://127.0.0.1:%1/v1").arg(server.serverPort()), "mock", { } };
        QImage image(8, 8, QImage::Format_RGB32);
        image.fill(Qt::white);
        auto client = std::make_unique<AiClient>();
        client->request(AiClient::Describe, image, settings);
        QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(), 1000);
        auto socket = server.nextPendingConnection();
        client.reset();
        QTRY_COMPARE_WITH_TIMEOUT(socket->state(), QAbstractSocket::UnconnectedState, 1000);
    }
    void staticVideoKeepsRecordedDuration_data()
    {
        QTest::addColumn<QString>("format");
        QTest::addColumn<QString>("codec");
        QTest::newRow("webm") << QString("webm") << QString("libvpx-vp9");
        QTest::newRow("mp4") << QString("mp4") << QString("libx264");
    }
    void staticVideoKeepsRecordedDuration()
    {
        QFETCH(QString, format);
        QFETCH(QString, codec);
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty()
            || QStandardPaths::findExecutable("ffprobe").isEmpty())
            QSKIP("FFmpeg tools unavailable");
        QProcess capabilities;
        capabilities.start("ffmpeg", { "-hide_banner", "-encoders" });
        QVERIFY(capabilities.waitForFinished(5000));
        if (!capabilities.readAllStandardOutput().contains(codec.toUtf8()))
            QSKIP(qPrintable("FFmpeg encoder unavailable: " + codec));
        QTemporaryDir dir;
        const auto source = dir.filePath("one-frame." + format), output = dir.filePath("held." + format);
        QProcess encoder;
        encoder.start("ffmpeg",
            { "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i",
                "color=black:size=64x64:rate=10", "-frames:v", "1", "-c:v", codec, source });
        QVERIFY(encoder.waitForFinished(5000));
        QCOMPARE(encoder.exitCode(), 0);
        QProcess mux;
        mux.start("ffmpeg",
            { "-hide_banner", "-loglevel", "error", "-y", "-i", source, "-c:v", "copy", "-bsf:v",
                Recorder::holdLastFrameFilter(2000), output });
        QVERIFY(mux.waitForFinished(5000));
        QCOMPARE(mux.exitCode(), 0);
        QProcess probe;
        probe.start("ffprobe", { "-v", "error", "-show_entries", "format=duration", "-of", "json", output });
        QVERIFY(probe.waitForFinished(5000));
        QCOMPARE(probe.exitCode(), 0);
        const auto duration = QJsonDocument::fromJson(probe.readAllStandardOutput())
                                  .object()
                                  .value("format")
                                  .toObject()
                                  .value("duration")
                                  .toString()
                                  .toDouble();
        QVERIFY(duration >= 1.99 && duration <= 2.1);
        auto hashes = [](const QString& path) {
            QProcess p;
            p.start("ffprobe",
                { "-v", "error", "-select_streams", "v:0", "-show_packets", "-show_data_hash", "sha256",
                    "-show_entries", "packet=data_hash", "-of", "json", path });
            p.waitForFinished(5000);
            return QJsonDocument::fromJson(p.readAllStandardOutput()).object().value("packets").toArray();
        };
        const auto before = hashes(source);
        QVERIFY(!before.isEmpty());
        QCOMPARE(hashes(output), before);
    }
    void endpoint()
    {
        QCOMPARE(AiClient::endpoint("http://localhost:8080/v1/").toString(),
            QString("http://localhost:8080/v1/chat/completions"));
        QCOMPARE(AiClient::endpoint("https://api.openai.com/v1/chat/completions").path(),
            QString("/v1/chat/completions"));
        QVERIFY(AiClient::endpoint("file:///etc/passwd").isEmpty());
    }
};
QTEST_MAIN(CoreCheck)
#include "core_check.moc"
