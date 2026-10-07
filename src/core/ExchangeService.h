#pragma once

#include <QString>
#include <QVector>

class QJsonDocument;

namespace grossbuch {

class Database;

// Trace d'une valeur locale écrasée lors d'une fusion : on consigne l'ancienne
// valeur quand un contenu local divergent est remplacé. Voir docs/adr/0012.
struct MergeConflict
{
    QString table; // "expenses" ou "recurring_expenses"
    QString uuid;
    QString localUpdatedAt;
    QString incomingUpdatedAt;
    QString description; // résumé lisible de ce qui a été remplacé
};

// Bilan d'une fusion.
struct MergeReport
{
    int expensesAdded = 0;
    int expensesUpdated = 0;
    int expensesDeleted = 0; // dépenses passées en tombstone par propagation
    int expensesUnchanged = 0;
    int recurringAdded = 0;
    int recurringUpdated = 0;
    int recurringUnchanged = 0;
    int skipped = 0; // lignes ignorées (catégorie inconnue, enregistrement invalide)
    QVector<MergeConflict> conflicts;
};

// Export et import du fichier d'échange JSON portable, et moteur de fusion
// ligne par ligne par uuid (« la plus récente l'emporte »). Voir docs/adr/0012.
class ExchangeService
{
public:
    // Identifiant et version du format de fichier d'échange.
    static constexpr int formatVersion = 1;
    static const char *formatName();

    explicit ExchangeService(Database &database);

    // Active la sauvegarde systématique de la base avant tout import (voir
    // docs/adr/0013). Lorsqu'un répertoire est fourni, importDocument crée une
    // sauvegarde complète puis applique la rotation avant de fusionner ; si la
    // sauvegarde échoue, l'import est abandonné. L'application fournit toujours
    // ce répertoire ; les tests de fusion l'omettent pour s'isoler.
    void setBackupDirectory(const QString &directory);

    // Sérialise toute la base (dépenses et paiements récurrents, tombstones
    // compris) en document d'échange.
    QJsonDocument exportDocument() const;

    // Écrit le document d'échange dans un fichier (format JSON indenté).
    bool exportToFile(const QString &path, QString *error = nullptr) const;

    // Fusionne un document d'échange dans la base locale et renseigne le rapport.
    bool importDocument(const QJsonDocument &document, MergeReport &report,
                        QString *error = nullptr);

    // Lit un fichier d'échange puis le fusionne.
    bool importFromFile(const QString &path, MergeReport &report, QString *error = nullptr);

private:
    Database &m_database;
    QString m_backupDirectory;
};

} // namespace grossbuch
