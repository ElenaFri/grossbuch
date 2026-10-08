#include "core/BackupService.h"
#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/DataController.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "ui/RestoreDialog.h"

#include <QApplication>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Test du dialogue de restauration : il doit lister les sauvegardes disponibles
// et n'autoriser la restauration qu'une fois une sauvegarde sélectionnée. La
// restauration elle-même ouvre une confirmation modale et est couverte au niveau
// de DataController ; on vérifie ici le peuplement et le verrouillage du bouton.
// Voir docs/adr/0013 et 0016.
class RestoreDialogTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void listsAvailableBackups();
    void restoreButtonGatedBySelection();

private:
    // Ouvre une base sur fichier, y ajoute une dépense et crée une sauvegarde.
    static void seedOneBackup(Database &db, const QString &backupDir);

    std::unique_ptr<QTemporaryDir> m_dir;
};

void RestoreDialogTest::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
}

void RestoreDialogTest::cleanup()
{
    m_dir.reset();
}

void RestoreDialogTest::seedOneBackup(Database &db, const QString &backupDir)
{
    CategoryRepository categories(db);
    ExpenseRepository expenses(db);
    const QVector<Category> selectable = categories.selectable();
    QVERIFY(!selectable.isEmpty());

    Expense expense;
    expense.amountCents = 1000;
    expense.date = QDate(2025, 5, 1);
    expense.categoryId = selectable.first().id;
    QVERIFY(expenses.add(expense).has_value());

    BackupService backup(db, backupDir);
    QVERIFY(backup.createBackup().has_value());
}

// Le dialogue liste les sauvegardes du contrôleur, chaque entrée portant le
// chemin du fichier de sauvegarde.
void RestoreDialogTest::listsAvailableBackups()
{
    const QString backups = m_dir->filePath(QStringLiteral("backups"));
    Database db(m_dir->filePath(QStringLiteral("grossbuch.db")), QStringLiteral("rd-list"));
    QVERIFY(db.open());
    seedOneBackup(db, backups);

    DataController controller(db, backups);
    QCOMPARE(controller.backups().size(), 1);

    RestoreDialog dialog(controller);
    auto *list = dialog.findChild<QListWidget *>(QStringLiteral("backupList"));
    QVERIFY(list != nullptr);
    if (list == nullptr)
        return;
    QCOMPARE(list->count(), 1);
    QVERIFY(!list->item(0)->data(Qt::UserRole).toString().isEmpty());
}

// Le bouton Restaurer est désactivé tant qu'aucune sauvegarde n'est sélectionnée,
// puis activé dès qu'on en choisit une.
void RestoreDialogTest::restoreButtonGatedBySelection()
{
    const QString backups = m_dir->filePath(QStringLiteral("backups"));
    Database db(m_dir->filePath(QStringLiteral("grossbuch.db")), QStringLiteral("rd-gate"));
    QVERIFY(db.open());
    seedOneBackup(db, backups);

    DataController controller(db, backups);
    RestoreDialog dialog(controller);

    auto *list = dialog.findChild<QListWidget *>(QStringLiteral("backupList"));
    auto *button = dialog.findChild<QPushButton *>(QStringLiteral("restoreButton"));
    QVERIFY(list != nullptr);
    QVERIFY(button != nullptr);
    if (list == nullptr || button == nullptr)
        return;

    // Au départ, rien n'est sélectionné : restauration interdite.
    QVERIFY(!button->isEnabled());

    // Sélectionner une sauvegarde active le bouton.
    list->setCurrentRow(0);
    QVERIFY(button->isEnabled());
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    RestoreDialogTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_restoredialog.moc"
