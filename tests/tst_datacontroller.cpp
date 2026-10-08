#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/DataController.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "core/ExchangeService.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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
    void importReportMentionsSkippedAndConflicts();
    void importFailureReportsError();
    void exportToUnwritablePathReportsError();
    void restoreBringsBaseBackToBackupState();
    void restoreFailureKeepsBaseUsable();

private:
    static int addExpense(ExpenseRepository &repo, int categoryId, qint64 cents, const QDate &date);
    static int firstSelectableCategory(const CategoryRepository &categories);
    static QString firstCategoryKey(const CategoryRepository &categories);
    static QJsonObject makeExpenseJson(const QString &uuid, double amount, const QString &date,
                                       const QString &categoryKey, const QString &updatedAt);
    static void writeExchangeFile(const QString &path, const QJsonArray &expenses);

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

QString DataControllerTest::firstCategoryKey(const CategoryRepository &categories)
{
    const QVector<Category> selectable = categories.selectable();
    return selectable.isEmpty() ? QString() : selectable.first().key;
}

// Construit un objet dépense au format d'échange (voir ExchangeService).
QJsonObject DataControllerTest::makeExpenseJson(const QString &uuid, double amount,
                                               const QString &date, const QString &categoryKey,
                                               const QString &updatedAt)
{
    QJsonObject expense;
    expense.insert(QStringLiteral("uuid"), uuid);
    expense.insert(QStringLiteral("amount"), amount);
    expense.insert(QStringLiteral("date"), date);
    expense.insert(QStringLiteral("label"), QJsonValue());
    expense.insert(QStringLiteral("category"), categoryKey);
    expense.insert(QStringLiteral("recurring"), QJsonValue());
    expense.insert(QStringLiteral("createdAt"), QStringLiteral("2025-01-01T10:00:00"));
    expense.insert(QStringLiteral("updatedAt"), updatedAt);
    expense.insert(QStringLiteral("deleted"), false);
    return expense;
}

// Écrit un document d'échange complet (en-tête + dépenses) sur disque.
void DataControllerTest::writeExchangeFile(const QString &path, const QJsonArray &expenses)
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QString::fromLatin1(ExchangeService::formatName()));
    root.insert(QStringLiteral("formatVersion"), ExchangeService::formatVersion);
    root.insert(QStringLiteral("exportedAt"), QStringLiteral("2025-02-01T10:00:00"));
    root.insert(QStringLiteral("expenses"), expenses);
    root.insert(QStringLiteral("recurring"), QJsonArray());

    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(QJsonDocument(root).toJson());
    file.close();
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

// Un import venu d'un autre poste peut à la fois ignorer des lignes (catégorie
// absente localement) et provoquer un conflit (version plus récente d'une dépense
// déjà présente). Le compte rendu doit mentionner les deux cas.
void DataControllerTest::importReportMentionsSkippedAndConflicts()
{
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    const QString seed = m_dir->filePath(QStringLiteral("seed.json"));
    const QString incoming = m_dir->filePath(QStringLiteral("incoming.json"));
    const QString backupsB = m_dir->filePath(QStringLiteral("backups-b"));

    Database b(fileB, QStringLiteral("dc-report-mix"));
    QVERIFY(b.open());
    CategoryRepository categories(b);
    const QString key = firstCategoryKey(categories);
    QVERIFY(!key.isEmpty());

    const QString uuid = QStringLiteral("22222222-2222-2222-2222-222222222222");

    // État initial de B : une dépense à 10,00 € (version du 1er janvier).
    writeExchangeFile(seed, QJsonArray{makeExpenseJson(uuid, 1000.0, QStringLiteral("2025-01-15"),
                                                       key, QStringLiteral("2025-01-01T10:00:00"))});
    {
        ExchangeService service(b);
        MergeReport report;
        QString error;
        QVERIFY2(service.importFromFile(seed, report, &error), qPrintable(error));
    }

    // Fichier reçu : version plus récente et divergente de la même dépense (conflit)
    // plus une dépense rattachée à une catégorie inconnue (ignorée).
    writeExchangeFile(
        incoming,
        QJsonArray{
            makeExpenseJson(uuid, 2000.0, QStringLiteral("2025-01-15"), key,
                            QStringLiteral("2025-02-01T10:00:00")),
            makeExpenseJson(QStringLiteral("33333333-3333-3333-3333-333333333333"), 500.0,
                            QStringLiteral("2025-02-10"), QStringLiteral("categorie.inexistante"),
                            QStringLiteral("2025-02-01T10:00:00"))});

    DataController controller(b, backupsB);
    const DataController::Result result = controller.importFromFile(incoming);
    QVERIFY2(result.ok, qPrintable(result.message));
    QVERIFY(result.message.contains(QStringLiteral("ignor\u00e9e(s)")));
    QVERIFY(result.message.contains(QStringLiteral("conflit(s)")));

    // La dépense en conflit a bien pris la valeur la plus récente.
    ExpenseRepository expB(b);
    const QVector<Expense> january = expB.forMonth(2025, 1);
    QCOMPARE(january.size(), 1);
    QCOMPARE(january.first().amountCents, qint64(2000));
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

// Exporter vers un emplacement non inscriptible échoue proprement (dossier parent
// inexistant) : le contrôleur renvoie une erreur plutôt que de planter.
void DataControllerTest::exportToUnwritablePathReportsError()
{
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    Database b(fileB, QStringLiteral("dc-export-fail"));
    QVERIFY(b.open());

    DataController controller(b, m_dir->filePath(QStringLiteral("backups-b")));
    const QString badPath =
        m_dir->filePath(QStringLiteral("dossier-absent/sous-dossier/export.json"));
    const DataController::Result result = controller.exportToFile(badPath);

    QVERIFY(!result.ok);
    QVERIFY(result.message.contains(QStringLiteral("Export impossible")));
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
