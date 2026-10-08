#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "core/RecurringRepository.h"
#include "core/SyncService.h"
#include "ui/AboutDialog.h"
#include "ui/EntryTab.h"
#include "ui/MainWindow.h"
#include "ui/RecurringTab.h"
#include "ui/SummaryTab.h"
#include "ui/UiHelpers.h"

#include "Version.h"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTimer>
#include <QToolBar>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Test de l'état de l'interface (vue par défaut au lancement, absence de barre
// d'outils) et de la présence des actions de menu. Le mode test de QStandardPaths
// redirige la configuration vers un emplacement temporaire : on ne touche jamais
// à la configuration réelle de l'utilisateur. Voir docs/adr/0016.
class UiStateTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void opensOnChartsView();
    void hasNoNavigationToolbar();
    void menusExposeActions();
    void aboutDialogShowsVersion();
    void addingExpenseRefreshesSummary();
    void addingRecurringRefreshesEntry();
    void autoSyncImportsOnOpenAndExportsOnClose();
    void syncNowActionMergesPeer();
    void quittingClosesWithoutLingering();

private:
    std::unique_ptr<MainWindow> makeWindow();
    static QStringList actionTexts(const MainWindow *window);
    static QAction *viewAction(const MainWindow *window, int index);
    static QAction *actionContaining(const MainWindow *window, const QString &needle);
    QString makePeerSnapshot(const QString &sharedFolder, const QDate &date, qint64 cents);

    std::unique_ptr<Database> m_db;
    std::unique_ptr<CategoryRepository> m_categories;
    std::unique_ptr<ExpenseRepository> m_expenses;
    std::unique_ptr<RecurringRepository> m_recurring;
};

void UiStateTest::initTestCase()
{
    // Redirige toute la configuration (QSettings) vers un dossier temporaire.
    QStandardPaths::setTestModeEnabled(true);
}

void UiStateTest::init()
{
    // Base en mémoire : le test ne porte que sur l'état de l'UI, pas les données.
    m_db = std::make_unique<Database>(QStringLiteral(":memory:"), QStringLiteral("uistatetest"));
    QVERIFY(m_db->open());
    m_categories = std::make_unique<CategoryRepository>(*m_db);
    m_expenses = std::make_unique<ExpenseRepository>(*m_db);
    m_recurring = std::make_unique<RecurringRepository>(*m_db);

    // Chaque test part d'une configuration vierge.
    QSettings settings(QStringLiteral("grossbuch"), QStringLiteral("grossbuch"));
    settings.clear();
    settings.sync();
}

std::unique_ptr<MainWindow> UiStateTest::makeWindow()
{
    return std::make_unique<MainWindow>(*m_db, *m_categories, *m_expenses, *m_recurring);
}

QStringList UiStateTest::actionTexts(const MainWindow *window)
{
    QStringList texts;
    const QList<QAction *> actions = window->findChildren<QAction *>();
    for (const QAction *action : actions)
        texts.append(action->text());
    return texts;
}

// Première action dont la donnée associée vaut l'indice de vue demandé (les
// actions de navigation portent leur indice de pile dans QAction::data()).
QAction *UiStateTest::viewAction(const MainWindow *window, int index)
{
    const QList<QAction *> actions = window->findChildren<QAction *>();
    QAction *found = nullptr;
    for (QAction *action : actions) {
        if (found == nullptr && action->data().isValid() && action->data().toInt() == index)
            found = action;
    }
    return found;
}

// Première action dont le libellé contient le texte donné.
QAction *UiStateTest::actionContaining(const MainWindow *window, const QString &needle)
{
    const QList<QAction *> actions = window->findChildren<QAction *>();
    QAction *found = nullptr;
    for (QAction *action : actions) {
        if (found == nullptr && action->text().contains(needle))
            found = action;
    }
    return found;
}

void UiStateTest::opensOnChartsView()
{
    // Index de la vue Graphiques dans la pile (ViewCharts).
    constexpr int chartsIndex = 3;

    {
        auto window = makeWindow();
        auto *stack = window->findChild<QStackedWidget *>();
        QVERIFY(stack != nullptr);
        if (stack == nullptr)
            return;
        QCOMPARE(stack->count(), 4);
        // Au lancement, la vue affichée est toujours les graphiques.
        QCOMPARE(stack->currentIndex(), chartsIndex);

        // On bascule ailleurs puis on ferme : la vue n'est volontairement pas
        // mémorisée d'une session à l'autre.
        QAction *target = viewAction(window.get(), 2);
        QVERIFY(target != nullptr);
        if (target == nullptr)
            return;
        target->trigger();
        QCOMPARE(stack->currentIndex(), 2);
        window->close();
    }

    // Une nouvelle fenêtre rouvre sur les graphiques, pas sur la dernière vue.
    auto window = makeWindow();
    auto *stack = window->findChild<QStackedWidget *>();
    QVERIFY(stack != nullptr);
    if (stack == nullptr)
        return;
    QCOMPARE(stack->currentIndex(), chartsIndex);
}

void UiStateTest::hasNoNavigationToolbar()
{
    // La navigation passe exclusivement par les menus : aucune barre d'outils ni
    // action de bascule ne doit subsister.
    auto window = makeWindow();
    QVERIFY(window->findChildren<QToolBar *>().isEmpty());
    const QStringList texts = actionTexts(window.get());
    QVERIFY(texts.filter(QStringLiteral("outils")).isEmpty());
}

// La barre de menus expose les actions attendues (échange de données,
// navigation entre vues et aide).
void UiStateTest::menusExposeActions()
{
    auto window = makeWindow();

    // Les quatre menus de premier niveau.
    const QList<QAction *> topLevel = window->menuBar()->actions();
    QStringList menuTitles;
    for (const QAction *menu : topLevel)
        menuTitles.append(menu->text());
    QVERIFY(menuTitles.filter(QStringLiteral("Fichier")).size() == 1);
    QVERIFY(menuTitles.filter(QStringLiteral("Édition")).size() == 1);
    QVERIFY(menuTitles.filter(QStringLiteral("Affichage")).size() == 1);
    QVERIFY(menuTitles.filter(QStringLiteral("Aide")).size() == 1);

    // Les actions essentielles, repérées par leur libellé.
    const QStringList texts = actionTexts(window.get());
    const auto hasAction = [&texts](const QString &needle) {
        return !texts.filter(needle).isEmpty();
    };
    QVERIFY(hasAction(QStringLiteral("Importer")));
    QVERIFY(hasAction(QStringLiteral("Exporter")));
    QVERIFY(hasAction(QStringLiteral("Restaurer")));
    QVERIFY(hasAction(QStringLiteral("Saisie des dépenses")));
    QVERIFY(hasAction(QStringLiteral("Dépenses récurrentes")));
    QVERIFY(hasAction(QStringLiteral("Récapitulatif")));
    QVERIFY(hasAction(QStringLiteral("Graphiques")));
    QVERIFY(hasAction(QStringLiteral("Guide")));
    QVERIFY(hasAction(QStringLiteral("propos")));
}

// Intégration : saisir une dépense depuis l'onglet Saisie doit rafraîchir le
// Récapitulatif (câblage expensesChanged -> onExpensesChanged dans MainWindow).
// On n'inspecte pas d'état interne : on bascule sur la vraie vue, on remplit le
// formulaire comme un utilisateur et on vérifie l'effet observable (le total du
// mois affiché par le récapitulatif).
void UiStateTest::addingExpenseRefreshesSummary()
{
    auto window = makeWindow();

    // Bascule sur la vue Saisie (index 0) via l'action de menu.
    QAction *entryView = viewAction(window.get(), 0);
    QVERIFY(entryView != nullptr);
    if (entryView == nullptr)
        return;
    entryView->trigger();

    auto *entry = window->findChild<EntryTab *>();
    auto *summary = window->findChild<SummaryTab *>();
    QVERIFY(entry != nullptr);
    QVERIFY(summary != nullptr);
    if (entry == nullptr || summary == nullptr)
        return;

    // Le récapitulatif s'ouvre sur le mois en cours : il est vide au départ.
    auto *total = summary->findChild<QLabel *>(QStringLiteral("totalLabel"));
    QVERIFY(total != nullptr);
    if (total == nullptr)
        return;
    QVERIFY(total->text().contains(formatMoney(0)));

    // Saisie d'une dépense datée du jour (catégorie valide déjà sélectionnée).
    auto *amountSpin = entry->findChild<QDoubleSpinBox *>(QStringLiteral("amountSpin"));
    auto *saveButton = entry->findChild<QPushButton *>(QStringLiteral("saveButton"));
    QVERIFY(amountSpin != nullptr);
    QVERIFY(saveButton != nullptr);
    if (amountSpin == nullptr || saveButton == nullptr)
        return;
    amountSpin->setValue(12.34);
    saveButton->click();

    // Sans action manuelle sur le récapitulatif, son total reflète la dépense :
    // c'est la preuve que le signal a bien déclenché son rafraîchissement.
    QVERIFY(total->text().contains(formatMoney(1234)));
}

// Intégration : créer un paiement récurrent matérialise une occurrence pour le
// mois courant, ce qui doit rafraîchir l'onglet Saisie (câblage recurringChanged
// -> onRecurringChanged). On vérifie que la table de saisie expose la nouvelle
// dépense sans avoir touché à cet onglet.
void UiStateTest::addingRecurringRefreshesEntry()
{
    auto window = makeWindow();

    QAction *recurringView = viewAction(window.get(), 1);
    QVERIFY(recurringView != nullptr);
    if (recurringView == nullptr)
        return;
    recurringView->trigger();

    auto *recurring = window->findChild<RecurringTab *>();
    auto *entry = window->findChild<EntryTab *>();
    QVERIFY(recurring != nullptr);
    QVERIFY(entry != nullptr);
    if (recurring == nullptr || entry == nullptr)
        return;

    auto *entryTable = entry->findChild<QTableWidget *>(QStringLiteral("expensesTable"));
    QVERIFY(entryTable != nullptr);
    if (entryTable == nullptr)
        return;
    QCOMPARE(entryTable->rowCount(), 0);

    // Remplit le formulaire de récurrent et l'enregistre.
    auto *amountSpin = recurring->findChild<QDoubleSpinBox *>(QStringLiteral("recAmountSpin"));
    auto *categoryCombo = recurring->findChild<QComboBox *>(QStringLiteral("recCategoryCombo"));
    auto *startEdit = recurring->findChild<QDateEdit *>(QStringLiteral("recStartEdit"));
    auto *saveButton = recurring->findChild<QPushButton *>(QStringLiteral("recSaveButton"));
    QVERIFY(amountSpin != nullptr);
    QVERIFY(categoryCombo != nullptr);
    QVERIFY(startEdit != nullptr);
    QVERIFY(saveButton != nullptr);
    if (amountSpin == nullptr || categoryCombo == nullptr || startEdit == nullptr
        || saveButton == nullptr)
        return;

    const int categoryId = m_categories->selectable().first().id;
    const int comboIndex = categoryCombo->findData(categoryId);
    QVERIFY(comboIndex >= 0);
    categoryCombo->setCurrentIndex(comboIndex);

    const QDate today = QDate::currentDate();
    amountSpin->setValue(42.00);
    startEdit->setDate(QDate(today.year(), today.month(), 1));
    saveButton->click();

    // L'occurrence matérialisée apparaît dans la table de saisie du mois courant,
    // preuve que l'onglet Saisie a été rafraîchi par le signal.
    QCOMPARE(entryTable->rowCount(), 1);
}

// La boîte de dialogue À propos affiche la version de l'application (du build).
void UiStateTest::aboutDialogShowsVersion()
{
    AboutDialog dialog;

    const QList<QLabel *> labels = dialog.findChildren<QLabel *>();
    bool hasVersion = false;
    for (const QLabel *label : labels) {
        if (label->objectName() == QStringLiteral("aboutVersion")
            && label->text().contains(QString::fromLatin1(kAppVersion)))
            hasVersion = true;
    }
    QVERIFY(hasVersion);
}

// Crée un instantané pair (une autre machine) contenant une dépense, dans le
// dossier partagé. Le fichier source .db est placé dans le dossier partagé (il
// est ignoré par l'import, qui ne lit que les grossbuch-*.json).
QString UiStateTest::makePeerSnapshot(const QString &sharedFolder, const QDate &date, qint64 cents)
{
    const QString peerDbPath = QDir(sharedFolder).filePath(QStringLiteral("peer-source.db"));
    Database peer(peerDbPath, QStringLiteral("uistate-peer"));
    const bool opened = peer.open();
    Q_ASSERT(opened);
    Q_UNUSED(opened);

    CategoryRepository cat(peer);
    ExpenseRepository exp(peer);
    int courses = 0;
    for (const Category &category : cat.all()) {
        if (category.key == QStringLiteral("alimentation.courses"))
            courses = category.id;
    }

    Expense expense;
    expense.amountCents = cents;
    expense.date = date;
    expense.label = QStringLiteral("pair");
    expense.categoryId = courses;
    exp.add(expense);

    QDir(sharedFolder).mkpath(QStringLiteral("backups"));
    SyncService sync(peer, sharedFolder, QStringLiteral("peer-device"),
                     QDir(sharedFolder).filePath(QStringLiteral("backups")));
    sync.exportToSharedFolder();
    return expense.label;
}

// Intégration : avec la synchronisation automatique activée et configurée, ouvrir
// la fenêtre importe et fusionne l'instantané pair, et la fermer exporte
// l'instantané local. Couvre les branches auto-sync du constructeur et de
// closeEvent, ainsi que le connecteur du signal imported().
void UiStateTest::autoSyncImportsOnOpenAndExportsOnClose()
{
    QTemporaryDir shared;
    QVERIFY(shared.isValid());
    makePeerSnapshot(shared.path(), QDate(2026, 6, 15), 1234);

    QSettings settings(QStringLiteral("grossbuch"), QStringLiteral("grossbuch"));
    settings.setValue(QStringLiteral("sync/sharedFolder"), shared.path());
    settings.setValue(QStringLiteral("sync/auto"), true);
    settings.setValue(QStringLiteral("sync/deviceId"), QStringLiteral("main-device"));
    settings.sync();

    auto window = makeWindow();

    // L'import automatique à l'ouverture a fusionné la dépense du pair.
    const QVector<Expense> june = m_expenses->forMonth(2026, 6);
    QCOMPARE(june.size(), 1);
    QCOMPARE(june.first().amountCents, qint64(1234));

    // La fermeture exporte l'instantané de cette machine.
    window->close();
    const QString ownSnapshot =
        QDir(shared.path()).filePath(QStringLiteral("grossbuch-main-device.json"));
    QVERIFY(QFile::exists(ownSnapshot));
}

// Intégration : l'action « Synchroniser maintenant » fusionne l'instantané pair
// même lorsque la synchronisation automatique est désactivée. Couvre onSyncNow.
void UiStateTest::syncNowActionMergesPeer()
{
    QTemporaryDir shared;
    QVERIFY(shared.isValid());

    QSettings settings(QStringLiteral("grossbuch"), QStringLiteral("grossbuch"));
    settings.setValue(QStringLiteral("sync/sharedFolder"), shared.path());
    settings.setValue(QStringLiteral("sync/auto"), false);
    settings.setValue(QStringLiteral("sync/deviceId"), QStringLiteral("main-device"));
    settings.sync();

    auto window = makeWindow();
    // Synchro manuelle : rien n'a été importé à l'ouverture.
    QVERIFY(m_expenses->forMonth(2026, 7).isEmpty());

    // Le pair publie son instantané après l'ouverture.
    makePeerSnapshot(shared.path(), QDate(2026, 7, 20), 5000);

    QAction *syncNow = actionContaining(window.get(), QStringLiteral("Synchroniser"));
    QVERIFY(syncNow != nullptr);
    if (syncNow == nullptr)
        return;
    syncNow->trigger();

    const QVector<Expense> july = m_expenses->forMonth(2026, 7);
    QCOMPARE(july.size(), 1);
    QCOMPARE(july.first().amountCents, qint64(5000));
}

// Régression : déclencher Quitter ferme la dernière fenêtre et laisse la boucle
// d'événements se terminer d'elle-même (l'application ne doit pas persister en
// arrière-plan). On s'appuie sur quitOnLastWindowClosed (défaut) : la fermeture
// de la dernière fenêtre émet lastWindowClosed, ce qui doit suffire à sortir de
// exec(). Un minuteur de sécurité évite tout blocage si la sortie n'arrivait pas.
void UiStateTest::quittingClosesWithoutLingering()
{
    QVERIFY(QApplication::quitOnLastWindowClosed());

    auto window = makeWindow();
    window->show();

    QAction *quit = actionContaining(window.get(), QStringLiteral("Quitter"));
    QVERIFY(quit != nullptr);
    if (quit == nullptr)
        return;

    QSignalSpy lastClosed(qApp, &QGuiApplication::lastWindowClosed);

    bool safetyFired = false;
    QTimer::singleShot(0, quit, [quit]() { quit->trigger(); });
    QTimer::singleShot(5000, qApp, [&safetyFired]() {
        safetyFired = true;
        QCoreApplication::quit();
    });
    QCoreApplication::exec();

    // La boucle est sortie d'elle-même (filet de sécurité non déclenché) et la
    // dernière fenêtre s'est bien fermée une fois.
    QVERIFY(!safetyFired);
    QCOMPARE(lastClosed.count(), 1);
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    UiStateTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_uistate.moc"
