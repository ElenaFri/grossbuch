#include "core/BackupService.h"
#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "core/ExchangeService.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QVariant>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Tests du service de sauvegarde : copie cohérente (VACUUM INTO), politique
// quotidienne, rotation, restauration et sauvegarde avant import. Voir
// docs/adr/0013.
class BackupTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void createBackupProducesConsistentCopy();
    void dailyBackupOncePerDay();
    void listAndRotateKeepNewest();
    void restoreSwapsBaseToBackupState();
    void restoreRejectsMissingAndCorrupt();
    void importBacksUpBeforeMerging();

private:
    static int addExpense(ExpenseRepository &repo, int categoryId, qint64 cents, const QDate &date);
    static int firstSelectableCategory(const CategoryRepository &categories);
    static int countExpenses(const QString &dbPath);

    std::unique_ptr<QTemporaryDir> m_dir;
};

void BackupTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void BackupTest::cleanup()
{
    m_dir.reset();
}

int BackupTest::addExpense(ExpenseRepository &repo, int categoryId, qint64 cents, const QDate &date)
{
    Expense expense;
    expense.amountCents = cents;
    expense.date = date;
    expense.categoryId = categoryId;
    return repo.add(expense).value_or(0);
}

int BackupTest::firstSelectableCategory(const CategoryRepository &categories)
{
    const QVector<Category> selectable = categories.selectable();
    return selectable.isEmpty() ? 0 : selectable.first().id;
}

int BackupTest::countExpenses(const QString &dbPath)
{
    int count = -1;
    const QString connectionName = QStringLiteral("count-") + dbPath;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName);
        db.setDatabaseName(dbPath);
        if (db.open()) {
            QSqlQuery query(db);
            if (query.exec(QStringLiteral("SELECT COUNT(*) FROM expenses")) && query.next())
                count = query.value(0).toInt();
            db.close();
        }
    }
    QSqlDatabase::removeDatabase(connectionName);
    return count;
}

// Une sauvegarde est un fichier SQLite autonome, intègre, contenant les mêmes
// données que la base vivante.
void BackupTest::createBackupProducesConsistentCopy()
{
    const QString livePath = m_dir->filePath(QStringLiteral("grossbuch.db"));
    const QString backupDir = m_dir->filePath(QStringLiteral("backups"));

    Database db(livePath, QStringLiteral("backup-create"));
    QVERIFY(db.open());
    CategoryRepository categories(db);
    ExpenseRepository expenses(db);
    const int category = firstSelectableCategory(categories);
    QVERIFY(category > 0);
    addExpense(expenses, category, 1500, QDate(2025, 4, 1));
    addExpense(expenses, category, 2500, QDate(2025, 4, 2));

    BackupService service(db, backupDir);
    QString error;
    const std::optional<QString> path = service.createBackup(&error);
    QVERIFY2(path.has_value(), qPrintable(error));
    QVERIFY(QFile::exists(*path));

    // La copie contient exactement les deux dépenses de la base vivante.
    QCOMPARE(countExpenses(*path), 2);
    QCOMPARE(countExpenses(*path), countExpenses(livePath));
}

// La sauvegarde quotidienne n'en crée qu'une par jour.
void BackupTest::dailyBackupOncePerDay()
{
    const QString livePath = m_dir->filePath(QStringLiteral("grossbuch.db"));
    const QString backupDir = m_dir->filePath(QStringLiteral("backups"));

    Database db(livePath, QStringLiteral("backup-daily"));
    QVERIFY(db.open());

    BackupService service(db, backupDir);
    QVERIFY(!service.hasBackupForDate(QDate::currentDate()));

    const std::optional<QString> first = service.dailyBackup();
    QVERIFY(first.has_value());
    QVERIFY(service.hasBackupForDate(QDate::currentDate()));

    // Un second appel le même jour ne crée rien.
    const std::optional<QString> second = service.dailyBackup();
    QVERIFY(!second.has_value());
    QCOMPARE(service.listBackups().size(), 1);
}

// La rotation conserve les N sauvegardes les plus récentes et supprime les plus
// anciennes ; la liste est triée de la plus récente à la plus ancienne.
void BackupTest::listAndRotateKeepNewest()
{
    const QString livePath = m_dir->filePath(QStringLiteral("grossbuch.db"));
    const QString backupDir = m_dir->filePath(QStringLiteral("backups"));

    Database db(livePath, QStringLiteral("backup-rotate"));
    QVERIFY(db.open());

    BackupService service(db, backupDir);
    QStringList created;
    for (int i = 0; i < 4; ++i) {
        const std::optional<QString> path = service.createBackup();
        QVERIFY(path.has_value());
        created << *path;
        QTest::qSleep(5); // horodatage à la milliseconde : garantit l'ordre
    }

    QCOMPARE(service.listBackups().size(), 4);
    // La plus récente d'abord : le dernier fichier créé est en tête.
    QCOMPARE(service.listBackups().first().path, created.last());

    const int removed = service.rotate(2);
    QCOMPARE(removed, 2);

    const QVector<BackupService::BackupInfo> remaining = service.listBackups();
    QCOMPARE(remaining.size(), 2);
    // Les deux conservées sont les deux dernières créées.
    QCOMPARE(remaining.at(0).path, created.at(3));
    QCOMPARE(remaining.at(1).path, created.at(2));
    QVERIFY(!QFile::exists(created.at(0)));
    QVERIFY(!QFile::exists(created.at(1)));
}

// Restaurer remplace la base vivante par l'état figé dans la sauvegarde.
void BackupTest::restoreSwapsBaseToBackupState()
{
    const QString livePath = m_dir->filePath(QStringLiteral("grossbuch.db"));
    const QString backupDir = m_dir->filePath(QStringLiteral("backups"));
    QString backupPath;

    // État S1 : une dépense, puis sauvegarde.
    {
        Database db(livePath, QStringLiteral("backup-restore-1"));
        QVERIFY(db.open());
        CategoryRepository categories(db);
        ExpenseRepository expenses(db);
        const int category = firstSelectableCategory(categories);
        addExpense(expenses, category, 1000, QDate(2025, 5, 1));

        BackupService service(db, backupDir);
        const std::optional<QString> path = service.createBackup();
        QVERIFY(path.has_value());
        backupPath = *path;

        // État S2 : deux dépenses supplémentaires.
        addExpense(expenses, category, 2000, QDate(2025, 5, 2));
        addExpense(expenses, category, 3000, QDate(2025, 5, 3));
    }
    QCOMPARE(countExpenses(livePath), 3);

    // Connexion fermée : on restaure la sauvegarde (état S1).
    QString error;
    QVERIFY2(BackupService::restore(livePath, backupPath, &error), qPrintable(error));

    // Réouverture : on retrouve exactement l'état S1.
    Database reopened(livePath, QStringLiteral("backup-restore-2"));
    QVERIFY(reopened.open());
    QVERIFY(reopened.checkIntegrity());
    ExpenseRepository expenses(reopened);
    QCOMPARE(expenses.forMonth(2025, 5).size(), 1);
}

// Une sauvegarde absente ou corrompue est refusée, sans toucher la base vivante.
void BackupTest::restoreRejectsMissingAndCorrupt()
{
    const QString livePath = m_dir->filePath(QStringLiteral("grossbuch.db"));

    // Base vivante avec une dépense, fermée ensuite.
    {
        Database db(livePath, QStringLiteral("backup-reject"));
        QVERIFY(db.open());
        CategoryRepository categories(db);
        ExpenseRepository expenses(db);
        addExpense(expenses, firstSelectableCategory(categories), 500, QDate(2025, 6, 1));
    }
    QCOMPARE(countExpenses(livePath), 1);

    // Sauvegarde inexistante.
    QString error;
    QVERIFY(!BackupService::restore(livePath, m_dir->filePath(QStringLiteral("absente.db")), &error));
    QVERIFY(!error.isEmpty());

    // Fichier corrompu (pas une base SQLite).
    const QString corrupt = m_dir->filePath(QStringLiteral("corrompue.db"));
    {
        QFile file(corrupt);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("ceci n'est pas une base SQLite valide");
        file.close();
    }
    QString corruptError;
    QVERIFY(!BackupService::restore(livePath, corrupt, &corruptError));
    QVERIFY(!corruptError.isEmpty());

    // La base vivante est intacte.
    QCOMPARE(countExpenses(livePath), 1);
}

// Un import via le moteur d'échange crée une sauvegarde préalable lorsqu'un
// répertoire de sauvegarde est configuré.
void BackupTest::importBacksUpBeforeMerging()
{
    const QString fileA = m_dir->filePath(QStringLiteral("a.db"));
    const QString fileB = m_dir->filePath(QStringLiteral("b.db"));
    const QString exchange = m_dir->filePath(QStringLiteral("export.json"));
    const QString backupDir = m_dir->filePath(QStringLiteral("backups-b"));

    {
        Database a(fileA, QStringLiteral("backup-import-a"));
        QVERIFY(a.open());
        CategoryRepository categories(a);
        ExpenseRepository expenses(a);
        addExpense(expenses, firstSelectableCategory(categories), 1234, QDate(2025, 7, 1));
        ExchangeService service(a);
        QString error;
        QVERIFY2(service.exportToFile(exchange, &error), qPrintable(error));
    }

    Database b(fileB, QStringLiteral("backup-import-b"));
    QVERIFY(b.open());
    ExchangeService service(b);
    service.setBackupDirectory(backupDir);

    MergeReport report;
    QString error;
    QVERIFY2(service.importFromFile(exchange, report, &error), qPrintable(error));
    QCOMPARE(report.expensesAdded, 1);

    // Une sauvegarde a été créée avant la fusion.
    BackupService inspect(b, backupDir);
    QCOMPARE(inspect.listBackups().size(), 1);
}

QTEST_GUILESS_MAIN(BackupTest)
#include "tst_backup.moc"
