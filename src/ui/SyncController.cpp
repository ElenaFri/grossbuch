#include "ui/SyncController.h"

#include "core/Database.h"

#include <QUuid>

namespace grossbuch {

namespace {
constexpr auto kFolderKey = "sync/sharedFolder";
constexpr auto kAutoKey = "sync/auto";
constexpr auto kDeviceIdKey = "sync/deviceId";
} // namespace

SyncController::SyncController(Database &database, QString backupDirectory, QObject *parent)
    : QObject(parent), m_database(database), m_backupDirectory(std::move(backupDirectory))
{
}

bool SyncController::isConfigured() const
{
    return !sharedFolder().isEmpty();
}

QString SyncController::sharedFolder() const
{
    return m_settings.value(QLatin1String(kFolderKey)).toString();
}

void SyncController::setSharedFolder(const QString &folder)
{
    m_settings.setValue(QLatin1String(kFolderKey), folder);
}

bool SyncController::autoSync() const
{
    return m_settings.value(QLatin1String(kAutoKey), false).toBool();
}

void SyncController::setAutoSync(bool enabled)
{
    m_settings.setValue(QLatin1String(kAutoKey), enabled);
}

QString SyncController::deviceId()
{
    QString id = m_settings.value(QLatin1String(kDeviceIdKey)).toString();
    if (id.isEmpty()) {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        m_settings.setValue(QLatin1String(kDeviceIdKey), id);
    }
    return id;
}

SyncService SyncController::makeService()
{
    return SyncService(m_database, sharedFolder(), deviceId(), m_backupDirectory);
}

SyncService::Result SyncController::importNow()
{
    SyncService service = makeService();
    const SyncService::Result result = service.importFromSharedFolder();
    if (result.changed)
        emit imported(result);
    return result;
}

SyncService::Result SyncController::exportNow()
{
    SyncService service = makeService();
    return service.exportToSharedFolder();
}

} // namespace grossbuch
