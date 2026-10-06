#include "core/Database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

#include <vector>

namespace grossbuch {

namespace {

// Données de pré-remplissage des catégories. Voir README et docs/adr/0006.
struct CategorySeed
{
    const char *name;
    std::vector<const char *> children;
};

const std::vector<CategorySeed> &categorySeeds()
{
    static const std::vector<CategorySeed> seeds = {
        {"Alimentation", {"Courses", "Restaurants"}},
        {"Vêtements", {"Adultes", "Enfants"}},
        {"Déplacements", {"Transports en commun", "Vélo", "Voiture", "Train"}},
        {"Éducation", {"École", "Loisirs", "Centres aérés", "Formation continue"}},
        {"Livres et jeux", {"Adultes", "Enfants"}},
        {"Santé", {"Adultes", "Enfants"}},
        {"Maison", {"Résidence principale", "Résidence secondaire"}},
        {"Sorties", {"Musées", "Sport", "Spectacles", "Babysitter"}},
        {"Cadeaux et dons", {}},
        {"Voyages", {}},
    };
    return seeds;
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

    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

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
            QStringLiteral("INSERT INTO categories(name, parent_id) VALUES(?, NULL)"));
        insertRoot.addBindValue(QString::fromUtf8(seed.name));
        if (!insertRoot.exec()) {
            db.rollback();
            return false;
        }

        const QVariant parentId = insertRoot.lastInsertId();
        for (const char *child : seed.children) {
            QSqlQuery insertChild(db);
            insertChild.prepare(
                QStringLiteral("INSERT INTO categories(name, parent_id) VALUES(?, ?)"));
            insertChild.addBindValue(QString::fromUtf8(child));
            insertChild.addBindValue(parentId);
            if (!insertChild.exec()) {
                db.rollback();
                return false;
            }
        }
    }

    return db.commit();
}

} // namespace grossbuch
