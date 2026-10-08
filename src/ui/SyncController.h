#pragma once

#include "core/BackupService.h"
#include "core/SyncService.h"

#include <QObject>
#include <QSettings>
#include <QString>

namespace grossbuch {

class Database;

// Pilote la synchronisation distante côté interface : persiste la configuration
// (dossier partagé, synchronisation automatique, identifiant stable de la
// machine) dans QSettings, et expose des actions import/export qui construisent
// un SyncService à la demande. L'identifiant de machine est généré une fois puis
// conservé. Voir docs/adr/0020.
class SyncController : public QObject
{
    Q_OBJECT

public:
    explicit SyncController(Database &database,
                            QString backupDirectory = BackupService::defaultBackupDirectory(),
                            QObject *parent = nullptr);

    // Vrai si un dossier partagé est configuré (non vide).
    bool isConfigured() const;

    QString sharedFolder() const;
    void setSharedFolder(const QString &folder);

    bool autoSync() const;
    void setAutoSync(bool enabled);

    // Identifiant stable de cette machine (généré au premier accès).
    QString deviceId();

    // Importe (fusionne) puis exporte l'instantané de cette machine. Émet
    // imported() si l'import a modifié la base.
    SyncService::Result importNow();
    SyncService::Result exportNow();

signals:
    // Émis après un import ayant modifié la base locale : les vues doivent se
    // rafraîchir.
    void imported(const SyncService::Result &result);

private:
    SyncService makeService();

    Database &m_database;
    QString m_backupDirectory;
    QSettings m_settings{QStringLiteral("grossbuch"), QStringLiteral("grossbuch")};
};

} // namespace grossbuch
