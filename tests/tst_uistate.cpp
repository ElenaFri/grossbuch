#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/ExpenseRepository.h"
#include "core/RecurringRepository.h"
#include "ui/AboutDialog.h"
#include "ui/MainWindow.h"

#include "Version.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QLocale>
#include <QMenuBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
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

private:
    std::unique_ptr<MainWindow> makeWindow();
    static QStringList actionTexts(const MainWindow *window);

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

void UiStateTest::opensOnChartsView()
{
    // Index de la vue Graphiques dans la pile (ViewCharts).
    constexpr int chartsIndex = 3;

    {
        auto window = makeWindow();
        auto *stack = window->findChild<QStackedWidget *>();
        QVERIFY(stack != nullptr);
        QCOMPARE(stack->count(), 4);
        // Au lancement, la vue affichée est toujours les graphiques.
        QCOMPARE(stack->currentIndex(), chartsIndex);

        // On bascule ailleurs puis on ferme : la vue n'est volontairement pas
        // mémorisée d'une session à l'autre.
        QAction *target = nullptr;
        const QList<QAction *> actions = window->findChildren<QAction *>();
        for (QAction *action : actions) {
            if (action->data().isValid() && action->data().toInt() == 2 && target == nullptr)
                target = action;
        }
        QVERIFY(target != nullptr);
        target->trigger();
        QCOMPARE(stack->currentIndex(), 2);
        window->close();
    }

    // Une nouvelle fenêtre rouvre sur les graphiques, pas sur la dernière vue.
    auto window = makeWindow();
    auto *stack = window->findChild<QStackedWidget *>();
    QVERIFY(stack != nullptr);
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

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    UiStateTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_uistate.moc"
