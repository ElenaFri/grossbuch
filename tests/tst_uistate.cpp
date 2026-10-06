#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/ExpenseRepository.h"
#include "core/RecurringRepository.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QLocale>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Test de la persistance de l'état de l'interface (dernier onglet) via QSettings.
// Le mode test de QStandardPaths redirige la configuration vers un emplacement
// temporaire : on ne touche jamais à la configuration réelle de l'utilisateur.
class UiStateTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void lastTabIsRestored();
    void outOfRangeTabFallsBackToFirst();

private:
    std::unique_ptr<MainWindow> makeWindow();

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
    return std::make_unique<MainWindow>(*m_categories, *m_expenses, *m_recurring);
}

void UiStateTest::lastTabIsRestored()
{
    {
        auto window = makeWindow();
        auto *tabs = window->findChild<QTabWidget *>();
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 4);
        tabs->setCurrentIndex(2);
        // La fermeture déclenche la sauvegarde dans closeEvent.
        window->close();
    }

    // Une nouvelle fenêtre doit rouvrir sur le dernier onglet consulté.
    auto window = makeWindow();
    auto *tabs = window->findChild<QTabWidget *>();
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->currentIndex(), 2);
}

void UiStateTest::outOfRangeTabFallsBackToFirst()
{
    // Un index invalide en configuration (par ex. après suppression d'un onglet)
    // ne doit pas faire planter : on retombe sur le premier onglet.
    QSettings settings(QStringLiteral("grossbuch"), QStringLiteral("grossbuch"));
    settings.setValue(QStringLiteral("ui/currentTab"), 99);
    settings.sync();

    auto window = makeWindow();
    auto *tabs = window->findChild<QTabWidget *>();
    QVERIFY(tabs != nullptr);
    QCOMPARE(tabs->currentIndex(), 0);
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
