#include "ocr.h"
#include "textresult.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
OcrClient::OcrClient(QObject* parent, const QString& runtime)
    : QObject(parent)
{
    root = runtime.isEmpty() ? qEnvironmentVariable("MOKOU_OCR_DIR",
                                   QCoreApplication::applicationDirPath() + "/../share/mokou-screenshot/ocr")
                             : runtime;
    deadline.setSingleShot(true);
    idle.setSingleShot(true);
    connect(&deadline, &QTimer::timeout, this, [this] { fail("本地 OCR 超时，已停止；可缩小选区后重试。"); });
    connect(&idle, &QTimer::timeout, this, &OcrClient::shutdown);
    worker.setStandardErrorFile(QProcess::nullDevice());
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.remove("MOKOU_API_KEY");
    environment.remove("OPENAI_API_KEY");
    environment.insert("OMP_NUM_THREADS", "4");
    worker.setProcessEnvironment(environment);
    connect(&worker, &QProcess::started, this, [this] {
        if (active) {
            worker.write(pending);
            pending.clear();
        }
    });
    connect(&worker, &QProcess::errorOccurred, this, [this] {
        if (active)
            fail("无法运行本地 OCR，请重新安装 OCR 运行环境。");
    });
    connect(&worker, &QProcess::finished, this, [this] {
        if (active)
            fail("本地 OCR 进程已退出，请重试或重新安装 OCR 运行环境。");
    });
    connect(&worker, &QProcess::readyReadStandardOutput, this, [this] {
        incoming += worker.readAllStandardOutput();
        if (!active) {
            incoming.clear();
            return;
        }
        if (incoming.size() > 16 * 1024 * 1024) {
            fail("OCR 结果过大，已取消。");
            return;
        }
        const int end = incoming.indexOf('\n');
        if (end < 0)
            return;
        const auto response = incoming.left(end);
        incoming.clear();
        QJsonParseError parse;
        const auto object = QJsonDocument::fromJson(response, &parse).object();
        if (parse.error != QJsonParseError::NoError || object.contains("error")) {
            fail("本地 OCR 未能完成，请缩小选区重试；若持续失败，请重新安装 OCR 运行环境。");
            return;
        }
        QString error;
        const auto blocks = TextResult::parseBlocks(QString::fromUtf8(response), &error);
        if (!error.isEmpty()) {
            fail("本地 OCR 返回了无效的文字位置。");
            return;
        }
        deadline.stop();
        active = false;
        emit busyChanged(false);
        idle.start(5 * 60 * 1000);
        emit textReady(blocks, clock.elapsed());
    });
}
OcrClient::~OcrClient()
{
    shutdown();
}
void OcrClient::request(const QImage& image, const QString& language)
{
    if (active)
        return;
    if (!QFileInfo::exists(root + "/venv/bin/python") || !QFileInfo::exists(root + "/worker.py")) {
        emit failed("本地 OCR 尚未安装，请运行 scripts/install-ocr.sh；也可在设置中主动选择视觉模型 OCR。");
        return;
    }
    if (image.isNull()) {
        emit failed("请先框选一个区域。");
        return;
    }
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG") || png.size() > 45 * 1024 * 1024) {
        emit failed("选区图片过大或无法编码，未进行 OCR。");
        return;
    }
    pending = QJsonDocument(
                  QJsonObject { { "png", QString::fromLatin1(png.toBase64()) }, { "language", language } })
                  .toJson(QJsonDocument::Compact)
        + '\n';
    incoming.clear();
    idle.stop();
    active = true;
    clock.start();
    deadline.start(60000);
    emit busyChanged(true);
    if (worker.state() == QProcess::Running) {
        worker.write(pending);
        pending.clear();
    } else
        worker.start(root + "/venv/bin/python", { "-u", root + "/worker.py" });
}
void OcrClient::shutdown()
{
    deadline.stop();
    idle.stop();
    const bool wasActive = active;
    active = false;
    pending.clear();
    incoming.clear();
    if (worker.state() != QProcess::NotRunning) {
        worker.kill();
        worker.waitForFinished(1000);
    }
    if (wasActive)
        emit busyChanged(false);
}
void OcrClient::cancel()
{
    if (active)
        shutdown();
}
void OcrClient::fail(const QString& message)
{
    shutdown();
    emit failed(message);
}
