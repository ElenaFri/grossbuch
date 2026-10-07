#include "core/DataController.h"

#include "core/Database.h"
#include "core/ExchangeService.h"

#include <QStringList>

namespace grossbuch {

DataController::DataController(Database &database, QString backupDirectory, QObject *parent)
    : QObject(parent), m_database(database), m_backupDirectory(std::move(backupDirectory))
{
}

DataController::Result DataController::exportToFile(const QString &path)
{
    ExchangeService service(m_database);
    QString error;
    if (!service.exportToFile(path, &error))
        return {false, QStringLiteral("Export impossible : %1").arg(error)};
    return {true, QStringLiteral("Données exportées vers :\n%1").arg(path)};
}

DataController::Result DataController::importFromFile(const QString &path)
{
    ExchangeService service(m_database);
    service.setBackupDirectory(m_backupDirectory);

    MergeReport report;
    QString error;
    if (!service.importFromFile(path, report, &error))
        return {false, QStringLiteral("Import impossible : %1").arg(error)};

    emit dataChanged();
    return {true, formatReport(report)};
}

QVector<BackupService::BackupInfo> DataController::backups() const
{
    return BackupService(m_database, m_backupDirectory).listBackups();
}

DataController::Result DataController::restoreFromBackup(const QString &backupPath)
{
    const QString livePath = m_database.path();
    m_database.close();

    QString error;
    if (!BackupService::restore(livePath, backupPath, &error)) {
        // Échec : on rouvre la base pour que l'application reste utilisable.
        m_database.open();
        return {false, QStringLiteral("Restauration impossible : %1").arg(error)};
    }
    return {true, QStringLiteral("La sauvegarde a été restaurée.")};
}

QString DataController::formatReport(const MergeReport &report)
{
    QStringList lines;
    lines << QStringLiteral("Import terminé.");
    lines << QStringLiteral("Dépenses : %1 ajoutée(s), %2 mise(s) à jour, %3 supprimée(s), "
                            "%4 inchangée(s).")
                 .arg(report.expensesAdded)
                 .arg(report.expensesUpdated)
                 .arg(report.expensesDeleted)
                 .arg(report.expensesUnchanged);
    lines << QStringLiteral("Paiements récurrents : %1 ajouté(s), %2 mis à jour, %3 inchangé(s).")
                 .arg(report.recurringAdded)
                 .arg(report.recurringUpdated)
                 .arg(report.recurringUnchanged);

    if (report.skipped > 0) {
        lines << QStringLiteral("%1 ligne(s) ignorée(s) (catégorie inconnue ou enregistrement "
                                "invalide).")
                     .arg(report.skipped);
    }

    if (!report.conflicts.isEmpty()) {
        lines << QString();
        lines << QStringLiteral("%1 conflit(s) : une valeur locale divergente a été remplacée "
                                "par la version plus récente.")
                     .arg(report.conflicts.size());
        for (const MergeConflict &conflict : report.conflicts)
            lines << QStringLiteral("- %1 : %2").arg(conflict.table, conflict.description);
    }

    return lines.join(QLatin1Char('\n'));
}

} // namespace grossbuch
