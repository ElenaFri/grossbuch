#include "core/Category.h"
#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "core/RecurringExpense.h"
#include "core/RecurringRepository.h"
#include "ui/RecurringTab.h"

#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTableWidget>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Tests de l'onglet Paiements récurrents. L'onglet est piloté via les objectName
// de ses widgets, contre une base SQLite en mémoire recréée à chaque test.
class RecurringTabTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void addCreatesModelAndMaterializesCurrentMonth();
    void addRejectsNonPositiveAmount();
    void editUpdatesModelKeepingStart();
    void deactivateThenReactivateUpdatesStateAndButton();

private:
    QDoubleSpinBox *amount() const { return w<QDoubleSpinBox>("recAmountSpin"); }
    QComboBox *category() const { return w<QComboBox>("recCategoryCombo"); }
    QLineEdit *label() const { return w<QLineEdit>("recLabelEdit"); }
    QSpinBox *day() const { return w<QSpinBox>("recDaySpin"); }
    QDateEdit *start() const { return w<QDateEdit>("recStartEdit"); }
    QPushButton *saveButton() const { return w<QPushButton>("recSaveButton"); }
    QPushButton *editButton() const { return w<QPushButton>("recEditButton"); }
    QPushButton *toggleButton() const { return w<QPushButton>("recToggleButton"); }
    QTableWidget *table() const { return w<QTableWidget>("recurringTable"); }

    template <typename T> T *w(const char *name) const { return m_tab->findChild<T *>(name); }

    int categoryId(const QString &name) const;
    void selectCategoryByName(const QString &name);

    std::unique_ptr<Database> m_db;
    std::unique_ptr<CategoryRepository> m_categories;
    std::unique_ptr<ExpenseRepository> m_expenses;
    std::unique_ptr<RecurringRepository> m_recurring;
    std::unique_ptr<RecurringTab> m_tab;
};

void RecurringTabTest::init()
{
    m_db = std::make_unique<Database>(QStringLiteral(":memory:"), QStringLiteral("rectabtest"));
    QVERIFY(m_db->open());
    m_categories = std::make_unique<CategoryRepository>(*m_db);
    m_expenses = std::make_unique<ExpenseRepository>(*m_db);
    m_recurring = std::make_unique<RecurringRepository>(*m_db);
    m_tab = std::make_unique<RecurringTab>(*m_categories, *m_recurring);
}

void RecurringTabTest::cleanup()
{
    m_tab.reset();
    m_recurring.reset();
    m_expenses.reset();
    m_categories.reset();
    m_db.reset();
}

int RecurringTabTest::categoryId(const QString &name) const
{
    for (const Category &category : m_categories->all()) {
        if (category.name == name)
            return category.id;
    }
    return 0;
}

void RecurringTabTest::selectCategoryByName(const QString &name)
{
    const int id = categoryId(name);
    const int index = category()->findData(id);
    QVERIFY(index >= 0);
    category()->setCurrentIndex(index);
}

void RecurringTabTest::addCreatesModelAndMaterializesCurrentMonth()
{
    QSignalSpy spy(m_tab.get(), &RecurringTab::recurringChanged);

    const QDate today = QDate::currentDate();
    amount()->setValue(850.0);
    selectCategoryByName(QStringLiteral("Résidence principale"));
    label()->setText(QStringLiteral("Loyer"));
    day()->setValue(5);
    start()->setDate(QDate(today.year(), today.month(), 1));
    saveButton()->click();

    // Le modèle apparaît dans la liste.
    QCOMPARE(table()->rowCount(), 1);
    QCOMPARE(m_recurring->all().size(), 1);

    // Une occurrence a été matérialisée pour le mois en cours.
    const QVector<Expense> thisMonth = m_expenses->forMonth(today.year(), today.month());
    QCOMPARE(thisMonth.size(), 1);
    QCOMPARE(thisMonth.first().amountCents, qint64(85000));
    QCOMPARE(thisMonth.first().categoryId, categoryId(QStringLiteral("Résidence principale")));

    // Les autres onglets ont été prévenus.
    QCOMPARE(spy.count(), 1);
}

void RecurringTabTest::addRejectsNonPositiveAmount()
{
    QSignalSpy spy(m_tab.get(), &RecurringTab::recurringChanged);

    amount()->setValue(0.0);
    selectCategoryByName(QStringLiteral("Résidence principale"));
    saveButton()->click();

    // Rien n'est créé et aucun signal n'est émis.
    QCOMPARE(table()->rowCount(), 0);
    QVERIFY(m_recurring->all().isEmpty());
    QCOMPARE(spy.count(), 0);
}

void RecurringTabTest::editUpdatesModelKeepingStart()
{
    const QDate today = QDate::currentDate();
    amount()->setValue(850.0);
    selectCategoryByName(QStringLiteral("Résidence principale"));
    label()->setText(QStringLiteral("Loyer"));
    day()->setValue(5);
    start()->setDate(QDate(today.year(), today.month(), 1));
    saveButton()->click();
    QCOMPARE(table()->rowCount(), 1);

    const int originalStartYear = m_recurring->all().first().startYear;
    const int originalStartMonth = m_recurring->all().first().startMonth;

    // On entre en mode édition et on modifie le montant et le libellé.
    table()->selectRow(0);
    editButton()->click();
    // Le mois de début n'est pas modifiable pendant l'édition.
    QVERIFY(!start()->isEnabled());
    amount()->setValue(900.0);
    label()->setText(QStringLiteral("Loyer révisé"));
    saveButton()->click();

    const QVector<RecurringExpense> all = m_recurring->all();
    QCOMPARE(all.size(), 1);
    QCOMPARE(all.first().amountCents, qint64(90000));
    QCOMPARE(all.first().label, QStringLiteral("Loyer révisé"));
    // Le début est inchangé.
    QCOMPARE(all.first().startYear, originalStartYear);
    QCOMPARE(all.first().startMonth, originalStartMonth);
    // Après enregistrement, le formulaire est revenu en mode ajout.
    QVERIFY(start()->isEnabled());
}

void RecurringTabTest::deactivateThenReactivateUpdatesStateAndButton()
{
    const QDate today = QDate::currentDate();
    amount()->setValue(1200.0);
    selectCategoryByName(QStringLiteral("Résidence principale"));
    start()->setDate(QDate(today.year(), today.month(), 1));
    saveButton()->click();
    QCOMPARE(table()->rowCount(), 1);

    // Désactivation.
    table()->selectRow(0);
    QCOMPARE(toggleButton()->text(), QStringLiteral("Désactiver"));
    toggleButton()->click();
    QVERIFY(!m_recurring->all().first().active);
    QCOMPARE(table()->item(0, 5)->text(), QStringLiteral("Désactivé"));

    // Le bouton propose désormais la réactivation.
    table()->selectRow(0);
    QCOMPARE(toggleButton()->text(), QStringLiteral("Réactiver"));
    toggleButton()->click();
    QVERIFY(m_recurring->all().first().active);
    QCOMPARE(table()->item(0, 5)->text(), QStringLiteral("Actif"));
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    RecurringTabTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_recurringtab.moc"
