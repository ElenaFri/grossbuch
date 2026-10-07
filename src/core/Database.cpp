#include "core/Database.h"

#include "core/SyncMeta.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>
#include <QVector>

#include <vector>

namespace grossbuch {

namespace {

// Données de pré-remplissage des catégories. Chaque catégorie porte une clé
// textuelle stable (voir docs/adr/0011) ; les sous-catégories reprennent le
// chemin de leur racine. Voir aussi README et docs/adr/0006.
struct ChildSeed
{
    const char *key;
    const char *name;
};

struct CategorySeed
{
    const char *key;
    const char *name;
    std::vector<ChildSeed> children;
};

const std::vector<CategorySeed> &categorySeeds()
{
    static const std::vector<CategorySeed> seeds = {
        {"alimentation", "Alimentation",
         {{"alimentation.courses", "Courses"}, {"alimentation.restaurants", "Restaurants"}}},
        {"vetements", "Vêtements",
         {{"vetements.adultes", "Adultes"}, {"vetements.enfants", "Enfants"}}},
        {"deplacements", "Déplacements",
         {{"deplacements.transports-en-commun", "Transports en commun"},
          {"deplacements.velo", "Vélo"},
          {"deplacements.voiture", "Voiture"},
          {"deplacements.train", "Train"}}},
        {"education", "Éducation",
         {{"education.ecole", "École"},
          {"education.loisirs", "Loisirs"},
          {"education.centres-aeres", "Centres aérés"},
          {"education.formation-continue", "Formation continue"}}},
        {"livres-et-jeux", "Livres et jeux",
         {{"livres-et-jeux.adultes", "Adultes"}, {"livres-et-jeux.enfants", "Enfants"}}},
        {"sante", "Santé",
         {{"sante.adultes", "Adultes"}, {"sante.enfants", "Enfants"}}},
        {"maison", "Maison",
         {{"maison.residence-principale", "Résidence principale"},
          {"maison.residence-secondaire", "Résidence secondaire"}}},
        {"sorties", "Sorties",
         {{"sorties.musees", "Musées"},
          {"sorties.sport", "Sport"},
          {"sorties.spectacles", "Spectacles"},
          {"sorties.babysitter", "Babysitter"}}},
        {"cadeaux-et-dons", "Cadeaux et dons", {}},
        {"voyages", "Voyages", {}},
    };
    return seeds;
}

// Attribue la clé stable à chaque catégorie déjà présente, par correspondance de
// nom et de hiérarchie (utilisé par la migration v3 sur une base existante).
bool populateCategoryKeys(QSqlDatabase &db)
{
    for (const CategorySeed &seed : categorySeeds()) {
        QSqlQuery updateRoot(db);
        updateRoot.prepare(QStringLiteral(
            "UPDATE categories SET key = ? WHERE name = ? AND parent_id IS NULL"));
        updateRoot.addBindValue(QString::fromUtf8(seed.key));
        updateRoot.addBindValue(QString::fromUtf8(seed.name));
        if (!updateRoot.exec())
            return false;

        QSqlQuery findRoot(db);
        findRoot.prepare(QStringLiteral(
            "SELECT id FROM categories WHERE name = ? AND parent_id IS NULL"));
        findRoot.addBindValue(QString::fromUtf8(seed.name));
        if (!findRoot.exec() || !findRoot.next())
            continue; // racine absente de cette base : rien à rattacher
        const int rootId = findRoot.value(0).toInt();

        for (const ChildSeed &child : seed.children) {
            QSqlQuery updateChild(db);
            updateChild.prepare(QStringLiteral(
                "UPDATE categories SET key = ? WHERE name = ? AND parent_id = ?"));
            updateChild.addBindValue(QString::fromUtf8(child.key));
            updateChild.addBindValue(QString::fromUtf8(child.name));
            updateChild.addBindValue(rootId);
            if (!updateChild.exec())
                return false;
        }
    }
    return true;
}

// Attribue un uuid et des horodatages aux lignes dépourvues d'identité
// synchronisable (lignes antérieures à la v3). Chaque uuid est unique.
bool backfillSyncColumns(QSqlDatabase &db, const QString &table, const QString &now)
{
    QSqlQuery select(db);
    if (!select.exec(QStringLiteral("SELECT id FROM %1 WHERE uuid IS NULL").arg(table)))
        return false;
    QVector<int> ids;
    while (select.next())
        ids.append(select.value(0).toInt());

    for (int id : ids) {
        QSqlQuery update(db);
        update.prepare(QStringLiteral(
            "UPDATE %1 SET uuid = ?, created_at = ?, updated_at = ? WHERE id = ?").arg(table));
        update.addBindValue(newUuid());
        update.addBindValue(now);
        update.addBindValue(now);
        update.addBindValue(id);
        if (!update.exec())
            return false;
    }
    return true;
}

} // namespace

QString Database::defaultDatabasePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("grossbuch.db"));
}

Database::Database(QString path, QString connectionName)
    : m_path(std::move(path)), m_connectionName(std::move(connectionName))
{
}

Database::~Database()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        // La connexion doit être détruite avant d'être retirée du registre.
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool Database::open()
{
    if (m_path != QStringLiteral(":memory:")) {
        const QFileInfo info(m_path);
        QDir().mkpath(info.absolutePath());
    }

    QSqlDatabase db = QSqlDatabase::contains(m_connectionName)
                          ? QSqlDatabase::database(m_connectionName, false)
                          : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(m_path);
    if (!db.open())
        return false;

    {
        QSqlQuery pragma(db);
        pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
        // Mode WAL : meilleure concurrence et robustesse aux arrêts brutaux (voir
        // docs/adr/0011). Sans effet sur une base en mémoire. Cette requête renvoie
        // une ligne de résultat : on la consomme et on la finalise (bloc dédié)
        // pour ne pas laisser de relevé actif au moment du commit des migrations.
        pragma.exec(QStringLiteral("PRAGMA journal_mode = WAL"));
        pragma.finish();
    }

    return applyMigrations() && seedCategoriesIfEmpty();
}

bool Database::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName)
           && QSqlDatabase::database(m_connectionName, false).isOpen();
}

QSqlDatabase Database::connection() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

QString Database::path() const
{
    return m_path;
}

void Database::close()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        // La connexion doit être détruite avant d'être retirée du registre.
        {
            QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
            if (db.isOpen())
                db.close();
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool Database::checkIntegrity() const
{
    QSqlQuery query(connection());
    if (!query.exec(QStringLiteral("PRAGMA integrity_check")) || !query.next())
        return false;
    return query.value(0).toString() == QStringLiteral("ok");
}

bool Database::applyMigrations()
{
    QSqlDatabase db = connection();
    QSqlQuery query(db);

    int version = 0;
    if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next())
        version = query.value(0).toInt();

    if (version >= schemaVersion)
        return true;

    if (!db.transaction())
        return false;

    // Migration vers la version 1 : schéma initial.
    if (version < 1) {
        const QStringList statements = {
            QStringLiteral("CREATE TABLE categories ("
                           "  id INTEGER PRIMARY KEY,"
                           "  name TEXT NOT NULL,"
                           "  parent_id INTEGER REFERENCES categories(id))"),
            QStringLiteral("CREATE TABLE expenses ("
                           "  id INTEGER PRIMARY KEY,"
                           "  amount INTEGER NOT NULL,"
                           "  date TEXT NOT NULL,"
                           "  label TEXT,"
                           "  category_id INTEGER NOT NULL REFERENCES categories(id))"),
            QStringLiteral("CREATE INDEX idx_expenses_date ON expenses(date)"),
            QStringLiteral("CREATE INDEX idx_expenses_category ON expenses(category_id)"),
        };
        for (const QString &statement : statements) {
            if (!query.exec(statement)) {
                db.rollback();
                return false;
            }
        }
    }

    // Migration vers la version 2 : paiements récurrents (voir docs/adr/0010).
    if (version < 2) {
        const QStringList statements = {
            QStringLiteral("CREATE TABLE recurring_expenses ("
                           "  id INTEGER PRIMARY KEY,"
                           "  amount INTEGER NOT NULL,"
                           "  label TEXT,"
                           "  category_id INTEGER NOT NULL REFERENCES categories(id),"
                           "  day_of_month INTEGER NOT NULL,"
                           "  start_year INTEGER NOT NULL,"
                           "  start_month INTEGER NOT NULL,"
                           "  active INTEGER NOT NULL DEFAULT 1,"
                           "  last_year INTEGER NOT NULL DEFAULT 0,"
                           "  last_month INTEGER NOT NULL DEFAULT 0)"),
            QStringLiteral("ALTER TABLE expenses ADD COLUMN recurring_id INTEGER "
                           "REFERENCES recurring_expenses(id)"),
            QStringLiteral("CREATE INDEX idx_expenses_recurring ON expenses(recurring_id)"),
        };
        for (const QString &statement : statements) {
            if (!query.exec(statement)) {
                db.rollback();
                return false;
            }
        }
    }

    // Migration vers la version 3 : schéma synchronisable (voir docs/adr/0011).
    // Clé stable de catégorie, identité synchronisable (uuid + horodatages +
    // marqueur de suppression) sur les dépenses et les paiements récurrents.
    if (version < 3) {
        const QStringList alters = {
            QStringLiteral("ALTER TABLE categories ADD COLUMN key TEXT"),
            QStringLiteral("ALTER TABLE expenses ADD COLUMN uuid TEXT"),
            QStringLiteral("ALTER TABLE expenses ADD COLUMN created_at TEXT"),
            QStringLiteral("ALTER TABLE expenses ADD COLUMN updated_at TEXT"),
            QStringLiteral("ALTER TABLE expenses ADD COLUMN deleted INTEGER NOT NULL DEFAULT 0"),
            QStringLiteral("ALTER TABLE recurring_expenses ADD COLUMN uuid TEXT"),
            QStringLiteral("ALTER TABLE recurring_expenses ADD COLUMN created_at TEXT"),
            QStringLiteral("ALTER TABLE recurring_expenses ADD COLUMN updated_at TEXT"),
            QStringLiteral(
                "ALTER TABLE recurring_expenses ADD COLUMN deleted INTEGER NOT NULL DEFAULT 0"),
        };
        for (const QString &statement : alters) {
            if (!query.exec(statement)) {
                db.rollback();
                return false;
            }
        }

        const QString now = nowTimestampUtc();
        if (!populateCategoryKeys(db) || !backfillSyncColumns(db, QStringLiteral("expenses"), now)
            || !backfillSyncColumns(db, QStringLiteral("recurring_expenses"), now)) {
            db.rollback();
            return false;
        }

        // L'unicité est posée par index, SQLite n'autorisant pas une colonne
        // UNIQUE ajoutée par ALTER. Elle est créée après remplissage.
        const QStringList indexes = {
            QStringLiteral("CREATE UNIQUE INDEX idx_expenses_uuid ON expenses(uuid)"),
            QStringLiteral("CREATE UNIQUE INDEX idx_recurring_uuid ON recurring_expenses(uuid)"),
            QStringLiteral("CREATE UNIQUE INDEX idx_categories_key ON categories(key)"),
        };
        for (const QString &statement : indexes) {
            if (!query.exec(statement)) {
                db.rollback();
                return false;
            }
        }
    }

    // PRAGMA user_version n'accepte pas de valeur liée : on interpole l'entier.
    if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(schemaVersion))) {
        db.rollback();
        return false;
    }

    return db.commit();
}

bool Database::seedCategoriesIfEmpty()
{
    QSqlDatabase db = connection();
    QSqlQuery count(db);
    if (!count.exec(QStringLiteral("SELECT COUNT(*) FROM categories")) || !count.next())
        return false;
    if (count.value(0).toInt() > 0)
        return true; // déjà pré-rempli

    if (!db.transaction())
        return false;

    for (const CategorySeed &seed : categorySeeds()) {
        QSqlQuery insertRoot(db);
        insertRoot.prepare(
            QStringLiteral("INSERT INTO categories(name, parent_id, key) VALUES(?, NULL, ?)"));
        insertRoot.addBindValue(QString::fromUtf8(seed.name));
        insertRoot.addBindValue(QString::fromUtf8(seed.key));
        if (!insertRoot.exec()) {
            db.rollback();
            return false;
        }

        const QVariant parentId = insertRoot.lastInsertId();
        for (const ChildSeed &child : seed.children) {
            QSqlQuery insertChild(db);
            insertChild.prepare(
                QStringLiteral("INSERT INTO categories(name, parent_id, key) VALUES(?, ?, ?)"));
            insertChild.addBindValue(QString::fromUtf8(child.name));
            insertChild.addBindValue(parentId);
            insertChild.addBindValue(QString::fromUtf8(child.key));
            if (!insertChild.exec()) {
                db.rollback();
                return false;
            }
        }
    }

    return db.commit();
}

} // namespace grossbuch
