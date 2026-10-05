#include "core/Category.h"
#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/Expense.h"
#include "core/ExpenseRepository.h"
#include "ui/EntryTab.h"

#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTimer>
#include <QtTest>

#include <memory>

using namespace grossbuch;

// Tests de l'onglet de saisie. L'onglet est piloté via les objectName de ses
// widgets, contre une base SQLite en mémoire recréée à chaque test.
class EntryTabTest : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void comboIsGroupedAndSelectable();
    void addExpenseThroughForm();
    void addResetsTheForm();
    void validationRejectsZeroAmount();
    void validationRejectsNoCategory();
    void editExpenseThroughForm();
    void cancelEditReturnsToAddMode();
    void deleteExpenseThroughForm();

private:
    QDoubleSpinBox *amount() const { return m_tab->findChild<QDoubleSpinBox *>("amountSpin"); }
    QDateEdit *dateEdit() const { return m_tab->findChild<QDateEdit *>("dateEdit"); }
    QLineEdit *label() const { return m_tab->findChild<QLineEdit *>("labelEdit"); }
    QComboBox *category() const { return m_tab->findChild<QComboBox *>("categoryCombo"); }
    QPushButton *saveButton() const { return m_tab->findChild<QPushButton *>("saveButton"); }
    QPushButton *cancelButton() const { return m_tab->findChild<QPushButton *>("cancelButton"); }
    QPushButton *editButton() const { return m_tab->findChild<QPushButton *>("editButton"); }
    QPushButton *deleteButton() const { return m_tab->findChild<QPushButton *>("deleteButton"); }
    QTableWidget *table() const { return m_tab->findChild<QTableWidget *>("expensesTable"); }

    int firstSelectableCategory() const { return m_categories->selectable().first().id; }
    void addExpenseToday(qint64 cents);
    static void acceptNextQuestionWithYes();

    std::unique_ptr<Database> m_db;
    std::unique_ptr<CategoryRepository> m_categories;
    std::unique_ptr<ExpenseRepository> m_expenses;
    std::unique_ptr<EntryTab> m_tab;
};

void EntryTabTest::init()
{
    m_db = std::make_unique<Database>(QStringLiteral(":memory:"), QStringLiteral("uitest"));
    QVERIFY(m_db->open());
    m_categories = std::make_unique<CategoryRepository>(*m_db);
    m_expenses = std::make_unique<ExpenseRepository>(*m_db);
    m_tab = std::make_unique<EntryTab>(*m_categories, *m_expenses);
}

void EntryTabTest::cleanup()
{
    m_tab.reset();
    m_expenses.reset();
    m_categories.reset();
    m_db.reset();
}

void EntryTabTest::addExpenseToday(qint64 cents)
{
    Expense expense;
    expense.amountCents = cents;
    expense.date = QDate::currentDate();
    expense.categoryId = firstSelectableCategory();
    QVERIFY(m_expenses->add(expense).has_value());
    m_tab->refresh();
}

void EntryTabTest::acceptNextQuestionWithYes()
{
    // Clique « Oui » dès que la boîte de dialogue modale apparaît, pour ne pas
    // bloquer le test sur QMessageBox::question().
    auto *timer = new QTimer;
    timer->setInterval(10);
    QObject::connect(timer, &QTimer::timeout, [timer] {
        if (auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
            box->button(QMessageBox::Yes)->click();
            timer->stop();
            timer->deleteLater();
        }
    });
    timer->start();
}

void EntryTabTest::comboIsGroupedAndSelectable()
{
    QComboBox *combo = category();
    QVERIFY(combo != nullptr);

    int selectableCount = 0;
    int headerCount = 0;
    auto *model = qobject_cast<QStandardItemModel *>(combo->model());
    QVERIFY(model != nullptr);
    for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).isValid() && combo->itemData(i).toInt() > 0)
            ++selectableCount;
        else if (model->item(i)->flags() == Qt::NoItemFlags)
            ++headerCount;
    }

    // Autant d'éléments sélectionnables que de catégories feuilles.
    QCOMPARE(selectableCount, m_categories->selectable().size());
    // Au moins une catégorie racine avec enfants => au moins un en-tête.
    QVERIFY(headerCount > 0);
    // Une catégorie valide est sélectionnée par défaut.
    QVERIFY(combo->currentData().toInt() > 0);
}

void EntryTabTest::addExpenseThroughForm()
{
    QSignalSpy spy(m_tab.get(), &EntryTab::expensesChanged);

    amount()->setValue(12.34);
    label()->setText(QStringLiteral("Test"));
    // Une catégorie est déjà sélectionnée par défaut.

    saveButton()->click();

    QCOMPARE(spy.count(), 1);
    const QDate today = QDate::currentDate();
    const QVector<Expense> month = m_expenses->forMonth(today.year(), today.month());
    QCOMPARE(month.size(), 1);
    QCOMPARE(month.first().amountCents, qint64(1234));
    QCOMPARE(month.first().label, QStringLiteral("Test"));
    QCOMPARE(table()->rowCount(), 1);
}

void EntryTabTest::addResetsTheForm()
{
    amount()->setValue(42.00);
    label()->setText(QStringLiteral("Quelque chose"));
    saveButton()->click();

    // Après enregistrement, le formulaire est réinitialisé.
    QCOMPARE(amount()->value(), 0.0);
    QVERIFY(label()->text().isEmpty());
}

void EntryTabTest::validationRejectsZeroAmount()
{
    QSignalSpy spy(m_tab.get(), &EntryTab::expensesChanged);

    amount()->setValue(0.0);
    saveButton()->click();

    QCOMPARE(spy.count(), 0);
    const QDate today = QDate::currentDate();
    QVERIFY(m_expenses->forMonth(today.year(), today.month()).isEmpty());
}

void EntryTabTest::validationRejectsNoCategory()
{
    QSignalSpy spy(m_tab.get(), &EntryTab::expensesChanged);

    // Force la sélection sur un en-tête (sans identifiant de catégorie).
    QComboBox *combo = category();
    auto *model = qobject_cast<QStandardItemModel *>(combo->model());
    int headerIndex = -1;
    for (int i = 0; i < combo->count(); ++i) {
        if (model->item(i)->flags() == Qt::NoItemFlags) {
            headerIndex = i;
            break;
        }
    }
    QVERIFY(headerIndex >= 0);
    combo->setCurrentIndex(headerIndex);

    amount()->setValue(10.0);
    saveButton()->click();

    QCOMPARE(spy.count(), 0);
    const QDate today = QDate::currentDate();
    QVERIFY(m_expenses->forMonth(today.year(), today.month()).isEmpty());
}

void EntryTabTest::editExpenseThroughForm()
{
    addExpenseToday(1000);
    QCOMPARE(table()->rowCount(), 1);

    QSignalSpy spy(m_tab.get(), &EntryTab::expensesChanged);

    table()->selectRow(0);
    editButton()->click();

    // Le formulaire est passé en mode édition avec le montant chargé.
    QCOMPARE(amount()->value(), 10.0);

    // Modification complète : montant, catégorie et date (premier jour du mois,
    // pour que la dépense reste visible dans la liste du mois en cours).
    const QVector<Category> selectable = m_categories->selectable();
    QVERIFY(selectable.size() >= 2);
    const int otherCategory = selectable.at(1).id;
    QVERIFY(otherCategory != selectable.first().id);
    const QDate firstOfMonth(QDate::currentDate().year(), QDate::currentDate().month(), 1);

    amount()->setValue(25.50);
    category()->setCurrentIndex(category()->findData(otherCategory));
    dateEdit()->setDate(firstOfMonth);
    saveButton()->click();

    QCOMPARE(spy.count(), 1);
    const QDate today = QDate::currentDate();
    const QVector<Expense> month = m_expenses->forMonth(today.year(), today.month());
    QCOMPARE(month.size(), 1);
    QCOMPARE(month.first().amountCents, qint64(2550));
    QCOMPARE(month.first().categoryId, otherCategory);
    QCOMPARE(month.first().date, firstOfMonth);
    // Pas de nouvelle ligne créée : l'édition remplace la dépense existante.
    QCOMPARE(table()->rowCount(), 1);
}

void EntryTabTest::cancelEditReturnsToAddMode()
{
    addExpenseToday(1000);
    QCOMPARE(table()->rowCount(), 1);

    table()->selectRow(0);
    editButton()->click();

    // En mode édition, le bouton Annuler est visible.
    QVERIFY(!cancelButton()->isHidden());
    cancelButton()->click();

    // Après annulation : formulaire réinitialisé, bouton Annuler masqué.
    QCOMPARE(amount()->value(), 0.0);
    QVERIFY(cancelButton()->isHidden());

    // Un enregistrement suivant crée une NOUVELLE dépense sans toucher à la première.
    QSignalSpy spy(m_tab.get(), &EntryTab::expensesChanged);
    amount()->setValue(5.00);
    saveButton()->click();

    QCOMPARE(spy.count(), 1);
    const QDate today = QDate::currentDate();
    const QVector<Expense> month = m_expenses->forMonth(today.year(), today.month());
    QCOMPARE(month.size(), 2);
    QCOMPARE(table()->rowCount(), 2);
}

void EntryTabTest::deleteExpenseThroughForm()
{
    addExpenseToday(800);
    QCOMPARE(table()->rowCount(), 1);

    QSignalSpy spy(m_tab.get(), &EntryTab::expensesChanged);

    table()->selectRow(0);
    acceptNextQuestionWithYes();
    deleteButton()->click();

    QCOMPARE(spy.count(), 1);
    const QDate today = QDate::currentDate();
    QVERIFY(m_expenses->forMonth(today.year(), today.month()).isEmpty());
    QCOMPARE(table()->rowCount(), 0);
}

int main(int argc, char *argv[])
{
    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QApplication app(argc, argv);
    QLocale::setDefault(QLocale(QLocale::French, QLocale::France));
    EntryTabTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_entrytab.moc"
