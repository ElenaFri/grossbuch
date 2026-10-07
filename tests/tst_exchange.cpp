#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "core/ExchangeService.h"
#include "core/RecurringExpense.h"
#include "core/RecurringRepository.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Tests du moteur d'échange et de fusion (export JSON + import « la plus récente
// l'emporte » par uuid). On simule deux machines par deux bases SQLite sur
// fichier (connexions distinctes) et on échange des instantanés. Voir
// docs/adr/0012.
class ExchangeTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void exportImportRebuildsVirginBase();
    void concurrentAddsMergeBothWays();
    void concurrentModificationLatestWinsAndLogsConflict();
    void deletionPropagatesViaTombstone();
    void reimportIsIdempotent();
    void deterministicOccurrenceUuidPreventsDuplicates();
    void unknownCategoryIsSkipped();
    void unsupportedFormatIsRejected();

private:
    static int categoryIdByKey(const CategoryRepository &categories, const QString &key);
    static int addExpense(ExpenseRepository &repo, int categoryId, qint64 cents, const QDate &date,
                          const QString &label = QString());
    // Force l'horodatage de mise à jour d'une dépense (résolution ISO à la
    // seconde : on « plante » une valeur pour rendre l'ordre déterministe).
    static void forceExpenseUpdatedAt(Database &db, int id, const QString &timestamp);

    std::unique_ptr<QTemporaryDir> m_dir;
};

void ExchangeTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void ExchangeTest::cleanup()
{
    m_dir.reset();
}

int ExchangeTest::categoryIdByKey(const CategoryRepository &categories, const QString &key)
{
    for (const Category &category : categories.all()) {
        if (category.key == key)
            return category.id;
    }
    return 0;
}

int ExchangeTest::addExpense(ExpenseRepository &repo, int categoryId, qint64 cents,
                             const QDate &date, const QString &label)
{
    Expense expense;
    expense.amountCents = cents;
    expense.date = date;
    expense.label = label;
    expense.categoryId = categoryId;
    const std::optional<int> id = repo.add(expense);
    return id.value_or(0);
}

void ExchangeTest::forceExpenseUpdatedAt(Database &db, int id, const QString &timestamp)
{
    QSqlQuery query(db.connection());
    query.prepare(QStringLiteral("UPDATE expenses SET updated_at = ? WHERE id = ?"));
    query.addBindValue(timestamp);
    query.addBindValue(id);
    QVERIFY(query.exec());
}

// Un export complet réimporté dans une base vierge doit tout reconstruire à
// l'identique, catégories résolues par clé stable.
void ExchangeTest::exportImportRebuildsVirginBase()
{
    const QString fileA = m_dir->filePath(QStringLiteral("a.db"));
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    const QString exchange = m_dir->filePath(QStringLiteral("export.json"));

    {
        Database a(fileA, QStringLiteral("rebuild-a"));
        QVERIFY(a.open());
        CategoryRepository categories(a);
        ExpenseRepository expenses(a);
        RecurringRepository recurring(a);

        const int courses = categoryIdByKey(categories, QStringLiteral("alimentation.courses"));
        const int train = categoryIdByKey(categories, QStringLiteral("deplacements.train"));
        QVERIFY(courses > 0);
        QVERIFY(train > 0);
        addExpense(expenses, courses, 1599, QDate(2025, 3, 15), QStringLiteral("Marché"));
        addExpense(expenses, train, 4200, QDate(2025, 3, 20));

        RecurringExpense model;
        model.amountCents = 5000;
        model.label = QStringLiteral("Abonnement");
        model.categoryId = train;
        model.dayOfMonth = 5;
        model.startYear = 2025;
        model.startMonth = 1;
        QVERIFY(recurring.add(model).has_value());

        ExchangeService service(a);
        QString error;
        QVERIFY2(service.exportToFile(exchange, &error), qPrintable(error));
    }

    Database b(fileB, QStringLiteral("rebuild-b"));
    QVERIFY(b.open());
    ExchangeService service(b);
    MergeReport report;
    QString error;
    QVERIFY2(service.importFromFile(exchange, report, &error), qPrintable(error));

    QCOMPARE(report.expensesAdded, 2);
    QCOMPARE(report.recurringAdded, 1);
    QCOMPARE(report.conflicts.size(), 0);

    CategoryRepository categoriesB(b);
    ExpenseRepository expensesB(b);
    RecurringRepository recurringB(b);

    const QVector<Expense> march = expensesB.forMonth(2025, 3);
    QCOMPARE(march.size(), 2);

    // La catégorie est résolue par clé : la dépense « Marché » doit pointer vers
    // la sous-catégorie Courses, quel que soit l'identifiant local.
    const int coursesB = categoryIdByKey(categoriesB, QStringLiteral("alimentation.courses"));
    bool marcheFound = false;
    for (const Expense &expense : march) {
        if (expense.label == QStringLiteral("Marché")) {
            marcheFound = true;
            QCOMPARE(expense.categoryId, coursesB);
            QCOMPARE(expense.amountCents, qint64(1599));
        }
    }
    QVERIFY(marcheFound);

    const QVector<RecurringExpense> models = recurringB.all();
    QCOMPARE(models.size(), 1);
    QCOMPARE(models.first().amountCents, qint64(5000));
    QCOMPARE(models.first().label, QStringLiteral("Abonnement"));
}

// Deux machines ajoutent chacune une dépense distincte ; après échange croisé des
// instantanés, chaque base contient les deux dépenses.
void ExchangeTest::concurrentAddsMergeBothWays()
{
    const QString fileA = m_dir->filePath(QStringLiteral("a.db"));
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    const QString fromA = m_dir->filePath(QStringLiteral("from-a.json"));
    const QString fromB = m_dir->filePath(QStringLiteral("from-b.json"));

    Database a(fileA, QStringLiteral("adds-a"));
    Database b(fileB, QStringLiteral("adds-b"));
    QVERIFY(a.open());
    QVERIFY(b.open());

    CategoryRepository catA(a);
    CategoryRepository catB(b);
    ExpenseRepository expA(a);
    ExpenseRepository expB(b);

    const int coursesA = categoryIdByKey(catA, QStringLiteral("alimentation.courses"));
    const int coursesB = categoryIdByKey(catB, QStringLiteral("alimentation.courses"));
    addExpense(expA, coursesA, 1000, QDate(2025, 6, 1), QStringLiteral("Dépense A"));
    addExpense(expB, coursesB, 2000, QDate(2025, 6, 2), QStringLiteral("Dépense B"));

    ExchangeService serviceA(a);
    ExchangeService serviceB(b);
    QString error;
    QVERIFY2(serviceA.exportToFile(fromA, &error), qPrintable(error));
    QVERIFY2(serviceB.exportToFile(fromB, &error), qPrintable(error));

    MergeReport intoB;
    MergeReport intoA;
    QVERIFY2(serviceB.importFromFile(fromA, intoB, &error), qPrintable(error));
    QVERIFY2(serviceA.importFromFile(fromB, intoA, &error), qPrintable(error));

    QCOMPARE(intoB.expensesAdded, 1);
    QCOMPARE(intoA.expensesAdded, 1);

    QCOMPARE(expA.forMonth(2025, 6).size(), 2);
    QCOMPARE(expB.forMonth(2025, 6).size(), 2);
}

// Modification concurrente de la même dépense : la version la plus récente gagne
// et le conflit (valeur écrasée) est journalisé.
void ExchangeTest::concurrentModificationLatestWinsAndLogsConflict()
{
    const QString fileA = m_dir->filePath(QStringLiteral("a.db"));
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    const QString seed = m_dir->filePath(QStringLiteral("seed.json"));
    const QString fromA = m_dir->filePath(QStringLiteral("from-a.json"));

    Database a(fileA, QStringLiteral("mod-a"));
    Database b(fileB, QStringLiteral("mod-b"));
    QVERIFY(a.open());
    QVERIFY(b.open());

    CategoryRepository catA(a);
    ExpenseRepository expA(a);
    const int courses = categoryIdByKey(catA, QStringLiteral("alimentation.courses"));
    const int idA = addExpense(expA, courses, 1000, QDate(2025, 7, 1), QStringLiteral("Original"));
    QVERIFY(idA > 0);

    // On partage la dépense avec B via un premier échange (même uuid des deux côtés).
    ExchangeService serviceA(a);
    ExchangeService serviceB(b);
    QString error;
    QVERIFY2(serviceA.exportToFile(seed, &error), qPrintable(error));
    MergeReport seedReport;
    QVERIFY2(serviceB.importFromFile(seed, seedReport, &error), qPrintable(error));
    QCOMPARE(seedReport.expensesAdded, 1);

    // A modifie le montant et porte un horodatage strictement plus récent.
    Expense updated = expA.forMonth(2025, 7).first();
    updated.amountCents = 3000;
    updated.label = QStringLiteral("Corrigé");
    QVERIFY(expA.update(updated));
    forceExpenseUpdatedAt(a, updated.id, QStringLiteral("2099-01-01T00:00:00"));

    // B importe la version récente d'A : elle doit l'emporter, avec un conflit.
    QVERIFY2(serviceA.exportToFile(fromA, &error), qPrintable(error));
    MergeReport report;
    QVERIFY2(serviceB.importFromFile(fromA, report, &error), qPrintable(error));

    QCOMPARE(report.expensesUpdated, 1);
    QCOMPARE(report.conflicts.size(), 1);
    QCOMPARE(report.conflicts.first().table, QStringLiteral("expenses"));
    QVERIFY(report.conflicts.first().description.contains(QStringLiteral("montant")));

    ExpenseRepository expB(b);
    const QVector<Expense> july = expB.forMonth(2025, 7);
    QCOMPARE(july.size(), 1);
    QCOMPARE(july.first().amountCents, qint64(3000));
    QCOMPARE(july.first().label, QStringLiteral("Corrigé"));
}

// Une suppression (tombstone) se propage : après import, la dépense disparaît des
// lectures de l'autre machine.
void ExchangeTest::deletionPropagatesViaTombstone()
{
    const QString fileA = m_dir->filePath(QStringLiteral("a.db"));
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    const QString seed = m_dir->filePath(QStringLiteral("seed.json"));
    const QString fromA = m_dir->filePath(QStringLiteral("from-a.json"));

    Database a(fileA, QStringLiteral("del-a"));
    Database b(fileB, QStringLiteral("del-b"));
    QVERIFY(a.open());
    QVERIFY(b.open());

    CategoryRepository catA(a);
    ExpenseRepository expA(a);
    const int courses = categoryIdByKey(catA, QStringLiteral("alimentation.courses"));
    const int idA = addExpense(expA, courses, 800, QDate(2025, 8, 12));
    QVERIFY(idA > 0);

    ExchangeService serviceA(a);
    ExchangeService serviceB(b);
    QString error;
    QVERIFY2(serviceA.exportToFile(seed, &error), qPrintable(error));
    MergeReport seedReport;
    QVERIFY2(serviceB.importFromFile(seed, seedReport, &error), qPrintable(error));
    QCOMPARE(seedReport.expensesAdded, 1);

    // A supprime (tombstone) et porte un horodatage plus récent.
    QVERIFY(expA.remove(idA));
    forceExpenseUpdatedAt(a, idA, QStringLiteral("2099-01-01T00:00:00"));

    QVERIFY2(serviceA.exportToFile(fromA, &error), qPrintable(error));
    MergeReport report;
    QVERIFY2(serviceB.importFromFile(fromA, report, &error), qPrintable(error));

    QCOMPARE(report.expensesDeleted, 1);
    QCOMPARE(report.expensesUpdated, 0);

    ExpenseRepository expB(b);
    QVERIFY(expB.forMonth(2025, 8).isEmpty());
}

// Réimporter le même instantané ne change rien et ne journalise aucun conflit.
void ExchangeTest::reimportIsIdempotent()
{
    const QString fileA = m_dir->filePath(QStringLiteral("a.db"));
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    const QString exchange = m_dir->filePath(QStringLiteral("export.json"));

    {
        Database a(fileA, QStringLiteral("idem-a"));
        QVERIFY(a.open());
        CategoryRepository categories(a);
        ExpenseRepository expenses(a);
        RecurringRepository recurring(a);
        const int courses = categoryIdByKey(categories, QStringLiteral("alimentation.courses"));
        addExpense(expenses, courses, 1234, QDate(2025, 9, 9), QStringLiteral("Unique"));

        RecurringExpense model;
        model.amountCents = 2500;
        model.categoryId = courses;
        model.dayOfMonth = 10;
        model.startYear = 2025;
        model.startMonth = 1;
        QVERIFY(recurring.add(model).has_value());

        ExchangeService service(a);
        QString error;
        QVERIFY2(service.exportToFile(exchange, &error), qPrintable(error));
    }

    Database b(fileB, QStringLiteral("idem-b"));
    QVERIFY(b.open());
    ExchangeService service(b);
    QString error;

    MergeReport first;
    QVERIFY2(service.importFromFile(exchange, first, &error), qPrintable(error));
    QCOMPARE(first.expensesAdded, 1);
    QCOMPARE(first.recurringAdded, 1);

    MergeReport second;
    QVERIFY2(service.importFromFile(exchange, second, &error), qPrintable(error));
    QCOMPARE(second.expensesAdded, 0);
    QCOMPARE(second.expensesUpdated, 0);
    QCOMPARE(second.recurringAdded, 0);
    QCOMPARE(second.recurringUpdated, 0);
    QCOMPARE(second.expensesUnchanged, 1);
    QCOMPARE(second.recurringUnchanged, 1);
    QCOMPARE(second.conflicts.size(), 0);

    // Toujours une seule dépense et un seul modèle après le second import.
    ExpenseRepository expensesB(b);
    RecurringRepository recurringB(b);
    QCOMPARE(expensesB.forMonth(2025, 9).size(), 1);
    QCOMPARE(recurringB.all().size(), 1);
}

// Deux machines partageant le même modèle récurrent matérialisent le même mois :
// l'uuid déterministe de l'occurrence empêche tout doublon après fusion.
void ExchangeTest::deterministicOccurrenceUuidPreventsDuplicates()
{
    const QString fileA = m_dir->filePath(QStringLiteral("a.db"));
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    const QString seed = m_dir->filePath(QStringLiteral("seed.json"));
    const QString fromA = m_dir->filePath(QStringLiteral("from-a.json"));

    Database a(fileA, QStringLiteral("occ-a"));
    Database b(fileB, QStringLiteral("occ-b"));
    QVERIFY(a.open());
    QVERIFY(b.open());

    CategoryRepository catA(a);
    RecurringRepository recA(a);
    const int courses = categoryIdByKey(catA, QStringLiteral("alimentation.courses"));

    RecurringExpense model;
    model.amountCents = 3000;
    model.label = QStringLiteral("Loyer");
    model.categoryId = courses;
    model.dayOfMonth = 1;
    model.startYear = 2025;
    model.startMonth = 2;
    QVERIFY(recA.add(model).has_value());

    // B reçoit le même modèle (même uuid) via un premier échange.
    ExchangeService serviceA(a);
    ExchangeService serviceB(b);
    QString error;
    QVERIFY2(serviceA.exportToFile(seed, &error), qPrintable(error));
    MergeReport seedReport;
    QVERIFY2(serviceB.importFromFile(seed, seedReport, &error), qPrintable(error));
    QCOMPARE(seedReport.recurringAdded, 1);

    // Chaque machine matérialise indépendamment l'occurrence de février 2025.
    QCOMPARE(recA.materializeDueOccurrences(QDate(2025, 2, 15)), 1);
    RecurringRepository recB(b);
    QCOMPARE(recB.materializeDueOccurrences(QDate(2025, 2, 15)), 1);

    // A exporte (modèle + occurrence) ; B importe : l'occurrence a le même uuid
    // déterministe, donc aucune dépense en double.
    QVERIFY2(serviceA.exportToFile(fromA, &error), qPrintable(error));
    MergeReport report;
    QVERIFY2(serviceB.importFromFile(fromA, report, &error), qPrintable(error));
    QCOMPARE(report.expensesAdded, 0);

    ExpenseRepository expB(b);
    QCOMPARE(expB.forMonth(2025, 2).size(), 1);
}

// Une ligne référençant une catégorie inconnue est ignorée (comptée dans skipped),
// sans rien insérer.
void ExchangeTest::unknownCategoryIsSkipped()
{
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    Database b(fileB, QStringLiteral("skip-b"));
    QVERIFY(b.open());

    QJsonObject expense;
    expense.insert(QStringLiteral("uuid"), QStringLiteral("11111111-1111-1111-1111-111111111111"));
    expense.insert(QStringLiteral("amount"), 500.0);
    expense.insert(QStringLiteral("date"), QStringLiteral("2025-10-01"));
    expense.insert(QStringLiteral("label"), QJsonValue());
    expense.insert(QStringLiteral("category"), QStringLiteral("categorie.inexistante"));
    expense.insert(QStringLiteral("recurring"), QJsonValue());
    expense.insert(QStringLiteral("createdAt"), QStringLiteral("2025-10-01T10:00:00"));
    expense.insert(QStringLiteral("updatedAt"), QStringLiteral("2025-10-01T10:00:00"));
    expense.insert(QStringLiteral("deleted"), false);

    QJsonObject root;
    root.insert(QStringLiteral("format"), QString::fromLatin1(ExchangeService::formatName()));
    root.insert(QStringLiteral("formatVersion"), ExchangeService::formatVersion);
    root.insert(QStringLiteral("exportedAt"), QStringLiteral("2025-10-01T10:00:00"));
    root.insert(QStringLiteral("expenses"), QJsonArray{expense});
    root.insert(QStringLiteral("recurring"), QJsonArray());

    ExchangeService service(b);
    MergeReport report;
    QString error;
    QVERIFY2(service.importDocument(QJsonDocument(root), report, &error), qPrintable(error));

    QCOMPARE(report.skipped, 1);
    QCOMPARE(report.expensesAdded, 0);

    ExpenseRepository expB(b);
    QVERIFY(expB.forMonth(2025, 10).isEmpty());
}

// Un fichier dont le format n'est pas reconnu est refusé proprement (cas réel :
// mauvais fichier sélectionné).
void ExchangeTest::unsupportedFormatIsRejected()
{
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    Database b(fileB, QStringLiteral("reject-b"));
    QVERIFY(b.open());

    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("autre-chose"));
    root.insert(QStringLiteral("formatVersion"), 1);

    ExchangeService service(b);
    MergeReport report;
    QString error;
    QVERIFY(!service.importDocument(QJsonDocument(root), report, &error));
    QVERIFY(!error.isEmpty());

    // Une version future doit aussi être refusée.
    QJsonObject future;
    future.insert(QStringLiteral("format"), QString::fromLatin1(ExchangeService::formatName()));
    future.insert(QStringLiteral("formatVersion"), ExchangeService::formatVersion + 1);
    QString futureError;
    QVERIFY(!service.importDocument(QJsonDocument(future), report, &futureError));
    QVERIFY(!futureError.isEmpty());
}

QTEST_GUILESS_MAIN(ExchangeTest)
#include "tst_exchange.moc"
