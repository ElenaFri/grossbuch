#pragma once

#include "core/BackupService.h"

#include <QObject>
#include <QString>
#include <QVector>

namespace grossbuch {

class Database;
struct MergeReport;

// Orchestration des actions sur les données : export vers un fichier d'échange,
// import avec fusion (sauvegarde préalable comprise) et restauration d'une
// sauvegarde. Sans dépendance à l'interface graphique : les dialogues de fichiers
// et les boîtes de message relèvent de la fenêtre principale. Produit des messages
// prêts à afficher. Voir docs/adr/0012 et docs/adr/0013.
class DataController : public QObject
{
    Q_OBJECT

public:
    // Résultat d'une action : succès ou échec, et message destiné à l'utilisateur.
    struct Result
    {
        bool ok = false;
        QString message;
    };

    explicit DataController(Database &database,
                            QString backupDirectory = BackupService::defaultBackupDirectory(),
                            QObject *parent = nullptr);

    // Exporte toute la base vers un fichier d'échange JSON.
    Result exportToFile(const QString &path);

    // Importe et fusionne un fichier d'échange. Une sauvegarde complète est créée
    // avant la fusion ; en cas d'échec de la sauvegarde, l'import est abandonné.
    // Émet dataChanged() en cas de succès.
    Result importFromFile(const QString &path);

    // Sauvegardes disponibles, de la plus récente à la plus ancienne.
    QVector<BackupService::BackupInfo> backups() const;

    // Restaure une sauvegarde par-dessus la base vivante. La connexion est fermée
    // au préalable ; en cas de succès, elle reste fermée et l'application doit
    // redémarrer. En cas d'échec, la base est rouverte pour que l'application
    // continue de fonctionner.
    Result restoreFromBackup(const QString &backupPath);

signals:
    // Émis après un import réussi : les onglets doivent se rafraîchir.
    void dataChanged();

private:
    static QString formatReport(const MergeReport &report);

    Database &m_database;
    QString m_backupDirectory;
};

} // namespace grossbuch
