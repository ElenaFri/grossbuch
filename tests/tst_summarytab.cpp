#include "core/Category.h"
#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "ui/SummaryTab.h"

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLocale>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Tests de l'onglet Récapitulatif. L'onglet est piloté via les objectName de ses
// widgets, contre une base SQLite en mémoire recréée à chaque test.
class SummaryTabTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void monthlyBreakdownAggregatesByCategory();
    void childlessRootHasNoChildren();
    void grandTotalMatchesSum();
    void monthSelectorExcludesOtherMonths();
    void annualModeAggregatesWholeYear();

private:
    QComboBox *mode() const { return m_tab->findChild<QComboBox *>("modeCombo"); }
    QComboBox *month() const { return m_tab->findChild<QComboBox *>("monthCombo"); }
    QComboBox *year() const { return m_tab->findChild<QComboBox *>("yearCombo"); }
    QTreeWidget *tree() const { return m_tab->findChild<QTreeWidget *>("summaryTree"); }
    QLabel *total() const { return m_tab->findChild<QLabel *>("totalLabel"); }

    int categoryId(const QString &name) const;
    void addExpense(qint64 cents, const QDate &date, const QString &categoryName);
    QTreeWidgetItem *topLevelItem(const QString &name) const;
    void selectMonthMode(int monthNumber);
    void selectYearMode();

    std::unique_ptr<Database> m_db;
    std::unique_ptr<CategoryRepository> m_categories;
    std::unique_ptr<ExpenseRepository> m_expenses;
    std::unique_ptr<SummaryTab> m_tab;
};

void SummaryTabTest::init()
{
    m_db = std::make_unique<Database>(QStringLiteral(":memory:"), QStringLiteral("summarytest"));
    QVERIFY(m_db->open());
    m_categories = std::make_unique<CategoryRepository>(*m_db);
    m_expenses = std::make_unique<ExpenseRepository>(*m_db);
    m_tab = std::make_unique<SummaryTab>(*m_categories, *m_expenses);
}

void SummaryTabTest::cleanup()
{
    m_tab.reset();
    m_expenses.reset();
    m_categories.reset();
    m_db.reset();
}

int SummaryTabTest::categoryId(const QString &name) const
{
    for (const Category &category : m_categories->all()) {
        if (category.name == name)
            return category.id;
    }
    return 0;
}

void SummaryTabTest::addExpense(qint64 cents, const QDate &date, const QString &categoryName)
{
    Expense expense;
    expense.amountCents = cents;
    expense.date = date;
    expense.categoryId = categoryId(categoryName);
    QVERIFY(expense.categoryId > 0);
    QVERIFY(m_expenses->add(expense).has_value());
    m_tab->refresh();
}

QTreeWidgetItem *SummaryTabTest::topLevelItem(const QString &name) const
{
    for (int i = 0; i < tree()->topLevelItemCount(); ++i) {
        if (tree()->topLevelItem(i)->text(0) == name)
            return tree()->topLevelItem(i);
    }
    return nullptr;
}

void SummaryTabTest::selectMonthMode(int monthNumber)
{
    mode()->setCurrentIndex(mode()->findData(0)); // 0 = Mois
    month()->setCurrentIndex(month()->findData(monthNumber));
}

void SummaryTabTest::selectYearMode()
{
    mode()->setCurrentIndex(mode()->findData(1)); // 1 = Année
}

void SummaryTabTest::monthlyBreakdownAggregatesByCategory()
{
    const QDate today = QDate::currentDate();

    // Deux sous-catégories d'Alimentation et une racine sans enfant (Voyages).
    addExpense(1000, today, QStringLiteral("Courses"));
    addExpense(500, today, QStringLiteral("Restaurants"));
    addExpense(2000, today, QStringLiteral("Voyages"));

    // Deux postes au premier niveau : Alimentation et Voyages.
    QCOMPARE(tree()->topLevelItemCount(), 2);

    QTreeWidgetItem *alimentation = topLevelItem(QStringLiteral("Alimentation"));
    QVERIFY(alimentation != nullptr);
    // Les deux sous-catégories dépensées apparaissent sous Alimentation.
    QCOMPARE(alimentation->childCount(), 2);

    // Une catégorie sans dépense ce mois ne figure pas dans l'arbre.
    QVERIFY(topLevelItem(QStringLiteral("Santé")) == nullptr);
}

void SummaryTabTest::childlessRootHasNoChildren()
{
    addExpense(2000, QDate::currentDate(), QStringLiteral("Voyages"));

    QTreeWidgetItem *voyages = topLevelItem(QStringLiteral("Voyages"));
    QVERIFY(voyages != nullptr);
    QCOMPARE(voyages->childCount(), 0);
}

void SummaryTabTest::grandTotalMatchesSum()
{
    const QDate today = QDate::currentDate();
    addExpense(1000, today, QStringLiteral("Courses"));
    addExpense(500, today, QStringLiteral("Restaurants"));
    addExpense(2000, today, QStringLiteral("Voyages"));

    const QString expected = QLocale().toCurrencyString(35.0); // 3500 centimes
    QVERIFY(total()->text().contains(expected));

    // Le libellé nomme explicitement le mois en cours.
    const QString period = QLocale().toString(QDate(today.year(), today.month(), 1),
                                              QStringLiteral("MMMM yyyy"));
    QVERIFY(total()->text().contains(period));
}

void SummaryTabTest::monthSelectorExcludesOtherMonths()
{
    const int y = QDate::currentDate().year();
    const int targetMonth = 6;
    const int otherMonth = 7;

    // Une dépense dans un seul mois (juin), aucune en juillet.
    addExpense(1000, QDate(y, targetMonth, 15), QStringLiteral("Courses"));

    // En sélectionnant un mois sans dépense, l'arbre est vide.
    selectMonthMode(otherMonth);
    QCOMPARE(tree()->topLevelItemCount(), 0);

    // En sélectionnant le mois de la dépense, elle apparaît.
    selectMonthMode(targetMonth);
    QCOMPARE(tree()->topLevelItemCount(), 1);
    QVERIFY(topLevelItem(QStringLiteral("Alimentation")) != nullptr);
}

void SummaryTabTest::annualModeAggregatesWholeYear()
{
    const int y = QDate::currentDate().year();

    // Deux dépenses dans deux mois distincts de la même année.
    addExpense(1000, QDate(y, 1, 15), QStringLiteral("Courses"));
    addExpense(2000, QDate(y, 6, 15), QStringLiteral("Restaurants"));

    selectYearMode();

    // Le mode annuel cumule toute l'année : Alimentation = 3000, deux enfants.
    QCOMPARE(tree()->topLevelItemCount(), 1);
    QTreeWidgetItem *alimentation = topLevelItem(QStringLiteral("Alimentation"));
    QVERIFY(alimentation != nullptr);
    QCOMPARE(alimentation->childCount(), 2);

    const QString expected = QLocale().toCurrencyString(30.0); // 3000 centimes
    QVERIFY(total()->text().contains(expected));

    // Le libellé nomme explicitement l'année.
    QVERIFY(total()->text().contains(QStringLiteral("l'année %1").arg(y)));
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    SummaryTabTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_summarytab.moc"
