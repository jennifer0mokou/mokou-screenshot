#include "capture.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QSocketNotifier>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
Capture::Capture(QObject* p)
    : QObject(p)
{
    timeout.setSingleShot(true);
    connect(&timeout, &QTimer::timeout, this, [this] { fail("桌面捕获超时，请检查 KWin 会话是否正常。"); });
}
Capture::~Capture()
{
    cleanup();
}
void Capture::cleanup()
{
    ++generation;
    timeout.stop();
    if (notifier) {
        notifier->setEnabled(false);
        notifier->deleteLater();
        notifier = nullptr;
    }
    if (fd >= 0)
        ::close(fd);
    fd = -1;
    eof = false;
    bytes.clear();
    meta.clear();
}
void Capture::cancel()
{
    cleanup();
}
void Capture::fail(const QString& s)
{
    cleanup();
    emit failed(s);
}
void Capture::start()
{
    if (busy())
        return;
    const auto requestId = ++generation;
    int pipes[2];
    if (::pipe2(pipes, O_CLOEXEC | O_NONBLOCK)) {
        emit failed("无法创建截图数据管道。");
        return;
    }
    fd = pipes[0];
    QDBusUnixFileDescriptor output(pipes[1]);
    ::close(pipes[1]);
    notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, [this] { readAvailable(); });
    auto call = QDBusMessage::createMethodCall("org.kde.KWin.ScreenShot2", "/org/kde/KWin/ScreenShot2",
        "org.kde.KWin.ScreenShot2", "CaptureWorkspace");
    QVariantMap options { { "include-cursor", false }, { "native-resolution", true } };
    call.setArguments({ options, QVariant::fromValue(output) });
    auto watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call, 15000), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, requestId] {
        QDBusPendingReply<QVariantMap> reply = *watcher;
        watcher->deleteLater();
        if (requestId != generation || !busy())
            return;
        if (reply.isError()) {
            fail("KDE 无法捕获屏幕：" + reply.error().message()
                + "\n请先运行 scripts/install.sh 注册应用权限，再从应用菜单启动。");
            return;
        }
        meta = reply.value();
        readAvailable();
        finishIfReady();
    });
    timeout.start(15000);
}
void Capture::readAvailable()
{
    if (fd < 0)
        return;
    char buf[65536];
    for (;;) {
        auto count = ::read(fd, buf, sizeof buf);
        if (count > 0) {
            bytes.append(buf, count);
            if (bytes.size() > 512 * 1024 * 1024) {
                fail("屏幕图像超过 512 MiB 限制。");
                return;
            }
        } else if (count == 0) {
            eof = true;
            if (notifier)
                notifier->setEnabled(false);
            break;
        } else {
            if (errno == EINTR)
                continue;
            if (errno != EAGAIN) {
                fail("读取截图数据失败。");
                return;
            }
            break;
        }
    }
    finishIfReady();
}
void Capture::finishIfReady()
{
    if (meta.isEmpty() || fd < 0)
        return;
    const int w = meta.value("width").toInt(), h = meta.value("height").toInt(),
              stride = meta.value("stride").toInt();
    const qint64 expected = qint64(h) * stride;
    if (w <= 0 || h <= 0 || stride <= 0 || w > 32768 || h > 32768 || expected > 512 * 1024 * 1024) {
        fail("KWin 返回了无效的图像尺寸。");
        return;
    }
    const int format = meta.value("format").toInt();
    if (format <= QImage::Format_Invalid || format >= QImage::NImageFormats) {
        fail("KWin 返回了无效的像素格式。");
        return;
    }
    const QImage row(w, 1, QImage::Format(format));
    if (row.isNull() || stride < (qint64(w) * row.depth() + 7) / 8) {
        fail("KWin 返回了无效的图像行跨度。");
        return;
    }
    if (bytes.size() < expected) {
        if (eof)
            fail("截图数据不完整，请重新截图。");
        return;
    }
    if (bytes.size() != expected) {
        fail("截图数据尺寸与元数据不一致。");
        return;
    }
    QImage result(reinterpret_cast<const uchar*>(bytes.constData()), w, h, stride,
        QImage::Format(meta.value("format").toInt()));
    result = result.copy();
    if (result.isNull()) {
        fail("无法解析 KWin 图像。");
        return;
    }
    cleanup();
    emit ready(result);
}
