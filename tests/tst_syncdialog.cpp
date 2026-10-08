#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "core/SyncService.h"
#include "ui/SyncController.h"
#include "ui/SyncDialog.h"

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Tests de la boîte de configuration de synchronisation et de son contrôleur. Le
// mode test de QStandardPaths redirige QSettings vers un emplacement temporaire :
// la configuration réelle n'est jamais touchée. On ne teste pas le bouton
// Parcourir (QFileDialog modal). Voir docs/adr/0020.
class SyncDialogTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void prefillsFromController();
    void acceptWritesToController();
    void deviceIdIsStableAndGenerated();
    void defaultsToNotConfiguredAndManual();
    void importNowMergesPeerAndEmits();
    void importNowWithoutPeersDoesNotEmit();
    void exportNowWritesOwnSnapshot();

private:
    static int categoryIdByKey(const CategoryRepository &categories, const QString &key);
    QString makePeerSnapshot(const QString &sharedFolder);

private:
    std::unique_ptr<Database> m_db;
    std::unique_ptr<SyncController> m_controller;
    QString m_backupDir;
    std::unique_ptr<QTemporaryDir> m_tmp;
};

void SyncDialogTest::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void SyncDialogTest::init()
{
    QSettings settings(QStringLiteral("grossbuch"), QStringLiteral("grossbuch"));
    settings.clear();
    settings.sync();

    m_tmp = std::make_unique<QTemporaryDir>();
    QVERIFY(m_tmp->isValid());
    m_backupDir = m_tmp->path();

    m_db = std::make_unique<Database>(QStringLiteral(":memory:"),
                                      QStringLiteral("syncdialogtest"));
    QVERIFY(m_db->open());
    m_controller = std::make_unique<SyncController>(*m_db, m_backupDir);
}

void SyncDialogTest::prefillsFromController()
{
    m_controller->setSharedFolder(QStringLiteral("/tmp/shared"));
    m_controller->setAutoSync(true);

    SyncDialog dialog(*m_controller);
    auto *folder = dialog.findChild<QLineEdit *>(QStringLiteral("folderEdit"));
    auto *autoCheck = dialog.findChild<QCheckBox *>(QStringLiteral("autoSyncCheck"));
    QVERIFY(folder != nullptr);
    QVERIFY(autoCheck != nullptr);
    if (folder == nullptr || autoCheck == nullptr)
        return;

    QCOMPARE(folder->text(), QStringLiteral("/tmp/shared"));
    QVERIFY(autoCheck->isChecked());
}

void SyncDialogTest::acceptWritesToController()
{
    SyncDialog dialog(*m_controller);
    auto *folder = dialog.findChild<QLineEdit *>(QStringLiteral("folderEdit"));
    auto *autoCheck = dialog.findChild<QCheckBox *>(QStringLiteral("autoSyncCheck"));
    QVERIFY(folder != nullptr);
    QVERIFY(autoCheck != nullptr);
    if (folder == nullptr || autoCheck == nullptr)
        return;

    folder->setText(QStringLiteral("  /tmp/partage  "));
    autoCheck->setChecked(true);

    // On clique le vrai bouton OK (comme l'utilisateur) pour déclencher l'écriture.
    auto *buttons = dialog.findChild<QDialogButtonBox *>();
    QVERIFY(buttons != nullptr);
    if (buttons == nullptr)
        return;
    buttons->button(QDialogButtonBox::Ok)->click();

    // Le dossier est enregistré sans espaces superflus et la synchro automatique
    // est activée.
    QCOMPARE(m_controller->sharedFolder(), QStringLiteral("/tmp/partage"));
    QVERIFY(m_controller->autoSync());
    QVERIFY(m_controller->isConfigured());
}

void SyncDialogTest::deviceIdIsStableAndGenerated()
{
    const QString first = m_controller->deviceId();
    QVERIFY(!first.isEmpty());
    // Deux appels successifs renvoient le même identifiant (persisté).
    QCOMPARE(m_controller->deviceId(), first);

    // Un nouveau contrôleur sur la même configuration réutilise l'identifiant.
    SyncController other(*m_db, m_backupDir);
    QCOMPARE(other.deviceId(), first);
}

void SyncDialogTest::defaultsToNotConfiguredAndManual()
{
    // Configuration vierge : aucun dossier, synchro manuelle par défaut.
    QVERIFY(!m_controller->isConfigured());
    QVERIFY(m_controller->sharedFolder().isEmpty());
    QVERIFY(!m_controller->autoSync());
}

int SyncDialogTest::categoryIdByKey(const CategoryRepository &categories, const QString &key)
{
    for (const Category &category : categories.all()) {
        if (category.key == key)
            return category.id;
    }
    return 0;
}

// Crée un instantané pair (une autre machine) contenant une dépense, dans le
// dossier partagé, et renvoie le libellé de la dépense insérée.
QString SyncDialogTest::makePeerSnapshot(const QString &sharedFolder)
{
    const QString peerDbPath = QDir(m_tmp->path()).filePath(QStringLiteral("peer.db"));
    Database peer(peerDbPath, QStringLiteral("syncdialog-peer"));
    const bool opened = peer.open();
    Q_ASSERT(opened);
    Q_UNUSED(opened);

    CategoryRepository cat(peer);
    ExpenseRepository exp(peer);
    Expense expense;
    expense.amountCents = 1234;
    expense.date = QDate(2026, 6, 15);
    expense.label = QStringLiteral("pair");
    expense.categoryId = categoryIdByKey(cat, QStringLiteral("alimentation.courses"));
    exp.add(expense);

    SyncService sync(peer, sharedFolder, QStringLiteral("peer-device"), m_backupDir);
    sync.exportToSharedFolder();
    return expense.label;
}

// Le contrôleur construit un SyncService à partir de sa configuration, fusionne
// l'instantané pair et émet imported() lorsque la base a changé.
void SyncDialogTest::importNowMergesPeerAndEmits()
{
    QTemporaryDir shared;
    QVERIFY(shared.isValid());
    makePeerSnapshot(shared.path());

    m_controller->setSharedFolder(shared.path());
    QSignalSpy spy(m_controller.get(), &SyncController::imported);

    const SyncService::Result result = m_controller->importNow();
    QVERIFY(result.ok);
    QVERIFY(result.changed);
    QCOMPARE(result.filesProcessed, 1);
    QCOMPARE(spy.count(), 1);

    // La dépense du pair est bien présente dans la base locale.
    ExpenseRepository local(*m_db);
    const QVector<Expense> june = local.forMonth(2026, 6);
    QCOMPARE(june.size(), 1);
    QCOMPARE(june.first().amountCents, qint64(1234));
}

// Sans instantané pair, l'import ne change rien et n'émet pas imported().
void SyncDialogTest::importNowWithoutPeersDoesNotEmit()
{
    QTemporaryDir shared;
    QVERIFY(shared.isValid());

    m_controller->setSharedFolder(shared.path());
    QSignalSpy spy(m_controller.get(), &SyncController::imported);

    const SyncService::Result result = m_controller->importNow();
    QVERIFY(result.ok);
    QVERIFY(!result.changed);
    QCOMPARE(spy.count(), 0);
}

// L'export écrit l'instantané de cette machine (nommé d'après son deviceId).
void SyncDialogTest::exportNowWritesOwnSnapshot()
{
    QTemporaryDir shared;
    QVERIFY(shared.isValid());

    m_controller->setSharedFolder(shared.path());
    const SyncService::Result result = m_controller->exportNow();
    QVERIFY(result.ok);
    QVERIFY(!result.skipped);

    const QString expected =
        QDir(shared.path()).filePath(SyncService::snapshotFileName(m_controller->deviceId()));
    QVERIFY(QFile::exists(expected));
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    SyncDialogTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_syncdialog.moc"
