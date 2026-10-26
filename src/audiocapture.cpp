#include "audiocapture.h"
#include <QStandardPaths>
AudioCapture::AudioCapture(QObject* parent)
    : QObject(parent)
{
    deadline.setSingleShot(true);
    query.setStandardErrorFile(QProcess::nullDevice());
    encoder.setStandardOutputFile(QProcess::nullDevice());
    encoder.setStandardErrorFile(QProcess::nullDevice());
    connect(&deadline, &QTimer::timeout, this,
        [this] { fail(phase == Phase::Querying ? "系统音源查询超时。" : "录音进程响应超时。"); });
    connect(&query, &QProcess::started, this, [this] {
        if (phase == Phase::Stopping)
            query.kill();
    });
    connect(&query, &QProcess::errorOccurred, this, [this] {
        if (phase == Phase::Querying)
            fail("无法查询系统音源，请检查 pactl 和音频服务。");
    });
    connect(&query, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (phase == Phase::Stopping) {
            finish();
            return;
        }
        if (phase != Phase::Querying)
            return;
        deadline.stop();
        if (code != 0 || status != QProcess::NormalExit) {
            fail("无法取得系统默认音频输出。");
            return;
        }
        const auto sink = QString::fromUtf8(query.readAllStandardOutput()).trimmed();
        if (sink.isEmpty()) {
            fail("未找到可录制的系统音源。");
            return;
        }
        launchEncoder(sink);
    });
    connect(&encoder, &QProcess::started, this, [this] {
        if (phase == Phase::Stopping) {
            encoder.write("q\n");
            return;
        }
        if (phase != Phase::Starting)
            return;
        deadline.stop();
        phase = Phase::Recording;
        emit started();
    });
    connect(&encoder, &QProcess::errorOccurred, this, [this] {
        if (busy())
            fail("录音进程启动或运行失败。");
    });
    connect(&encoder, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        if (!busy())
            return;
        if (phase != Phase::Stopping || code != 0 || status != QProcess::NormalExit) {
            fail("录音进程异常结束。");
            return;
        }
        finish();
    });
}
AudioCapture::~AudioCapture()
{
    cancel();
}
void AudioCapture::start(const QString& path, bool system, bool mic)
{
    if (busy())
        return;
    if (!system && !mic) {
        emit failed("未选择录音来源。");
        return;
    }
    if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) {
        emit failed("录音需要 FFmpeg。");
        return;
    }
    output = path;
    systemSound = system;
    microphone = mic;
    if (systemSound) {
        phase = Phase::Querying;
        deadline.start(1500);
        query.start("pactl", { "get-default-sink" });
    } else
        launchEncoder();
}
void AudioCapture::launchEncoder(const QString& sink)
{
    QStringList args { "-hide_banner", "-loglevel", "error", "-y" };
    if (systemSound)
        args << "-thread_queue_size" << "512" << "-f" << "pulse" << "-i" << sink + ".monitor";
    if (microphone)
        args << "-thread_queue_size" << "512" << "-f" << "pulse" << "-i" << "default";
    if (systemSound && microphone)
        args << "-filter_complex" << "[0:a][1:a]amix=inputs=2:duration=longest:normalize=1[a]" << "-map"
             << "[a]";
    args << "-c:a" << "libopus" << "-b:a" << "128k" << output;
    phase = Phase::Starting;
    deadline.start(5000);
    encoder.start("ffmpeg", args);
}
void AudioCapture::stop()
{
    if (!busy() || phase == Phase::Stopping)
        return;
    const auto previous = phase;
    phase = Phase::Stopping;
    deadline.start(5000);
    if (previous == Phase::Querying)
        query.kill();
    else if (previous == Phase::Recording)
        encoder.write("q\n");
    // If the encoder is still starting, its started signal sends the stop command.
}
void AudioCapture::cancel()
{
    phase = Phase::Idle;
    deadline.stop();
    for (auto process : { &query, &encoder })
        if (process->state() != QProcess::NotRunning) {
            process->kill();
            process->waitForFinished(1000);
        }
}
void AudioCapture::fail(const QString& reason)
{
    cancel();
    emit failed(reason);
}
void AudioCapture::finish()
{
    phase = Phase::Idle;
    deadline.stop();
    emit finished();
}
