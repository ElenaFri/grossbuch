#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

namespace grossbuch {

class Database;

// Sauvegardes locales de la base : copies cohérentes et horodatées via
// VACUUM INTO, politique quotidienne, rotation et restauration. Voir
// docs/adr/0013.
class BackupService
{
public:
    // Nombre de sauvegardes conservées par défaut par la rotation.
    static constexpr int defaultKeep = 30;

    // Description d'une sauvegarde existante.
    struct BackupInfo
    {
        QString path;
        QString name;
        QDateTime created; // date de modification du fichier
        qint64 sizeBytes = 0;
    };

    // Répertoire de sauvegarde standard (sous le répertoire de données de
    // l'application) : ~/.local/share/grossbuch/backups.
    static QString defaultBackupDirectory();

    BackupService(Database &database, QString backupDirectory);

    // Crée une sauvegarde cohérente horodatée (VACUUM INTO) et renvoie son
    // chemin, ou std::nullopt en cas d'échec.
    std::optional<QString> createBackup(QString *error = nullptr);

    // Crée une sauvegarde seulement si aucune ne porte déjà la date du jour.
    // Renvoie le chemin de la sauvegarde créée, ou std::nullopt si une
    // sauvegarde du jour existait déjà (ou en cas d'échec, error étant alors
    // renseigné).
    std::optional<QString> dailyBackup(QString *error = nullptr);

    // Sauvegardes présentes, de la plus récente à la plus ancienne.
    QVector<BackupInfo> listBackups() const;

    // Vrai si une sauvegarde porte la date donnée.
    bool hasBackupForDate(const QDate &date) const;

    // Supprime les sauvegardes au-delà des keep plus récentes ; renvoie le
    // nombre de fichiers supprimés.
    int rotate(int keep = defaultKeep);

    // Restaure une sauvegarde par-dessus la base vivante. Opération de fichiers
    // autonome : la connexion vivante doit être fermée au préalable. L'intégrité
    // de la sauvegarde est vérifiée avant tout remplacement ; les fichiers
    // annexes du journal (-wal, -shm) de la base vivante sont supprimés. Renvoie
    // faux (et renseigne error) si la sauvegarde est absente, corrompue, ou si
    // le remplacement échoue.
    static bool restore(const QString &livePath, const QString &backupPath,
                        QString *error = nullptr);

private:
    Database &m_database;
    QString m_directory;
};

} // namespace grossbuch
