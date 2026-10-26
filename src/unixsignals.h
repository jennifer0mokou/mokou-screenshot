#pragma once
#include <QObject>
#include <signal.h>
class QSocketNotifier;
// The POSIX handler only writes a byte. Qt work happens on the main event loop.
class UnixSignals : public QObject
{
    Q_OBJECT
public:
    explicit UnixSignals(QObject* parent = nullptr);
    ~UnixSignals() override;
signals:
    void terminationRequested();

private:
    int descriptors[2] { -1, -1 };
    QSocketNotifier* notifier = nullptr;
    struct sigaction previousTerm { }, previousInt { };
};
