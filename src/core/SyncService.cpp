#include "core/SyncService.h"

#include "core/BackupService.h"
#include "core/Database.h"

#include <QDir>
#include <QFileInfo>

namespace grossbuch {

SyncService::SyncService(Database &database, QString sharedFolder, QString deviceId,
                         QString backupDirectory)
    : m_database(database), m_sharedFolder(std::move(sharedFolder)),
      m_deviceId(std::move(deviceId)), m_backupDirectory(std::move(backupDirectory))
{
}

QString SyncService::snapshotFileName(const QString &deviceId)
{
    return QStringLiteral("grossbuch-%1.json").arg(deviceId);
}

QString SyncService::ownSnapshotPath() const
{
    return QDir(m_sharedFolder).filePath(snapshotFileName(m_deviceId));
}

SyncService::Result SyncService::importFromSharedFolder()
{
    Result result;

    if (m_sharedFolder.isEmpty()) {
        result.ok = true;
        result.skipped = true;
        result.message = QStringLiteral("Synchronisation non configurée.");
        return result;
    }

    const QDir dir(m_sharedFolder);
    if (!dir.exists()) {
        result.ok = true;
        result.skipped = true;
        result.message = QStringLiteral("Dossier de synchronisation indisponible.");
        return result;
    }

    // Tous les instantanés présents sauf le sien.
    const QString ownName = snapshotFileName(m_deviceId);
    QStringList peers;
    const QStringList entries = dir.entryList({QStringLiteral("grossbuch-*.json")}, QDir::Files);
    for (const QString &name : entries) {
        if (name != ownName)
            peers << name;
    }

    if (peers.isEmpty()) {
        result.ok = true;
        result.changed = false;
        result.message = QStringLiteral("Aucun instantané distant à fusionner.");
        return result;
    }

    // Une unique sauvegarde avant la salve d'imports (pas une par fichier), pour
    // ne pas engorger la rotation.
    if (!m_backupDirectory.isEmpty()) {
        BackupService backup(m_database, m_backupDirectory);
        QString backupError;
        if (!backup.createBackup(&backupError).has_value()) {
            result.ok = false;
            result.message = QStringLiteral("Sauvegarde préalable impossible : %1").arg(backupError);
            return result;
        }
        backup.rotate();
    }

    // Pas de sauvegarde par fichier : elle est déjà prise ci-dessus.
    ExchangeService exchange(m_database);
    exchange.setBackupDirectory(QString());

    int corrupted = 0;
    for (const QString &name : peers) {
        const QString path = dir.filePath(name);
        MergeReport r;
        QString error;
        if (!exchange.importFromFile(path, r, &error)) {
            // Fichier corrompu ou illisible : on l'ignore et on continue.
            ++corrupted;
            continue;
        }

        ++result.filesProcessed;
        result.report.expensesAdded += r.expensesAdded;
        result.report.expensesUpdated += r.expensesUpdated;
        result.report.expensesDeleted += r.expensesDeleted;
        result.report.expensesUnchanged += r.expensesUnchanged;
        result.report.recurringAdded += r.recurringAdded;
        result.report.recurringUpdated += r.recurringUpdated;
        result.report.recurringUnchanged += r.recurringUnchanged;
        result.report.skipped += r.skipped;
        result.report.conflicts += r.conflicts;
    }

    result.ok = true;
    result.changed = (result.report.expensesAdded + result.report.expensesUpdated
                      + result.report.expensesDeleted + result.report.recurringAdded
                      + result.report.recurringUpdated)
                     > 0;

    if (corrupted > 0) {
        result.message = QStringLiteral("Synchronisation : %1 instantané(s) fusionné(s), "
                                        "%2 illisible(s) ignoré(s).")
                             .arg(result.filesProcessed)
                             .arg(corrupted);
    } else {
        result.message = QStringLiteral("Synchronisation : %1 instantané(s) fusionné(s).")
                             .arg(result.filesProcessed);
    }
    return result;
}

SyncService::Result SyncService::exportToSharedFolder()
{
    Result result;

    if (m_sharedFolder.isEmpty()) {
        result.ok = true;
        result.skipped = true;
        result.message = QStringLiteral("Synchronisation non configurée.");
        return result;
    }

    if (!QDir(m_sharedFolder).exists()) {
        result.ok = true;
        result.skipped = true;
        result.message = QStringLiteral("Dossier de synchronisation indisponible.");
        return result;
    }

    ExchangeService exchange(m_database);
    QString error;
    if (!exchange.exportToFile(ownSnapshotPath(), &error)) {
        result.ok = false;
        result.message = QStringLiteral("Export de l'instantané impossible : %1").arg(error);
        return result;
    }

    result.ok = true;
    result.message = QStringLiteral("Instantané exporté.");
    return result;
}

} // namespace grossbuch
