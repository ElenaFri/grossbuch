#pragma once

#include "core/ExchangeService.h"

#include <QString>

namespace grossbuch {

class Database;

// Synchronisation distante semi-automatique par dossier partagé (Syncthing,
// Nextcloud, Dropbox…). Chaque machine écrit son propre instantané
// « grossbuch-<deviceId>.json » dans le dossier partagé ; à l'import, on fusionne
// tous les instantanés des autres machines (jamais le sien). La fusion existante
// (par uuid, « la plus récente l'emporte », idempotente) fait converger les
// installations. La base vivante n'est jamais synchronisée : seul le fichier
// d'échange transite par le dossier partagé. Voir docs/adr/0020.
class SyncService
{
public:
    // Bilan d'une opération de synchronisation.
    struct Result
    {
        bool ok = false;      // l'opération s'est déroulée sans erreur bloquante
        bool skipped = false; // ignorée (dossier non configuré ou indisponible)
        bool changed = false; // la base locale a été modifiée par la fusion
        int filesProcessed = 0; // nombre d'instantanés pairs effectivement fusionnés
        MergeReport report;     // bilan cumulé des fusions
        QString message;        // message lisible destiné à la barre d'état
    };

    // sharedFolder peut être vide (synchronisation non configurée). deviceId
    // identifie l'instantané de cette machine. backupDirectory peut être vide
    // pour désactiver la sauvegarde préalable (tests) ; l'application fournit
    // toujours un répertoire réel.
    SyncService(Database &database, QString sharedFolder, QString deviceId,
                QString backupDirectory);

    // Nom de fichier conventionnel de l'instantané d'une machine.
    static QString snapshotFileName(const QString &deviceId);

    // Chemin complet de l'instantané de cette machine dans le dossier partagé.
    QString ownSnapshotPath() const;

    // Fusionne tous les instantanés pairs du dossier partagé (tous les
    // « grossbuch-*.json » sauf le sien). Une unique sauvegarde est prise avant
    // la salve d'imports. Les fichiers corrompus sont ignorés sans interrompre la
    // fusion des autres.
    Result importFromSharedFolder();

    // Écrit l'instantané de cette machine dans le dossier partagé.
    Result exportToSharedFolder();

private:
    Database &m_database;
    QString m_sharedFolder;
    QString m_deviceId;
    QString m_backupDirectory;
};

} // namespace grossbuch
