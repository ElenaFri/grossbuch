#pragma once

#include <QSqlDatabase>
#include <QString>

namespace grossbuch {

// Gère la connexion SQLite : ouverture, création/migration du schéma et
// pré-remplissage des catégories au premier lancement. Voir docs/adr/0002.
class Database
{
public:
    // Version du schéma gérée par le code courant.
    static constexpr int schemaVersion = 3;

    // Chemin du fichier de base dans le répertoire de données standard de
    // l'utilisateur (ex. ~/.local/share/grossbuch/grossbuch.db).
    static QString defaultDatabasePath();

    // path peut être ":memory:" pour une base en mémoire (utile aux tests).
    explicit Database(QString path, QString connectionName = QStringLiteral("grossbuch"));
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // Ouvre la connexion, applique les migrations et pré-remplit les catégories.
    bool open();

    bool isOpen() const;
    QSqlDatabase connection() const;

    // Chemin du fichier de base géré par cette instance.
    QString path() const;

    // Ferme la connexion et la retire du registre Qt, libérant le fichier (utile
    // avant une restauration qui remplace le fichier). La base peut être
    // rouverte ensuite par open(). Voir docs/adr/0013.
    void close();

    // Contrôle d'intégrité de la base (PRAGMA integrity_check). Renvoie vrai si
    // SQLite rapporte « ok ». Utile avant une sauvegarde ou un import. Voir
    // docs/adr/0011.
    bool checkIntegrity() const;

private:
    bool applyMigrations();
    bool seedCategoriesIfEmpty();

    QString m_path;
    QString m_connectionName;
};

} // namespace grossbuch
