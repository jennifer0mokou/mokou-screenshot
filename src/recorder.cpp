#include "recorder.h"
#include "qwayland-zkde-screencast-unstable-v1.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QWaylandClientExtensionTemplate>
#include <pipewirerecord.h>
class CastManager : public QWaylandClientExtensionTemplate<CastManager>,
                    public QtWayland::zkde_screencast_unstable_v1
{
public:
    using QtWayland::zkde_screencast_unstable_v1::version;
    CastManager()
        : QWaylandClientExtensionTemplate<CastManager>(6)
    {
        initialize();
    }
    ~CastManager() override
    {
        if (isActive())
            destroy();
    }
};
class CastStream : public QObject, public QtWayland::zkde_screencast_stream_unstable_v1
{
    Q_OBJECT
public:
    CastStream(::zkde_screencast_stream_unstable_v1* s, QObject* p)
        : QObject(p)
        , QtWayland::zkde_screencast_stream_unstable_v1(s)
    {
    }
    ~CastStream() override
    {
        if (isInitialized())
            close();
    }
    quint64 objectSerial = 0;
signals:
    void ready(uint node, quint64 serial);
    void ended();
    void failure(const QString& message);

protected:
    void zkde_screencast_stream_unstable_v1_created(uint32_t node) override
    {
        emit ready(node, objectSerial);
    }
    void zkde_screencast_stream_unstable_v1_serial(uint32_t hi, uint32_t lo) override
    {
        objectSerial = (quint64(hi) << 32) | lo;
    }
    void zkde_screencast_stream_unstable_v1_closed() override
    {
        emit ended();
    }
    void zkde_screencast_stream_unstable_v1_failed(const QString& message) override
    {
        emit failure(message);
    }
};
Recorder::Recorder(QObject* p)
    : QObject(p)
{
    deadline.setSingleShot(true);
    connect(&deadline, &QTimer::timeout, this, [this] {
        if (phase == State::Starting)
            error("录屏启动超时。");
        else if (phase == State::Finishing)
            error("录屏收尾超时，临时数据已保留。");
    });
    mux.setProcessChannelMode(QProcess::SeparateChannels);
    connect(&audio, &AudioCapture::failed, this, [this](const QString& reason) {
        if (busy())
            error(reason);
    });
    connect(&audio, &AudioCapture::finished, this, &Recorder::maybeFinalize);
    connect(&mux, &QProcess::readyReadStandardError, this, [this] {
        mux.readAllStandardError();
        // Bound a stalled mux, not the total duration of a large recording's processing.
        if (phase == State::Finishing && finalizing)
            deadline.start(60000);
    });
    connect(&mux, &QProcess::errorOccurred, this, [this] {
        if (busy())
            error("无法启动录屏封装进程。临时数据：" + videoFile);
    });
    connect(&mux, &QProcess::finished, this, [this](int code, QProcess::ExitStatus state) {
        if (!busy())
            return;
        if (code || state != QProcess::NormalExit || QFileInfo(muxFile).size() == 0) {
            error("录屏封装失败。临时画面与声音保留在：" + temporary->path());
            return;
        }
        if (!QFile::rename(muxFile, output)) {
            error("无法发布录屏文件，完整文件保留在：" + muxFile);
            return;
        }
        const auto path = output;
        release();
        emit saved(path);
    });
}
Recorder::~Recorder()
{
    if (video) {
        video->stop();
        video.reset();
    }
    audio.cancel();
    if (mux.state() != QProcess::NotRunning) {
        mux.kill();
        mux.waitForFinished(1500);
    }
    if (busy() && temporary)
        temporary->setAutoRemove(false);
}
void Recorder::start(const QRect& region, const Settings& s, bool sys, bool mic)
{
    if (busy())
        return;
    if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) {
        emit failed("录屏封装需要 FFmpeg（含 setts 位流过滤器）。");
        return;
    }
    if (region.width() < 2 || region.height() < 2) {
        emit failed("录屏选区至少需要 2 × 2 像素。");
        return;
    }
    if (!QDir().mkpath(s.videoDirectory)) {
        emit failed("无法创建录屏目录：" + s.videoDirectory);
        return;
    }
    if (!manager)
        manager = std::make_unique<CastManager>();
    if (!manager->isActive() || manager->version() < 6) {
        emit failed("当前 KWin 不提供区域录屏接口。请确认通过已安装的应用入口启动。");
        return;
    }
    video = std::make_unique<PipeWireRecord>();
    const auto encoders = video->suggestedEncoders();
    const bool mp4 = s.videoFormat != "webm";
    auto encoder = mp4 ? (encoders.contains(PipeWireRecord::H264Main) ? PipeWireRecord::H264Main
                                                                      : PipeWireRecord::H264Baseline)
                       : PipeWireRecord::VP9;
    if (!encoders.contains(encoder)) {
        video.reset();
        emit failed("所选格式的编码器不可用，请在设置中选择另一种格式或安装相应 FFmpeg 编码器。");
        return;
    }
    format = mp4 ? "mp4" : "webm";
    output = uniqueOutputPath(s.videoDirectory, format);
    temporary = std::make_unique<QTemporaryDir>(s.videoDirectory + "/.mokou-XXXXXX");
    if (!temporary->isValid()) {
        video.reset();
        temporary.reset();
        emit failed("录屏目录不可写。");
        return;
    }
    videoFile = temporary->filePath("video." + format);
    audioFile = temporary->filePath("audio.mka");
    muxFile = temporary->filePath("complete." + format);
    audioWanted = sys || mic;
    phase = State::Starting;
    finalizing = false;
    video->setEncoder(encoder);
    video->setOutput(videoFile);
    video->setMaxFramerate(s.frameRate);
    video->setQuality(80);
    video->setMaxPendingFrames(8);
    connect(video.get(), &PipeWireRecord::errorFound, this,
        [this](const QString& s) { error("录屏编码失败：" + s); });
    connect(video.get(), &PipeWireRecord::stateChanged, this, [this, sys, mic] {
        if (!busy())
            return;
        if (video->state() == PipeWireRecord::Recording && phase == State::Starting) {
            deadline.stop();
            phase = State::Recording;
            recordingTime.start();
            if (audioWanted)
                audio.start(audioFile, sys, mic);
            if (busy())
                emit started();
        }
        if (phase == State::Finishing)
            maybeFinalize();
    });
    auto raw = manager->stream_region(region.x(), region.y(), region.width(), region.height(),
        manager->version() >= 5 ? 0 : 1, CastManager::pointer_embedded);
    stream = new CastStream(raw, this);
    connect(stream, &CastStream::ready, this, [this](uint node, quint64 serial) {
        if (phase != State::Starting)
            return;
        if (!serial) {
            error("KWin 未返回录屏流序列号。");
            return;
        }
        // KPipeWire 6.7 still checks nodeId in start(), even when objectSerial is set.
        // Pass both identifiers; objectSerial prevents connecting to a reused node.
        QT_WARNING_PUSH
        QT_WARNING_DISABLE_DEPRECATED
        video->setNodeId(node);
        QT_WARNING_POP
        video->setObjectSerial(serial);
        video->start();
    });
    connect(stream, &CastStream::failure, this, [this](const QString& s) { error("KWin 录屏失败：" + s); });
    connect(stream, &CastStream::ended, this, [this] {
        if (phase == State::Recording)
            stop();
        else if (phase == State::Starting)
            error("KWin 关闭了录屏流。");
    });
    deadline.start(15000);
}
void Recorder::stop()
{
    if (phase == State::Idle || phase == State::Finishing)
        return;
    if (phase == State::Starting) {
        release();
        emit canceled();
        return;
    }
    durationMilliseconds = qMax(qint64(1), recordingTime.elapsed());
    phase = State::Finishing;
    deadline.start(30000);
    if (video)
        video->stop();
    if (phase != State::Finishing)
        return;
    emit finishing();
    audio.stop();
    maybeFinalize();
}
void Recorder::maybeFinalize()
{
    if (!busy() || phase != State::Finishing || finalizing || !video
        || video->state() != PipeWireRecord::Idle)
        return;
    if (audio.busy())
        return;
    finalizing = true;
    if (QFileInfo(videoFile).size() == 0) {
        error("录屏没有生成有效画面。");
        return;
    }
    if (audioWanted && QFileInfo(audioFile).size() == 0) {
        error("录音未生成有效声音，画面已保留在：" + videoFile);
        return;
    }
    QStringList args { "-hide_banner", "-loglevel", "error", "-progress", "pipe:2", "-nostats", "-n", "-i",
        videoFile };
    if (audioWanted)
        args << "-i" << audioFile;
    args << "-map" << "0:v:0" << "-c:v" << "copy" << "-bsf:v" << holdLastFrameFilter(durationMilliseconds);
    if (audioWanted)
        args << "-map" << "1:a:0" << "-c:a" << (format == "mp4" ? "aac" : "libopus") << "-af" << "apad"
             << "-shortest";
    else
        args << "-an";
    if (format == "mp4")
        args << "-movflags" << "+faststart";
    args << muxFile;
    mux.start("ffmpeg", args);
    deadline.start(60000);
}
QString Recorder::holdLastFrameFilter(qint64 milliseconds)
{
    // KPipeWire omits unchanged frames. Extend only the final packet, without retiming other frames
    // or re-encoding video. Explicit PTS/DTS passthrough also preserves reordered video packets.
    const auto seconds = QString::number(qMax(qint64(1), milliseconds) / 1000.0, 'f', 3);
    return "setts=pts=PTS:dts=DTS:duration='if(eq(NEXT_PTS,NOPTS),max(DURATION," + seconds
        + "/TB-PTS+STARTPTS),DURATION)'";
}
void Recorder::error(const QString& s)
{
    if (!busy()) {
        emit failed(s);
        return;
    }
    if (temporary)
        temporary->setAutoRemove(false);
    const QString detail = temporary ? "\n可恢复数据：" + temporary->path() : QString();
    release();
    emit failed(s + detail);
}
void Recorder::release()
{
    phase = State::Idle;
    deadline.stop();
    if (stream) {
        stream->disconnect(this);
        stream->deleteLater();
        stream = nullptr;
    }
    if (video) {
        auto old = video.release();
        old->disconnect(this);
        old->setParent(this);
        // The encoder may still be opening or closing its output on another thread.
        // Its destructor joins that thread before QObject::destroyed releases the directory.
        auto workspace = temporary.release();
        connect(old, &QObject::destroyed, [workspace] { delete workspace; });
        connect(old, &PipeWireRecord::stateChanged, old, [old] {
            if (old->state() == PipeWireRecord::Idle)
                old->deleteLater();
        });
        if (old->state() == PipeWireRecord::Idle)
            old->deleteLater();
        else
            old->stop();
    }
    audio.cancel();
    if (mux.state() != QProcess::NotRunning) {
        mux.kill();
        mux.waitForFinished(1000);
    }
    temporary.reset();
}
#include "recorder.moc"
