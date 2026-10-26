#include "unixsignals.h"
#include <QSocketNotifier>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
namespace
{
volatile sig_atomic_t signalFd = -1;
void signalHandler(int)
{
    const int saved = errno;
    const char byte = 1;
    if (signalFd >= 0) {
        const auto ignored = ::write(int(signalFd), &byte, 1);
        (void)ignored;
    }
    errno = saved;
}
}
UnixSignals::UnixSignals(QObject* parent)
    : QObject(parent)
{
    if (::pipe2(descriptors, O_CLOEXEC | O_NONBLOCK) != 0)
        qFatal("Cannot create termination signal pipe");
    struct sigaction handler { };
    handler.sa_handler = signalHandler;
    handler.sa_flags = SA_RESTART;
    sigemptyset(&handler.sa_mask);
    signalFd = descriptors[1];
    if (sigaction(SIGTERM, &handler, &previousTerm) != 0 || sigaction(SIGINT, &handler, &previousInt) != 0)
        qFatal("Cannot install termination signal handlers");
    notifier = new QSocketNotifier(descriptors[0], QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this, [this] {
        char bytes[64];
        bool received = false;
        for (;;) {
            const auto count = ::read(descriptors[0], bytes, sizeof bytes);
            if (count > 0)
                received = true;
            else if (count < 0 && errno == EINTR)
                continue;
            else
                break;
        }
        if (received)
            emit terminationRequested();
    });
}
UnixSignals::~UnixSignals()
{
    notifier->setEnabled(false);
    sigaction(SIGTERM, &previousTerm, nullptr);
    sigaction(SIGINT, &previousInt, nullptr);
    signalFd = -1;
    ::close(descriptors[0]);
    ::close(descriptors[1]);
}
