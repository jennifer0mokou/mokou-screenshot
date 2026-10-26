#include "shortcuts.h"
#include <KGlobalAccel>
#include <KGlobalShortcutInfo>
#include <QAction>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusReply>
bool registerPrintShortcut(QAction* action, QString* error)
{
    error->clear();
    const auto bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()) {
        *error = "用户 D-Bus 暂不可用。";
        return false;
    }
    const QDBusReply<bool> available = bus.interface()->isServiceRegistered("org.kde.kglobalaccel");
    if (!available.isValid() || !available.value()) {
        *error = "KDE 快捷键服务暂不可用。";
        return false;
    }
    const QKeySequence print(Qt::Key_Print);
    auto accelerator = KGlobalAccel::self();
    accelerator->setDefaultShortcut(action, { print }, KGlobalAccel::NoAutoloading);
    for (const auto& owner : KGlobalAccel::globalShortcutsByKey(print))
        if (owner.componentUniqueName() != action->property("componentName").toString()
            || owner.uniqueName() != action->objectName()) {
            accelerator->setShortcut(action, { }, KGlobalAccel::NoAutoloading);
            *error = "PrtScn 被其他应用占用，请在 KDE 快捷键设置中调整。";
            return false;
        }
    if (!accelerator->setShortcut(action, { print }, KGlobalAccel::NoAutoloading)) {
        *error = "无法注册 PrtScn，请检查 KDE 快捷键设置。";
        return false;
    }
    return true;
}
