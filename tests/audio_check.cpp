#include "audiocapture.h"
#include <QDir>
#include <QFile>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>
class AudioCheck : public QObject
{
    Q_OBJECT
    bool script(const QString& path, const QByteArray& body)
    {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        if (file.write("#!" + QByteArray(MUSHROOM_TEST_PYTHON) + "\n" + body) < 0)
            return false;
        file.close();
        return file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup
            | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
    }
private slots:
    void slowQueryDoesNotBlockAndCanBeStopped()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(script(dir.filePath("pactl"), "import time\ntime.sleep(10)\nprint('test')\n"));
        QVERIFY(script(dir.filePath("ffmpeg"), "raise SystemExit(0)\n"));
        const auto previous = qgetenv("PATH");
        const auto restore = qScopeGuard([&] { qputenv("PATH", previous); });
        qputenv("PATH", dir.path().toUtf8() + ":" + previous);
        AudioCapture capture;
        QSignalSpy errors(&capture, &AudioCapture::failed), done(&capture, &AudioCapture::finished);
        QElapsedTimer clock;
        clock.start();
        qint64 tick = -1;
        QTimer::singleShot(20, this, [&] { tick = clock.elapsed(); });
        capture.start(dir.filePath("audio.mka"), true, false);
        QTRY_VERIFY_WITH_TIMEOUT(tick >= 0, 500);
        QVERIFY(tick < 500);
        capture.stop();
        QTRY_COMPARE_WITH_TIMEOUT(done.size(), 1, 1000);
        QCOMPARE(errors.size(), 0);
        QVERIFY(!capture.busy());
    }
    void queryFailureEmitsOnce()
    {
        QTemporaryDir dir;
        QVERIFY(script(dir.filePath("pactl"), "raise SystemExit(3)\n"));
        QVERIFY(script(dir.filePath("ffmpeg"), "raise SystemExit(0)\n"));
        const auto previous = qgetenv("PATH");
        const auto restore = qScopeGuard([&] { qputenv("PATH", previous); });
        qputenv("PATH", dir.path().toUtf8() + ":" + previous);
        AudioCapture capture;
        QSignalSpy errors(&capture, &AudioCapture::failed), done(&capture, &AudioCapture::finished);
        capture.start(dir.filePath("audio.mka"), true, false);
        QTRY_COMPARE_WITH_TIMEOUT(errors.size(), 1, 1000);
        QVERIFY(!capture.busy());
        QCOMPARE(done.size(), 0);
    }
    void encoderStopsAndCanRestart()
    {
        QTemporaryDir dir;
        QVERIFY(script(dir.filePath("pactl"), "print('test')\n"));
        QVERIFY(script(dir.filePath("ffmpeg"),
            "import sys\nsys.stderr.write('diagnostic' * 100000)\nsys.stderr.flush()\n"
            "with open(sys.argv[-1], 'wb') as f: f.write(b'audio')\nsys.stdin.readline()\n"));
        const auto previous = qgetenv("PATH");
        const auto restore = qScopeGuard([&] { qputenv("PATH", previous); });
        qputenv("PATH", dir.path().toUtf8() + ":" + previous);
        AudioCapture capture;
        QSignalSpy ready(&capture, &AudioCapture::started), done(&capture, &AudioCapture::finished),
            errors(&capture, &AudioCapture::failed);
        for (int i = 1; i <= 2; ++i) {
            const auto path = dir.filePath(QString("audio%1.mka").arg(i));
            capture.start(path, true, false);
            QTRY_COMPARE_WITH_TIMEOUT(ready.size(), i, 1000);
            capture.stop();
            QTRY_COMPARE_WITH_TIMEOUT(done.size(), i, 1000);
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), QByteArray("audio"));
            QVERIFY(!capture.busy());
        }
        QCOMPARE(errors.size(), 0);
    }
};
QTEST_GUILESS_MAIN(AudioCheck)
#include "audio_check.moc"
