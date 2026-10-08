#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "core/SyncService.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Tests de la synchronisation distante par dossier partagé. On simule deux
// machines (« A » et « B ») par deux bases SQLite sur fichier, un QTemporaryDir
// comme dossier partagé et un autre comme répertoire de sauvegarde. Chaque
// machine a son propre deviceId et écrit « grossbuch-<deviceId>.json ». Voir
// docs/adr/0020.
class SyncTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void exportCreatesOwnSnapshot();
    void importMergesPeersIgnoringOwn();
    void convergenceRoundTrip();
    void missingFolderSkipped();
    void notConfiguredSkipped();
    void emptyFolderNoChange();
    void oneBackupForBatch();
    void corruptPeerFileSkipped();
    void ownFileNotReimported();

private:
    static int categoryIdByKey(const CategoryRepository &categories, const QString &key);
    static int addExpense(ExpenseRepository &repo, int categoryId, qint64 cents, const QDate &date,
                          const QString &label = QString());

    QString sharedFolder() const;
    QString backupFolder() const;

    std::unique_ptr<QTemporaryDir> m_root;
};

void SyncTest::init()
{
    m_root = std::make_unique<QTemporaryDir>();
    QVERIFY(m_root->isValid());
    QVERIFY(QDir(m_root->path()).mkpath(QStringLiteral("shared")));
    QVERIFY(QDir(m_root->path()).mkpath(QStringLiteral("backups")));
}

void SyncTest::cleanup()
{
    m_root.reset();
}

QString SyncTest::sharedFolder() const
{
    return QDir(m_root->path()).filePath(QStringLiteral("shared"));
}

QString SyncTest::backupFolder() const
{
    return QDir(m_root->path()).filePath(QStringLiteral("backups"));
}

int SyncTest::categoryIdByKey(const CategoryRepository &categories, const QString &key)
{
    for (const Category &category : categories.all()) {
        if (category.key == key)
            return category.id;
    }
    return 0;
}

int SyncTest::addExpense(ExpenseRepository &repo, int categoryId, qint64 cents, const QDate &date,
                         const QString &label)
{
    Expense expense;
    expense.amountCents = cents;
    expense.date = date;
    expense.label = label;
    expense.categoryId = categoryId;
    const std::optional<int> id = repo.add(expense);
    return id.value_or(0);
}

void SyncTest::exportCreatesOwnSnapshot()
{
    const QString dbPath = QDir(m_root->path()).filePath(QStringLiteral("a.db"));
    Database db(dbPath, QStringLiteral("sync-a"));
    QVERIFY(db.open());

    SyncService sync(db, sharedFolder(), QStringLiteral("alice"), backupFolder());
    const SyncService::Result result = sync.exportToSharedFolder();

    QVERIFY(result.ok);
    QVERIFY(!result.skipped);
    const QString expected = QDir(sharedFolder()).filePath(QStringLiteral("grossbuch-alice.json"));
    QCOMPARE(sync.ownSnapshotPath(), expected);
    QVERIFY(QFile::exists(expected));
}

void SyncTest::importMergesPeersIgnoringOwn()
{
    // Machine B écrit son instantané avec une dépense ; machine A l'importe.
    const QString dbA = QDir(m_root->path()).filePath(QStringLiteral("a.db"));
    const QString dbB = QDir(m_root->path()).filePath(QStringLiteral("b.db"));

    {
        Database b(dbB, QStringLiteral("sync-b"));
        QVERIFY(b.open());
        CategoryRepository catB(b);
        ExpenseRepository expB(b);
        const int courses = categoryIdByKey(catB, QStringLiteral("alimentation.courses"));
        QVERIFY(courses > 0);
        addExpense(expB, courses, 1500, QDate(2026, 3, 10), QStringLiteral("Marché"));

        SyncService syncB(b, sharedFolder(), QStringLiteral("bob"), backupFolder());
        QVERIFY(syncB.exportToSharedFolder().ok);
    }

    Database a(dbA, QStringLiteral("sync-a"));
    QVERIFY(a.open());

    SyncService syncA(a, sharedFolder(), QStringLiteral("alice"), backupFolder());
    const SyncService::Result result = syncA.importFromSharedFolder();

    QVERIFY(result.ok);
    QVERIFY(!result.skipped);
    QVERIFY(result.changed);
    QCOMPARE(result.filesProcessed, 1);
    QCOMPARE(result.report.expensesAdded, 1);

    ExpenseRepository expA(a);
    const QVector<Expense> march = expA.forMonth(2026, 3);
    QCOMPARE(march.size(), 1);
    QCOMPARE(march.first().amountCents, qint64(1500));
}

void SyncTest::convergenceRoundTrip()
{
    // A ajoute X, B ajoute Y. Exports croisés, chacun importe l'autre : les deux
    // bases doivent contenir X et Y.
    const QString dbA = QDir(m_root->path()).filePath(QStringLiteral("a.db"));
    const QString dbB = QDir(m_root->path()).filePath(QStringLiteral("b.db"));

    Database a(dbA, QStringLiteral("sync-a"));
    Database b(dbB, QStringLiteral("sync-b"));
    QVERIFY(a.open());
    QVERIFY(b.open());

    CategoryRepository catA(a);
    CategoryRepository catB(b);
    ExpenseRepository expA(a);
    ExpenseRepository expB(b);
    const int coursesA = categoryIdByKey(catA, QStringLiteral("alimentation.courses"));
    const int coursesB = categoryIdByKey(catB, QStringLiteral("alimentation.courses"));

    addExpense(expA, coursesA, 1000, QDate(2026, 1, 5), QStringLiteral("X"));
    addExpense(expB, coursesB, 2000, QDate(2026, 1, 6), QStringLiteral("Y"));

    SyncService syncA(a, sharedFolder(), QStringLiteral("alice"), backupFolder());
    SyncService syncB(b, sharedFolder(), QStringLiteral("bob"), backupFolder());

    QVERIFY(syncA.exportToSharedFolder().ok);
    QVERIFY(syncB.exportToSharedFolder().ok);

    QVERIFY(syncA.importFromSharedFolder().ok);
    QVERIFY(syncB.importFromSharedFolder().ok);

    // Chaque machine voit les deux dépenses de janvier.
    QCOMPARE(expA.forMonth(2026, 1).size(), 2);
    QCOMPARE(expB.forMonth(2026, 1).size(), 2);
}

void SyncTest::missingFolderSkipped()
{
    const QString dbPath = QDir(m_root->path()).filePath(QStringLiteral("a.db"));
    Database db(dbPath, QStringLiteral("sync-a"));
    QVERIFY(db.open());

    const QString absent = QDir(m_root->path()).filePath(QStringLiteral("does-not-exist"));
    SyncService sync(db, absent, QStringLiteral("alice"), backupFolder());

    const SyncService::Result imported = sync.importFromSharedFolder();
    QVERIFY(imported.ok);
    QVERIFY(imported.skipped);
    QVERIFY(!imported.changed);

    const SyncService::Result exported = sync.exportToSharedFolder();
    QVERIFY(exported.ok);
    QVERIFY(exported.skipped);
    // Le dossier ne doit pas avoir été créé.
    QVERIFY(!QDir(absent).exists());
}

void SyncTest::notConfiguredSkipped()
{
    const QString dbPath = QDir(m_root->path()).filePath(QStringLiteral("a.db"));
    Database db(dbPath, QStringLiteral("sync-a"));
    QVERIFY(db.open());

    SyncService sync(db, QString(), QStringLiteral("alice"), backupFolder());
    QVERIFY(sync.importFromSharedFolder().skipped);
    QVERIFY(sync.exportToSharedFolder().skipped);
}

void SyncTest::emptyFolderNoChange()
{
    const QString dbPath = QDir(m_root->path()).filePath(QStringLiteral("a.db"));
    Database db(dbPath, QStringLiteral("sync-a"));
    QVERIFY(db.open());

    SyncService sync(db, sharedFolder(), QStringLiteral("alice"), backupFolder());
    const SyncService::Result result = sync.importFromSharedFolder();

    QVERIFY(result.ok);
    QVERIFY(!result.skipped);
    QVERIFY(!result.changed);
    QCOMPARE(result.filesProcessed, 0);
    // Aucune sauvegarde ne doit être prise quand il n'y a rien à fusionner.
    QVERIFY(QDir(backupFolder()).entryList(QDir::Files).isEmpty());
}

void SyncTest::oneBackupForBatch()
{
    // Deux instantanés pairs présents : l'import doit prendre exactement une
    // sauvegarde pour la salve, pas une par fichier.
    const QString dbA = QDir(m_root->path()).filePath(QStringLiteral("a.db"));
    const QString dbB = QDir(m_root->path()).filePath(QStringLiteral("b.db"));
    const QString dbC = QDir(m_root->path()).filePath(QStringLiteral("c.db"));

    for (const auto &pair : {std::make_pair(dbB, QStringLiteral("bob")),
                             std::make_pair(dbC, QStringLiteral("carol"))}) {
        Database peer(pair.first, QStringLiteral("sync-peer-%1").arg(pair.second));
        QVERIFY(peer.open());
        CategoryRepository cat(peer);
        ExpenseRepository exp(peer);
        const int courses = categoryIdByKey(cat, QStringLiteral("alimentation.courses"));
        addExpense(exp, courses, 500, QDate(2026, 2, 1), pair.second);
        SyncService sync(peer, sharedFolder(), pair.second, backupFolder());
        QVERIFY(sync.exportToSharedFolder().ok);
    }

    Database a(dbA, QStringLiteral("sync-a"));
    QVERIFY(a.open());
    SyncService syncA(a, sharedFolder(), QStringLiteral("alice"), backupFolder());
    const SyncService::Result result = syncA.importFromSharedFolder();

    QVERIFY(result.ok);
    QCOMPARE(result.filesProcessed, 2);
    // Une seule sauvegarde pour toute la salve.
    const QStringList backups =
        QDir(backupFolder()).entryList({QStringLiteral("*.db")}, QDir::Files);
    QCOMPARE(backups.size(), 1);
}

void SyncTest::corruptPeerFileSkipped()
{
    // Un instantané pair valide et un corrompu : le valide doit être fusionné,
    // le corrompu ignoré sans faire échouer l'opération.
    const QString dbA = QDir(m_root->path()).filePath(QStringLiteral("a.db"));
    const QString dbB = QDir(m_root->path()).filePath(QStringLiteral("b.db"));

    {
        Database b(dbB, QStringLiteral("sync-b"));
        QVERIFY(b.open());
        CategoryRepository catB(b);
        ExpenseRepository expB(b);
        const int courses = categoryIdByKey(catB, QStringLiteral("alimentation.courses"));
        addExpense(expB, courses, 700, QDate(2026, 4, 2), QStringLiteral("valide"));
        SyncService syncB(b, sharedFolder(), QStringLiteral("bob"), backupFolder());
        QVERIFY(syncB.exportToSharedFolder().ok);
    }

    // Instantané corrompu d'un troisième pair.
    const QString corruptPath =
        QDir(sharedFolder()).filePath(QStringLiteral("grossbuch-carol.json"));
    QFile corrupt(corruptPath);
    QVERIFY(corrupt.open(QIODevice::WriteOnly));
    corrupt.write("{ this is not valid json ");
    corrupt.close();

    Database a(dbA, QStringLiteral("sync-a"));
    QVERIFY(a.open());
    SyncService syncA(a, sharedFolder(), QStringLiteral("alice"), backupFolder());
    const SyncService::Result result = syncA.importFromSharedFolder();

    QVERIFY(result.ok);
    QCOMPARE(result.filesProcessed, 1);
    QVERIFY(result.changed);

    ExpenseRepository expA(a);
    QCOMPARE(expA.forMonth(2026, 4).size(), 1);
}

void SyncTest::ownFileNotReimported()
{
    // A exporte son instantané puis importe : son propre fichier ne doit pas être
    // refusionné.
    const QString dbPath = QDir(m_root->path()).filePath(QStringLiteral("a.db"));
    Database db(dbPath, QStringLiteral("sync-a"));
    QVERIFY(db.open());

    CategoryRepository cat(db);
    ExpenseRepository exp(db);
    const int courses = categoryIdByKey(cat, QStringLiteral("alimentation.courses"));
    addExpense(exp, courses, 900, QDate(2026, 5, 3), QStringLiteral("local"));

    SyncService sync(db, sharedFolder(), QStringLiteral("alice"), backupFolder());
    QVERIFY(sync.exportToSharedFolder().ok);

    const SyncService::Result result = sync.importFromSharedFolder();
    QVERIFY(result.ok);
    QVERIFY(!result.changed);
    QCOMPARE(result.filesProcessed, 0);
}

QTEST_GUILESS_MAIN(SyncTest)

#include "tst_sync.moc"
