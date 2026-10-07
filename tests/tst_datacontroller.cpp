#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/DataController.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "core/ExchangeService.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Tests du contrôleur de données (export, import avec fusion et sauvegarde
// préalable, restauration), sans dialogue graphique. Voir docs/adr/0012 et 0013.
class DataControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void exportThenImportRoundTrip();
    void importReportsCountsAndEmitsSignal();
    void importFailureReportsError();
    void restoreBringsBaseBackToBackupState();
    void restoreFailureKeepsBaseUsable();

private:
    static int addExpense(ExpenseRepository &repo, int categoryId, qint64 cents, const QDate &date);
    static int firstSelectableCategory(const CategoryRepository &categories);

    std::unique_ptr<QTemporaryDir> m_dir;
};

void DataControllerTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void DataControllerTest::cleanup()
{
    m_dir.reset();
}

int DataControllerTest::addExpense(ExpenseRepository &repo, int categoryId, qint64 cents,
                                   const QDate &date)
{
    Expense expense;
    expense.amountCents = cents;
    expense.date = date;
    expense.categoryId = categoryId;
    return repo.add(expense).value_or(0);
}

int DataControllerTest::firstSelectableCategory(const CategoryRepository &categories)
{
    const QVector<Category> selectable = categories.selectable();
    return selectable.isEmpty() ? 0 : selectable.first().id;
}

// Exporter puis importer dans une base vierge reconstruit les données.
void DataControllerTest::exportThenImportRoundTrip()
{
    const QString fileA = m_dir->filePath(QStringLiteral("a.db"));
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    const QString exchange = m_dir->filePath(QStringLiteral("export.json"));
    const QString backupsB = m_dir->filePath(QStringLiteral("backups-b"));

    {
        Database a(fileA, QStringLiteral("dc-export-a"));
        QVERIFY(a.open());
        CategoryRepository categories(a);
        ExpenseRepository expenses(a);
        addExpense(expenses, firstSelectableCategory(categories), 1599, QDate(2025, 3, 15));

        DataController controller(a, m_dir->filePath(QStringLiteral("backups-a")));
        const DataController::Result result = controller.exportToFile(exchange);
        QVERIFY2(result.ok, qPrintable(result.message));
        QVERIFY(result.message.contains(exchange));
    }

    Database b(fileB, QStringLiteral("dc-export-b"));
    QVERIFY(b.open());
    DataController controller(b, backupsB);
    const DataController::Result result = controller.importFromFile(exchange);
    QVERIFY2(result.ok, qPrintable(result.message));

    ExpenseRepository expenses(b);
    QCOMPARE(expenses.forMonth(2025, 3).size(), 1);
}

// Un import renseigne les compteurs du rapport, crée une sauvegarde préalable et
// émet dataChanged().
void DataControllerTest::importReportsCountsAndEmitsSignal()
{
    const QString fileA = m_dir->filePath(QStringLiteral("a.db"));
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    const QString exchange = m_dir->filePath(QStringLiteral("export.json"));
    const QString backupsB = m_dir->filePath(QStringLiteral("backups-b"));

    {
        Database a(fileA, QStringLiteral("dc-report-a"));
        QVERIFY(a.open());
        CategoryRepository categories(a);
        ExpenseRepository expenses(a);
        const int category = firstSelectableCategory(categories);
        addExpense(expenses, category, 1000, QDate(2025, 4, 1));
        addExpense(expenses, category, 2000, QDate(2025, 4, 2));
        ExchangeService service(a);
        QVERIFY(service.exportToFile(exchange));
    }

    Database b(fileB, QStringLiteral("dc-report-b"));
    QVERIFY(b.open());
    DataController controller(b, backupsB);
    QSignalSpy spy(&controller, &DataController::dataChanged);

    const DataController::Result result = controller.importFromFile(exchange);
    QVERIFY2(result.ok, qPrintable(result.message));
    QVERIFY(result.message.contains(QStringLiteral("2 ajoutée(s)")));
    QCOMPARE(spy.count(), 1);

    // Une sauvegarde préalable a bien été créée.
    QCOMPARE(controller.backups().size(), 1);
}

// Un fichier illisible produit un échec explicite, sans émettre dataChanged().
void DataControllerTest::importFailureReportsError()
{
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    Database b(fileB, QStringLiteral("dc-fail-b"));
    QVERIFY(b.open());
    DataController controller(b, m_dir->filePath(QStringLiteral("backups-b")));
    QSignalSpy spy(&controller, &DataController::dataChanged);

    const DataController::Result result =
        controller.importFromFile(m_dir->filePath(QStringLiteral("inexistant.json")));
    QVERIFY(!result.ok);
    QVERIFY(!result.message.isEmpty());
    QCOMPARE(spy.count(), 0);
}

// Restaurer ramène la base à l'état figé dans la sauvegarde choisie.
void DataControllerTest::restoreBringsBaseBackToBackupState()
{
    const QString livePath = m_dir->filePath(QStringLiteral("grossbuch.db"));
    const QString backups = m_dir->filePath(QStringLiteral("backups"));

    Database db(livePath, QStringLiteral("dc-restore"));
    QVERIFY(db.open());
    CategoryRepository categories(db);
    ExpenseRepository expenses(db);
    const int category = firstSelectableCategory(categories);

    // État S1 (une dépense) puis sauvegarde via le service.
    addExpense(expenses, category, 1000, QDate(2025, 5, 1));
    BackupService service(db, backups);
    const std::optional<QString> backupPath = service.createBackup();
    QVERIFY(backupPath.has_value());

    // État S2 : deux dépenses supplémentaires.
    addExpense(expenses, category, 2000, QDate(2025, 5, 2));
    addExpense(expenses, category, 3000, QDate(2025, 5, 3));
    QCOMPARE(expenses.forMonth(2025, 5).size(), 3);

    DataController controller(db, backups);
    const DataController::Result result = controller.restoreFromBackup(*backupPath);
    QVERIFY2(result.ok, qPrintable(result.message));
    QVERIFY(!db.isOpen()); // la connexion reste fermée après restauration

    // Réouverture : on retrouve l'état S1.
    Database reopened(livePath, QStringLiteral("dc-restore-reopened"));
    QVERIFY(reopened.open());
    ExpenseRepository reopenedExpenses(reopened);
    QCOMPARE(reopenedExpenses.forMonth(2025, 5).size(), 1);
}

// Une restauration impossible laisse la base rouverte et exploitable.
void DataControllerTest::restoreFailureKeepsBaseUsable()
{
    const QString livePath = m_dir->filePath(QStringLiteral("grossbuch.db"));

    Database db(livePath, QStringLiteral("dc-restore-fail"));
    QVERIFY(db.open());
    CategoryRepository categories(db);
    ExpenseRepository expenses(db);
    addExpense(expenses, firstSelectableCategory(categories), 500, QDate(2025, 6, 1));

    DataController controller(db, m_dir->filePath(QStringLiteral("backups")));
    const DataController::Result result =
        controller.restoreFromBackup(m_dir->filePath(QStringLiteral("absente.db")));
    QVERIFY(!result.ok);

    // La base a été rouverte : on peut toujours lire les données.
    QVERIFY(db.isOpen());
    ExpenseRepository reopened(db);
    QCOMPARE(reopened.forMonth(2025, 6).size(), 1);
}

QTEST_GUILESS_MAIN(DataControllerTest)
#include "tst_datacontroller.moc"
