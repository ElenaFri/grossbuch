#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSet>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Tests unitaires du cœur métier. Chaque test part d'une base SQLite en mémoire
// fraîchement ouverte et pré-remplie (seed des catégories).
class CoreTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void seedPopulatesCategories();
    void seedHierarchyIsCorrect();
    void selectableCategories();
    void categoryById();
    void reopeningKeepsSchemaAndSeed();
    void addAndReadExpense();
    void addStoresEmptyLabelAsNull();
    void updateExpense();
    void removeExpense();
    void totalsByCategory();
    void totalsByCategoryForYear();
    void monthlyTotalsForYear();
    void monthAndYearBoundariesAreExclusive();
    void availableYears();
    void centsHaveNoRoundingError();

    void categoriesHaveStableKeys();
    void addAssignsSyncIdentity();
    void uuidsAreUnique();
    void removeIsLogicalAndKeepsTombstone();
    void deletedExpensesAreExcludedFromAggregations();
    void updateRefreshesTimestampAndRejectsDeleted();
    void integrityCheckPassesOnHealthyBase();
    void migratesV2BaseToSynchronizableSchema();
    void defaultDatabasePathIsUnderAppData();

private:
    int categoryId(const QString &name) const;

    std::unique_ptr<Database> m_db;
    std::unique_ptr<CategoryRepository> m_categories;
    std::unique_ptr<ExpenseRepository> m_expenses;
};

void CoreTest::init()
{
    m_db = std::make_unique<Database>(QStringLiteral(":memory:"), QStringLiteral("test"));
    QVERIFY(m_db->open());
    m_categories = std::make_unique<CategoryRepository>(*m_db);
    m_expenses = std::make_unique<ExpenseRepository>(*m_db);
}

void CoreTest::cleanup()
{
    m_expenses.reset();
    m_categories.reset();
    m_db.reset();
}

int CoreTest::categoryId(const QString &name) const
{
    for (const Category &category : m_categories->all()) {
        if (category.name == name)
            return category.id;
    }
    return 0;
}

void CoreTest::seedPopulatesCategories()
{
    // 10 catégories racines + 22 sous-catégories.
    QCOMPARE(m_categories->all().size(), 32);
}

void CoreTest::seedHierarchyIsCorrect()
{
    const QVector<Category> all = m_categories->all();

    auto find = [&all](const QString &name) -> std::optional<Category> {
        for (const Category &category : all) {
            if (category.name == name)
                return category;
        }
        return std::nullopt;
    };

    // Une racine avec enfants.
    const std::optional<Category> alimentation = find(QStringLiteral("Alimentation"));
    QVERIFY(alimentation.has_value());
    QVERIFY(alimentation->isRoot());

    // Une sous-catégorie doit pointer vers sa racine.
    const std::optional<Category> courses = find(QStringLiteral("Courses"));
    QVERIFY(courses.has_value());
    QVERIFY(!courses->isRoot());
    QCOMPARE(courses->parentId.value(), alimentation->id);

    // Une racine sans enfant.
    const std::optional<Category> voyages = find(QStringLiteral("Voyages"));
    QVERIFY(voyages.has_value());
    QVERIFY(voyages->isRoot());
}

void CoreTest::selectableCategories()
{
    // 22 sous-catégories + 2 racines sans enfant (Cadeaux et dons, Voyages).
    const QVector<Category> selectable = m_categories->selectable();
    QCOMPARE(selectable.size(), 24);

    bool hasVoyages = false;
    for (const Category &category : selectable) {
        if (category.name == QStringLiteral("Voyages"))
            hasVoyages = true;
        // Aucune catégorie racine avec enfant ne doit être sélectionnable.
        QVERIFY(category.name != QStringLiteral("Alimentation"));
    }
    QVERIFY(hasVoyages);
}

void CoreTest::categoryById()
{
    const int coursesId = categoryId(QStringLiteral("Courses"));
    QVERIFY(coursesId > 0);

    const std::optional<Category> found = m_categories->byId(coursesId);
    QVERIFY(found.has_value());
    QCOMPARE(found->id, coursesId);
    QCOMPARE(found->name, QStringLiteral("Courses"));
    QVERIFY(!found->isRoot());

    // Un identifiant inexistant ne renvoie rien.
    QVERIFY(!m_categories->byId(999999).has_value());
}

void CoreTest::reopeningKeepsSchemaAndSeed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("grossbuch.db"));

    // Premier cycle : ouverture d'un fichier neuf (migration + seed), puis une
    // dépense enregistrée pour vérifier la persistance.
    {
        Database db(path, QStringLiteral("reopen"));
        QVERIFY(db.open());

        CategoryRepository categories(db);
        QCOMPARE(categories.all().size(), 32);

        const QVector<Category> selectable = categories.selectable();
        QVERIFY(!selectable.isEmpty());

        ExpenseRepository expenses(db);
        Expense expense;
        expense.amountCents = 4200;
        expense.date = QDate(2025, 8, 1);
        expense.categoryId = selectable.first().id;
        QVERIFY(expenses.add(expense).has_value());
    }

    // Second cycle : réouverture du même fichier. Le seed ne doit pas se rejouer
    // (toujours 32 catégories, pas 64) et les données doivent subsister.
    {
        Database db(path, QStringLiteral("reopen"));
        QVERIFY(db.open());

        CategoryRepository categories(db);
        QCOMPARE(categories.all().size(), 32);

        ExpenseRepository expenses(db);
        const QVector<Expense> august = expenses.forMonth(2025, 8);
        QCOMPARE(august.size(), 1);
        QCOMPARE(august.first().amountCents, qint64(4200));
    }
}

void CoreTest::addAndReadExpense()
{
    Expense expense;
    expense.amountCents = 1599;
    expense.date = QDate(2025, 3, 15);
    expense.label = QStringLiteral("Marché");
    expense.categoryId = categoryId(QStringLiteral("Courses"));

    const std::optional<int> id = m_expenses->add(expense);
    QVERIFY(id.has_value());

    const QVector<Expense> march = m_expenses->forMonth(2025, 3);
    QCOMPARE(march.size(), 1);
    QCOMPARE(march.first().amountCents, qint64(1599));
    QCOMPARE(march.first().date, QDate(2025, 3, 15));
    QCOMPARE(march.first().label, QStringLiteral("Marché"));
    QCOMPARE(march.first().categoryId, expense.categoryId);

    // Une dépense d'un autre mois ne doit pas apparaître.
    QVERIFY(m_expenses->forMonth(2025, 4).isEmpty());
}

void CoreTest::addStoresEmptyLabelAsNull()
{
    Expense expense;
    expense.amountCents = 500;
    expense.date = QDate(2025, 1, 10);
    expense.categoryId = categoryId(QStringLiteral("Voyages"));

    QVERIFY(m_expenses->add(expense).has_value());

    const QVector<Expense> january = m_expenses->forMonth(2025, 1);
    QCOMPARE(january.size(), 1);
    QVERIFY(january.first().label.isEmpty());
}

void CoreTest::updateExpense()
{
    Expense expense;
    expense.amountCents = 1000;
    expense.date = QDate(2025, 5, 1);
    expense.categoryId = categoryId(QStringLiteral("Restaurants"));
    const std::optional<int> id = m_expenses->add(expense);
    QVERIFY(id.has_value());

    expense.id = *id;
    expense.amountCents = 2500;
    expense.label = QStringLiteral("Dîner");
    expense.date = QDate(2025, 5, 2);
    QVERIFY(m_expenses->update(expense));

    const QVector<Expense> may = m_expenses->forMonth(2025, 5);
    QCOMPARE(may.size(), 1);
    QCOMPARE(may.first().amountCents, qint64(2500));
    QCOMPARE(may.first().label, QStringLiteral("Dîner"));
    QCOMPARE(may.first().date, QDate(2025, 5, 2));

    // Mettre à jour une dépense inexistante échoue.
    Expense ghost;
    ghost.id = 99999;
    ghost.amountCents = 1;
    ghost.date = QDate(2025, 5, 2);
    ghost.categoryId = expense.categoryId;
    QVERIFY(!m_expenses->update(ghost));
}

void CoreTest::removeExpense()
{
    Expense expense;
    expense.amountCents = 800;
    expense.date = QDate(2025, 6, 12);
    expense.categoryId = categoryId(QStringLiteral("Vélo"));
    const std::optional<int> id = m_expenses->add(expense);
    QVERIFY(id.has_value());

    QVERIFY(m_expenses->remove(*id));
    QVERIFY(m_expenses->forMonth(2025, 6).isEmpty());

    // Supprimer une dépense déjà absente échoue.
    QVERIFY(!m_expenses->remove(*id));
}

void CoreTest::totalsByCategory()
{
    const int courses = categoryId(QStringLiteral("Courses"));
    const int restaurants = categoryId(QStringLiteral("Restaurants"));

    auto addOne = [this](qint64 cents, const QDate &date, int category) {
        Expense expense;
        expense.amountCents = cents;
        expense.date = date;
        expense.categoryId = category;
        QVERIFY(m_expenses->add(expense).has_value());
    };

    addOne(1000, QDate(2025, 3, 1), courses);
    addOne(2000, QDate(2025, 3, 10), courses);
    addOne(3000, QDate(2025, 3, 20), restaurants);
    // Dépense hors du mois ciblé : ne doit pas compter.
    addOne(9999, QDate(2025, 4, 1), courses);

    const QVector<CategoryTotal> totals = m_expenses->totalsByCategory(2025, 3);
    QCOMPARE(totals.size(), 2);

    QHash<int, qint64> byCategory;
    for (const CategoryTotal &total : totals)
        byCategory.insert(total.categoryId, total.amountCents);

    QCOMPARE(byCategory.value(courses), qint64(3000));
    QCOMPARE(byCategory.value(restaurants), qint64(3000));
}

void CoreTest::totalsByCategoryForYear()
{
    const int courses = categoryId(QStringLiteral("Courses"));
    const int restaurants = categoryId(QStringLiteral("Restaurants"));

    auto addOne = [this](qint64 cents, const QDate &date, int category) {
        Expense expense;
        expense.amountCents = cents;
        expense.date = date;
        expense.categoryId = category;
        QVERIFY(m_expenses->add(expense).has_value());
    };

    // Deux mois différents de 2025 : le total annuel cumule toute l'année.
    addOne(1000, QDate(2025, 1, 5), courses);
    addOne(2000, QDate(2025, 6, 10), courses);
    addOne(3000, QDate(2025, 11, 20), restaurants);
    // Autre année : exclue.
    addOne(9999, QDate(2024, 3, 1), courses);

    const QVector<CategoryTotal> totals = m_expenses->totalsByCategoryForYear(2025);
    QCOMPARE(totals.size(), 2);

    QHash<int, qint64> byCategory;
    for (const CategoryTotal &total : totals)
        byCategory.insert(total.categoryId, total.amountCents);

    QCOMPARE(byCategory.value(courses), qint64(3000));
    QCOMPARE(byCategory.value(restaurants), qint64(3000));
}

void CoreTest::monthlyTotalsForYear()
{
    const int train = categoryId(QStringLiteral("Train"));

    auto addOne = [this, train](qint64 cents, const QDate &date) {
        Expense expense;
        expense.amountCents = cents;
        expense.date = date;
        expense.categoryId = train;
        QVERIFY(m_expenses->add(expense).has_value());
    };

    addOne(1000, QDate(2025, 1, 5));
    addOne(500, QDate(2025, 1, 25));
    addOne(4200, QDate(2025, 3, 15));
    addOne(7000, QDate(2025, 12, 31));
    // Autre année : exclue.
    addOne(9999, QDate(2024, 3, 1));

    const std::array<qint64, 12> totals = m_expenses->monthlyTotals(2025);
    QCOMPARE(totals[0], qint64(1500)); // janvier
    QCOMPARE(totals[1], qint64(0));    // février
    QCOMPARE(totals[2], qint64(4200)); // mars
    QCOMPARE(totals[11], qint64(7000)); // décembre
}

void CoreTest::monthAndYearBoundariesAreExclusive()
{
    const int courses = categoryId(QStringLiteral("Courses"));

    auto addOne = [this, courses](qint64 cents, const QDate &date) {
        Expense expense;
        expense.amountCents = cents;
        expense.date = date;
        expense.categoryId = courses;
        QVERIFY(m_expenses->add(expense).has_value());
    };

    // Autour de la frontière mars/avril 2025.
    addOne(100, QDate(2025, 3, 1));  // premier jour de mars
    addOne(200, QDate(2025, 3, 31)); // dernier jour de mars
    addOne(400, QDate(2025, 4, 1));  // premier jour d'avril
    // Autour de la frontière décembre 2025 / janvier 2026.
    addOne(800, QDate(2025, 12, 31));
    addOne(1600, QDate(2026, 1, 1));

    // forMonth : mars contient ses deux dépenses, pas celle du 1er avril.
    const QVector<Expense> march = m_expenses->forMonth(2025, 3);
    QCOMPARE(march.size(), 2);
    QCOMPARE(march.first().date, QDate(2025, 3, 1));
    QCOMPARE(march.last().date, QDate(2025, 3, 31));

    const QVector<Expense> april = m_expenses->forMonth(2025, 4);
    QCOMPARE(april.size(), 1);
    QCOMPARE(april.first().date, QDate(2025, 4, 1));

    // totalsByCategory : le total de mars n'inclut pas le 1er avril.
    const QVector<CategoryTotal> marchTotals = m_expenses->totalsByCategory(2025, 3);
    QCOMPARE(marchTotals.size(), 1);
    QCOMPARE(marchTotals.first().amountCents, qint64(300));

    // monthlyTotals : décembre 2025 contient le 31/12 mais pas le 1er/01/2026.
    const std::array<qint64, 12> totals2025 = m_expenses->monthlyTotals(2025);
    QCOMPARE(totals2025[2], qint64(300));  // mars
    QCOMPARE(totals2025[3], qint64(400));  // avril
    QCOMPARE(totals2025[11], qint64(800)); // décembre

    const std::array<qint64, 12> totals2026 = m_expenses->monthlyTotals(2026);
    QCOMPARE(totals2026[0], qint64(1600)); // janvier 2026
}

void CoreTest::availableYears()
{
    const int gifts = categoryId(QStringLiteral("Cadeaux et dons"));

    auto addOne = [this, gifts](const QDate &date) {
        Expense expense;
        expense.amountCents = 100;
        expense.date = date;
        expense.categoryId = gifts;
        QVERIFY(m_expenses->add(expense).has_value());
    };

    QVERIFY(m_expenses->availableYears().isEmpty());

    addOne(QDate(2023, 2, 1));
    addOne(QDate(2025, 7, 1));
    addOne(QDate(2023, 9, 1)); // même année que la première

    const QVector<int> years = m_expenses->availableYears();
    QCOMPARE(years, QVector<int>({2023, 2025}));
}

void CoreTest::centsHaveNoRoundingError()
{
    const int courses = categoryId(QStringLiteral("Courses"));

    // 0,10 € + 0,20 € : en flottant, 0.1 + 0.2 != 0.3. En centiers entiers, exact.
    auto addOne = [this, courses](qint64 cents) {
        Expense expense;
        expense.amountCents = cents;
        expense.date = QDate(2025, 2, 14);
        expense.categoryId = courses;
        QVERIFY(m_expenses->add(expense).has_value());
    };

    addOne(10);
    addOne(20);

    const QVector<CategoryTotal> totals = m_expenses->totalsByCategory(2025, 2);
    QCOMPARE(totals.size(), 1);
    QCOMPARE(totals.first().amountCents, qint64(30));

    // Grand montant : vérifie l'absence de débordement sur 64 bits.
    addOne(Q_INT64_C(5000000000)); // 50 millions d'euros en centimes
    const std::array<qint64, 12> monthly = m_expenses->monthlyTotals(2025);
    QCOMPARE(monthly[1], Q_INT64_C(5000000030));
}

void CoreTest::categoriesHaveStableKeys()
{
    const QVector<Category> all = m_categories->all();
    QSet<QString> keys;
    for (const Category &category : all) {
        QVERIFY(!category.key.isEmpty());
        keys.insert(category.key);
    }
    QCOMPARE(keys.size(), all.size()); // toutes les clés sont distinctes

    const auto keyOf = [&all](const QString &name) {
        for (const Category &category : all) {
            if (category.name == name)
                return category.key;
        }
        return QString();
    };
    // Une racine, une sous-catégorie préfixée par sa racine, une racine sans enfant.
    QCOMPARE(keyOf(QStringLiteral("Alimentation")), QStringLiteral("alimentation"));
    QCOMPARE(keyOf(QStringLiteral("Courses")), QStringLiteral("alimentation.courses"));
    QCOMPARE(keyOf(QStringLiteral("Résidence principale")),
             QStringLiteral("maison.residence-principale"));
    QCOMPARE(keyOf(QStringLiteral("Voyages")), QStringLiteral("voyages"));
}

void CoreTest::addAssignsSyncIdentity()
{
    Expense expense;
    expense.amountCents = 700;
    expense.date = QDate(2025, 2, 2);
    expense.categoryId = categoryId(QStringLiteral("Courses"));
    QVERIFY(m_expenses->add(expense).has_value());

    const QVector<Expense> february = m_expenses->forMonth(2025, 2);
    QCOMPARE(february.size(), 1);
    QVERIFY(!february.first().uuid.isEmpty());
    QVERIFY(!february.first().createdAt.isEmpty());
    // À la création, création et dernière modification coïncident.
    QCOMPARE(february.first().createdAt, february.first().updatedAt);
    QVERIFY(!february.first().deleted);
}

void CoreTest::uuidsAreUnique()
{
    const int courses = categoryId(QStringLiteral("Courses"));
    auto addOne = [this, courses](const QDate &date) {
        Expense expense;
        expense.amountCents = 100;
        expense.date = date;
        expense.categoryId = courses;
        return m_expenses->add(expense);
    };
    QVERIFY(addOne(QDate(2025, 1, 1)).has_value());
    QVERIFY(addOne(QDate(2025, 1, 2)).has_value());

    const QVector<Expense> january = m_expenses->forMonth(2025, 1);
    QCOMPARE(january.size(), 2);
    QVERIFY(!january.at(0).uuid.isEmpty());
    QVERIFY(january.at(0).uuid != january.at(1).uuid);
}

void CoreTest::removeIsLogicalAndKeepsTombstone()
{
    Expense expense;
    expense.amountCents = 1000;
    expense.date = QDate(2025, 9, 3);
    expense.categoryId = categoryId(QStringLiteral("Courses"));
    const std::optional<int> id = m_expenses->add(expense);
    QVERIFY(id.has_value());

    QVERIFY(m_expenses->remove(*id));
    // Invisible aux lectures.
    QVERIFY(m_expenses->forMonth(2025, 9).isEmpty());

    // Mais la ligne subsiste, marquée supprimée (tombstone) et horodatée, pour
    // pouvoir propager la suppression lors d'une future fusion.
    QSqlQuery query(m_db->connection());
    query.prepare(QStringLiteral("SELECT deleted, updated_at FROM expenses WHERE id = ?"));
    query.addBindValue(*id);
    QVERIFY(query.exec() && query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QVERIFY(!query.value(1).toString().isEmpty());
}

void CoreTest::deletedExpensesAreExcludedFromAggregations()
{
    const int courses = categoryId(QStringLiteral("Courses"));
    auto addOne = [this, courses](qint64 cents, const QDate &date) {
        Expense expense;
        expense.amountCents = cents;
        expense.date = date;
        expense.categoryId = courses;
        return m_expenses->add(expense);
    };
    const std::optional<int> kept = addOne(1000, QDate(2025, 4, 1));
    const std::optional<int> dropped = addOne(2500, QDate(2025, 4, 2));
    QVERIFY(kept.has_value() && dropped.has_value());
    QVERIFY(m_expenses->remove(*dropped));

    // La dépense supprimée ne compte plus dans aucune vue ni agrégation.
    QCOMPARE(m_expenses->forMonth(2025, 4).size(), 1);
    const QVector<CategoryTotal> totals = m_expenses->totalsByCategory(2025, 4);
    QCOMPARE(totals.size(), 1);
    QCOMPARE(totals.first().amountCents, qint64(1000));
    QCOMPARE(m_expenses->totalsByCategoryForYear(2025).first().amountCents, qint64(1000));
    QCOMPARE(m_expenses->monthlyTotals(2025)[3], qint64(1000)); // avril
}

void CoreTest::updateRefreshesTimestampAndRejectsDeleted()
{
    Expense expense;
    expense.amountCents = 1000;
    expense.date = QDate(2025, 5, 5);
    expense.categoryId = categoryId(QStringLiteral("Courses"));
    const std::optional<int> id = m_expenses->add(expense);
    QVERIFY(id.has_value());

    // On plante un horodatage ancien pour vérifier que update() le rafraîchit, sans
    // dépendre de la résolution de l'horloge réelle.
    {
        QSqlQuery seed(m_db->connection());
        seed.prepare(QStringLiteral("UPDATE expenses SET updated_at = ? WHERE id = ?"));
        seed.addBindValue(QStringLiteral("2000-01-01T00:00:00"));
        seed.addBindValue(*id);
        QVERIFY(seed.exec());
    }
    const Expense before = m_expenses->forMonth(2025, 5).first();
    QCOMPARE(before.updatedAt, QStringLiteral("2000-01-01T00:00:00"));

    Expense edited = before;
    edited.amountCents = 1200;
    QVERIFY(m_expenses->update(edited));

    const Expense after = m_expenses->forMonth(2025, 5).first();
    QCOMPARE(after.amountCents, qint64(1200));
    QVERIFY(after.updatedAt > before.updatedAt); // la modification est horodatée
    QCOMPARE(after.createdAt, before.createdAt);  // la création ne bouge pas
    QCOMPARE(after.uuid, before.uuid);            // l'identité reste stable

    // Une dépense supprimée (tombstone) ne peut plus être modifiée.
    QVERIFY(m_expenses->remove(*id));
    Expense ghost = after;
    ghost.amountCents = 9999;
    QVERIFY(!m_expenses->update(ghost));
}

void CoreTest::integrityCheckPassesOnHealthyBase()
{
    QVERIFY(m_db->checkIntegrity());
}

void CoreTest::migratesV2BaseToSynchronizableSchema()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("v2.db"));

    // Construit à la main une base au schéma v2 (sans identité synchronisable),
    // avec un petit jeu de données réaliste et des catégories aux vrais noms.
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                                    QStringLiteral("v2build"));
        db.setDatabaseName(path);
        QVERIFY(db.open());
        QSqlQuery q(db);
        QVERIFY(q.exec(QStringLiteral(
            "CREATE TABLE categories (id INTEGER PRIMARY KEY, name TEXT NOT NULL, "
            "parent_id INTEGER REFERENCES categories(id))")));
        QVERIFY(q.exec(QStringLiteral(
            "CREATE TABLE expenses (id INTEGER PRIMARY KEY, amount INTEGER NOT NULL, "
            "date TEXT NOT NULL, label TEXT, category_id INTEGER NOT NULL, recurring_id INTEGER)")));
        QVERIFY(q.exec(QStringLiteral(
            "CREATE TABLE recurring_expenses (id INTEGER PRIMARY KEY, amount INTEGER NOT NULL, "
            "label TEXT, category_id INTEGER NOT NULL, day_of_month INTEGER NOT NULL, "
            "start_year INTEGER NOT NULL, start_month INTEGER NOT NULL, "
            "active INTEGER NOT NULL DEFAULT 1, last_year INTEGER NOT NULL DEFAULT 0, "
            "last_month INTEGER NOT NULL DEFAULT 0)")));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO categories(id, name, parent_id) VALUES(1, 'Alimentation', NULL)")));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO categories(id, name, parent_id) VALUES(2, 'Courses', 1)")));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO categories(id, name, parent_id) VALUES(3, 'Voyages', NULL)")));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO expenses(amount, date, label, category_id) "
            "VALUES(1500, '2025-03-10', 'Marché', 2)")));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO expenses(amount, date, label, category_id) "
            "VALUES(8000, '2025-07-01', NULL, 3)")));
        QVERIFY(q.exec(QStringLiteral(
            "INSERT INTO recurring_expenses(amount, label, category_id, day_of_month, "
            "start_year, start_month) VALUES(5000, 'Abonnement', 2, 5, 2025, 1)")));
        QVERIFY(q.exec(QStringLiteral("PRAGMA user_version = 2")));
        q.finish();
        db.close();
    }
    QSqlDatabase::removeDatabase(QStringLiteral("v2build"));

    QString migratedUuid;
    // Ouverture par le code courant : la migration v3 doit s'appliquer.
    {
        Database db(path, QStringLiteral("v2open"));
        QVERIFY(db.open());
        QVERIFY(db.checkIntegrity());

        // Données préservées.
        ExpenseRepository expenses(db);
        const QVector<Expense> march = expenses.forMonth(2025, 3);
        QCOMPARE(march.size(), 1);
        QCOMPARE(march.first().amountCents, qint64(1500));
        QCOMPARE(march.first().label, QStringLiteral("Marché"));
        // Identité synchronisable attribuée aux lignes existantes.
        QVERIFY(!march.first().uuid.isEmpty());
        QVERIFY(!march.first().createdAt.isEmpty());
        QVERIFY(!march.first().updatedAt.isEmpty());
        migratedUuid = march.first().uuid;

        // Clés de catégorie remplies par correspondance de nom et de hiérarchie.
        CategoryRepository categories(db);
        const QVector<Category> all = categories.all();
        QCOMPARE(all.size(), 3); // pas de re-seed : les catégories existaient déjà
        const auto keyOf = [&all](const QString &name) {
            for (const Category &category : all) {
                if (category.name == name)
                    return category.key;
            }
            return QString();
        };
        QCOMPARE(keyOf(QStringLiteral("Alimentation")), QStringLiteral("alimentation"));
        QCOMPARE(keyOf(QStringLiteral("Courses")), QStringLiteral("alimentation.courses"));
        QCOMPARE(keyOf(QStringLiteral("Voyages")), QStringLiteral("voyages"));

        // uuid uniques et présents sur les deux dépenses migrées.
        QSqlQuery check(db.connection());
        QVERIFY(check.exec(QStringLiteral(
            "SELECT COUNT(DISTINCT uuid), COUNT(*) FROM expenses")));
        QVERIFY(check.next());
        QCOMPARE(check.value(1).toInt(), 2);
        QCOMPARE(check.value(0).toInt(), 2);
    }

    // Réouverture : la migration ne se rejoue pas et ne régénère pas l'identité.
    {
        Database db(path, QStringLiteral("v2reopen"));
        QVERIFY(db.open());
        ExpenseRepository expenses(db);
        const QVector<Expense> march = expenses.forMonth(2025, 3);
        QCOMPARE(march.size(), 1);
        QCOMPARE(march.first().uuid, migratedUuid); // identité stable d'une ouverture à l'autre
    }
}

// La base par défaut est rangée sous l'emplacement de données de l'application
// (contrat d'emplacement : une seule base, au bon endroit, nommée grossbuch.db).
void CoreTest::defaultDatabasePathIsUnderAppData()
{
    const QString path = Database::defaultDatabasePath();
    QVERIFY(path.endsWith(QStringLiteral("grossbuch.db")));
    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QVERIFY(!appData.isEmpty());
    QVERIFY(path.startsWith(appData));
}

QTEST_GUILESS_MAIN(CoreTest)

#include "tst_core.moc"
