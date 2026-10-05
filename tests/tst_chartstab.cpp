#include "core/Category.h"
#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "ui/ChartsTab.h"

#include <QApplication>
#include <QCheckBox>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Tests de l'onglet Graphiques. L'onglet est piloté via les objectName de ses
// widgets (chartView, year_AAAA), contre une base SQLite en mémoire recréée à
// chaque test. On vérifie la correspondance courbe/année, les valeurs mensuelles
// et la sélection des années.
class ChartsTabTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void defaultSelectsThreeMostRecentYears();
    void fewerThanThreeYearsAllSelected();
    void seriesPlotsMonthlyTotalsInEuros();
    void togglingYearAddsAndRemovesSeries();
    void noDataYieldsNoSeries();

private:
    QChart *chart() const { return m_tab->findChild<QChartView *>("chartView")->chart(); }
    QCheckBox *yearBox(int year) const
    {
        return m_tab->findChild<QCheckBox *>(QStringLiteral("year_%1").arg(year));
    }
    QLineSeries *seriesNamed(int year) const;
    QStringList seriesNames() const;

    int categoryId(const QString &name) const;
    // Insère une dépense sans rafraîchir : en conditions réelles, la base est
    // peuplée avant que l'onglet ne calcule ses courbes. Les tests rafraîchissent
    // ensuite une seule fois pour reproduire ce flux.
    void addExpense(qint64 cents, const QDate &date, const QString &categoryName);

    std::unique_ptr<Database> m_db;
    std::unique_ptr<CategoryRepository> m_categories;
    std::unique_ptr<ExpenseRepository> m_expenses;
    std::unique_ptr<ChartsTab> m_tab;
};

void ChartsTabTest::init()
{
    m_db = std::make_unique<Database>(QStringLiteral(":memory:"), QStringLiteral("chartstest"));
    QVERIFY(m_db->open());
    m_categories = std::make_unique<CategoryRepository>(*m_db);
    m_expenses = std::make_unique<ExpenseRepository>(*m_db);
    m_tab = std::make_unique<ChartsTab>(*m_expenses);
}

void ChartsTabTest::cleanup()
{
    m_tab.reset();
    m_expenses.reset();
    m_categories.reset();
    m_db.reset();
}

int ChartsTabTest::categoryId(const QString &name) const
{
    for (const Category &category : m_categories->all()) {
        if (category.name == name)
            return category.id;
    }
    return 0;
}

void ChartsTabTest::addExpense(qint64 cents, const QDate &date, const QString &categoryName)
{
    Expense expense;
    expense.amountCents = cents;
    expense.date = date;
    expense.categoryId = categoryId(categoryName);
    QVERIFY(expense.categoryId > 0);
    QVERIFY(m_expenses->add(expense).has_value());
}

QLineSeries *ChartsTabTest::seriesNamed(int year) const
{
    const QString name = QString::number(year);
    const QList<QAbstractSeries *> all = chart()->series();
    for (QAbstractSeries *series : all) {
        if (series->name() == name)
            return qobject_cast<QLineSeries *>(series);
    }
    return nullptr;
}

QStringList ChartsTabTest::seriesNames() const
{
    QStringList names;
    const QList<QAbstractSeries *> all = chart()->series();
    for (QAbstractSeries *series : all)
        names << series->name();
    return names;
}

void ChartsTabTest::defaultSelectsThreeMostRecentYears()
{
    // Quatre années disponibles : seules les trois dernières sont tracées.
    addExpense(1000, QDate(2021, 3, 1), QStringLiteral("Courses"));
    addExpense(1000, QDate(2022, 3, 1), QStringLiteral("Courses"));
    addExpense(1000, QDate(2023, 3, 1), QStringLiteral("Courses"));
    addExpense(1000, QDate(2024, 3, 1), QStringLiteral("Courses"));
    m_tab->refresh();

    QCOMPARE(chart()->series().size(), 3);
    QVERIFY(!yearBox(2021)->isChecked());
    QVERIFY(yearBox(2022)->isChecked());
    QVERIFY(yearBox(2023)->isChecked());
    QVERIFY(yearBox(2024)->isChecked());

    // Chaque courbe porte le nom de son année.
    const QStringList names = seriesNames();
    QVERIFY(names.contains(QStringLiteral("2022")));
    QVERIFY(names.contains(QStringLiteral("2023")));
    QVERIFY(names.contains(QStringLiteral("2024")));
    QVERIFY(!names.contains(QStringLiteral("2021")));
}

void ChartsTabTest::fewerThanThreeYearsAllSelected()
{
    // Moins de trois années (cas d'un usage récent) : toutes sont tracées.
    addExpense(1000, QDate(2024, 1, 1), QStringLiteral("Courses"));
    addExpense(1000, QDate(2025, 1, 1), QStringLiteral("Courses"));
    m_tab->refresh();

    QCOMPARE(chart()->series().size(), 2);
    QVERIFY(yearBox(2024)->isChecked());
    QVERIFY(yearBox(2025)->isChecked());
}

void ChartsTabTest::seriesPlotsMonthlyTotalsInEuros()
{
    // Deux dépenses dans le même mois se cumulent ; les autres mois sont à zéro.
    addExpense(1000, QDate(2025, 2, 10), QStringLiteral("Courses"));      // février
    addExpense(500, QDate(2025, 2, 20), QStringLiteral("Restaurants"));   // février
    addExpense(3000, QDate(2025, 12, 31), QStringLiteral("Train"));       // décembre
    m_tab->refresh();

    QLineSeries *series = seriesNamed(2025);
    QVERIFY(series != nullptr);

    // Seuls les mois avec dépense sont tracés : février et décembre, pas janvier.
    const QList<QPointF> points = series->points();
    QCOMPARE(points.size(), 2);
    QCOMPARE(points.at(0).x(), 1.0);   // février (indice 1)
    QCOMPARE(points.at(0).y(), 15.0);  // 10,00 + 5,00
    QCOMPARE(points.at(1).x(), 11.0);  // décembre (indice 11)
    QCOMPARE(points.at(1).y(), 30.0);
}

void ChartsTabTest::togglingYearAddsAndRemovesSeries()
{
    addExpense(1000, QDate(2024, 1, 1), QStringLiteral("Courses"));
    addExpense(1000, QDate(2025, 1, 1), QStringLiteral("Courses"));
    m_tab->refresh();
    QCOMPARE(chart()->series().size(), 2);

    // Décocher une année retire sa courbe.
    yearBox(2024)->setChecked(false);
    QCOMPARE(chart()->series().size(), 1);
    QVERIFY(seriesNamed(2024) == nullptr);
    QVERIFY(seriesNamed(2025) != nullptr);

    // La recocher la rétablit.
    yearBox(2024)->setChecked(true);
    QCOMPARE(chart()->series().size(), 2);
    QVERIFY(seriesNamed(2024) != nullptr);
}

void ChartsTabTest::noDataYieldsNoSeries()
{
    // Base vierge (installation neuve) : aucune courbe, aucune case, pas de plantage.
    m_tab->refresh();
    QCOMPARE(chart()->series().size(), 0);
    QVERIFY(yearBox(2025) == nullptr);
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    ChartsTabTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_chartstab.moc"
