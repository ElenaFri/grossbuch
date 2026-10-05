#include "ui/EntryTab.h"

#include "core/Category.h"
#include "core/CategoryRepository.h"
#include "core/ExpenseRepository.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace grossbuch {

namespace {

QString formatMoney(qint64 cents)
{
    return QLocale().toCurrencyString(static_cast<double>(cents) / 100.0);
}

} // namespace

EntryTab::EntryTab(CategoryRepository &categories, ExpenseRepository &expenses, QWidget *parent)
    : QWidget(parent), m_categories(categories), m_expenses(expenses)
{
    // --- Formulaire de saisie ---
    m_amount = new QDoubleSpinBox;
    m_amount->setObjectName(QStringLiteral("amountSpin"));
    m_amount->setDecimals(2);
    m_amount->setMaximum(99999999.99);
    m_amount->setSuffix(QStringLiteral(" €"));
    m_amount->setGroupSeparatorShown(true);

    m_date = new QDateEdit(QDate::currentDate());
    m_date->setObjectName(QStringLiteral("dateEdit"));
    m_date->setCalendarPopup(true);
    m_date->setDisplayFormat(QStringLiteral("dd/MM/yyyy"));

    m_label = new QLineEdit;
    m_label->setObjectName(QStringLiteral("labelEdit"));
    m_label->setPlaceholderText(tr("Libellé (optionnel)"));

    m_category = new QComboBox;
    m_category->setObjectName(QStringLiteral("categoryCombo"));

    auto *form = new QFormLayout;
    form->addRow(tr("Montant :"), m_amount);
    form->addRow(tr("Date :"), m_date);
    form->addRow(tr("Libellé :"), m_label);
    form->addRow(tr("Catégorie :"), m_category);

    m_save = new QPushButton(tr("Enregistrer"));
    m_save->setObjectName(QStringLiteral("saveButton"));
    m_save->setDefault(true);
    m_cancel = new QPushButton(tr("Annuler la modification"));
    m_cancel->setObjectName(QStringLiteral("cancelButton"));
    m_cancel->setVisible(false);
    m_feedback = new QLabel;

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_save);
    buttons->addWidget(m_cancel);
    buttons->addWidget(m_feedback);
    buttons->addStretch();

    auto *formLayout = new QVBoxLayout;
    formLayout->addLayout(form);
    formLayout->addLayout(buttons);

    auto *formBox = new QGroupBox(tr("Nouvelle dépense"));
    formBox->setLayout(formLayout);

    // --- Liste des dépenses du mois ---
    m_table = new QTableWidget;
    m_table->setObjectName(QStringLiteral("expensesTable"));
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels(
        {tr("Date"), tr("Catégorie"), tr("Libellé"), tr("Montant")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    m_edit = new QPushButton(tr("Modifier"));
    m_edit->setObjectName(QStringLiteral("editButton"));
    m_delete = new QPushButton(tr("Supprimer"));
    m_delete->setObjectName(QStringLiteral("deleteButton"));
    m_edit->setEnabled(false);
    m_delete->setEnabled(false);

    auto *tableButtons = new QHBoxLayout;
    tableButtons->addWidget(m_edit);
    tableButtons->addWidget(m_delete);
    tableButtons->addStretch();

    const QLocale locale;
    auto *monthTitle = new QLabel(
        tr("Dépenses de %1")
            .arg(locale.toString(QDate::currentDate(), QStringLiteral("MMMM yyyy"))));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(formBox);
    layout->addWidget(monthTitle);
    layout->addWidget(m_table);
    layout->addLayout(tableButtons);

    // --- Connexions ---
    connect(m_save, &QPushButton::clicked, this, &EntryTab::onSave);
    connect(m_cancel, &QPushButton::clicked, this, &EntryTab::onCancelEdit);
    connect(m_edit, &QPushButton::clicked, this, &EntryTab::onEditSelected);
    connect(m_delete, &QPushButton::clicked, this, &EntryTab::onDeleteSelected);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &EntryTab::updateButtonsState);
    connect(m_table, &QTableWidget::doubleClicked, this, &EntryTab::onEditSelected);

    refresh();
}

void EntryTab::refresh()
{
    populateCategories();
    reloadExpenses();
}

void EntryTab::populateCategories()
{
    const int previous = m_category->currentData().isValid() ? m_category->currentData().toInt() : 0;

    m_category->clear();
    auto *model = qobject_cast<QStandardItemModel *>(m_category->model());

    const QVector<Category> all = m_categories.all();

    QVector<Category> roots;
    QHash<int, QVector<Category>> childrenByParent;
    for (const Category &category : all) {
        if (category.isRoot())
            roots.append(category);
        else
            childrenByParent[category.parentId.value()].append(category);
    }

    auto byName = [](const Category &a, const Category &b) { return a.name < b.name; };
    std::sort(roots.begin(), roots.end(), byName);

    int firstSelectable = -1;
    for (const Category &root : roots) {
        QVector<Category> children = childrenByParent.value(root.id);
        if (children.isEmpty()) {
            // Racine sans enfant : sélectionnable directement.
            m_category->addItem(root.name, root.id);
            if (firstSelectable < 0)
                firstSelectable = m_category->count() - 1;
        } else {
            // En-tête non sélectionnable, puis les sous-catégories.
            m_category->addItem(root.name);
            if (model != nullptr)
                model->item(m_category->count() - 1)->setFlags(Qt::NoItemFlags);

            std::sort(children.begin(), children.end(), byName);
            for (const Category &child : children) {
                m_category->addItem(QStringLiteral("    %1").arg(child.name), child.id);
                if (firstSelectable < 0)
                    firstSelectable = m_category->count() - 1;
            }
        }
    }

    if (previous > 0)
        selectCategory(previous);
    if (m_category->currentData().toInt() <= 0 && firstSelectable >= 0)
        m_category->setCurrentIndex(firstSelectable);
}

void EntryTab::reloadExpenses()
{
    const QDate today = QDate::currentDate();
    m_monthExpenses = m_expenses.forMonth(today.year(), today.month());

    QHash<int, Category> byId;
    for (const Category &category : m_categories.all())
        byId.insert(category.id, category);

    auto displayName = [&byId](int categoryId) -> QString {
        const Category category = byId.value(categoryId);
        if (category.id == 0)
            return QString();
        if (category.isRoot())
            return category.name;
        return QStringLiteral("%1 / %2").arg(byId.value(category.parentId.value()).name, category.name);
    };

    m_table->setRowCount(m_monthExpenses.size());
    const QLocale locale;
    for (int row = 0; row < m_monthExpenses.size(); ++row) {
        const Expense &expense = m_monthExpenses.at(row);

        auto *dateItem = new QTableWidgetItem(
            locale.toString(expense.date, QStringLiteral("dd/MM/yyyy")));
        auto *categoryItem = new QTableWidgetItem(displayName(expense.categoryId));
        auto *labelItem = new QTableWidgetItem(expense.label);
        auto *amountItem = new QTableWidgetItem(formatMoney(expense.amountCents));
        amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_table->setItem(row, 0, dateItem);
        m_table->setItem(row, 1, categoryItem);
        m_table->setItem(row, 2, labelItem);
        m_table->setItem(row, 3, amountItem);
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    updateButtonsState();
}

void EntryTab::selectCategory(int categoryId)
{
    for (int i = 0; i < m_category->count(); ++i) {
        if (m_category->itemData(i).toInt() == categoryId) {
            m_category->setCurrentIndex(i);
            return;
        }
    }
}

void EntryTab::onSave()
{
    const int categoryId = m_category->currentData().toInt();
    if (categoryId <= 0) {
        showFeedback(tr("Veuillez choisir une catégorie."), true);
        return;
    }
    const qint64 cents = std::llround(m_amount->value() * 100.0);
    if (cents <= 0) {
        showFeedback(tr("Le montant doit être supérieur à zéro."), true);
        return;
    }

    Expense expense;
    expense.amountCents = cents;
    expense.date = m_date->date();
    expense.label = m_label->text().trimmed();
    expense.categoryId = categoryId;

    bool ok = false;
    if (m_editingId.has_value()) {
        expense.id = *m_editingId;
        ok = m_expenses.update(expense);
    } else {
        ok = m_expenses.add(expense).has_value();
    }

    if (!ok) {
        showFeedback(tr("Échec de l'enregistrement."), true);
        return;
    }

    const bool wasEditing = m_editingId.has_value();
    leaveEditMode();
    reloadExpenses();
    emit expensesChanged();
    showFeedback(wasEditing ? tr("Dépense modifiée.") : tr("Dépense enregistrée."));
}

void EntryTab::onEditSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_monthExpenses.size())
        return;
    enterEditMode(m_monthExpenses.at(row));
}

void EntryTab::onDeleteSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_monthExpenses.size())
        return;

    const Expense expense = m_monthExpenses.at(row);
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Supprimer la dépense"),
        tr("Supprimer cette dépense de %1 ?").arg(formatMoney(expense.amountCents)));
    if (answer != QMessageBox::Yes)
        return;

    if (!m_expenses.remove(expense.id)) {
        showFeedback(tr("Échec de la suppression."), true);
        return;
    }

    if (m_editingId == expense.id)
        leaveEditMode();
    reloadExpenses();
    emit expensesChanged();
    showFeedback(tr("Dépense supprimée."));
}

void EntryTab::onCancelEdit()
{
    leaveEditMode();
}

void EntryTab::enterEditMode(const Expense &expense)
{
    m_editingId = expense.id;
    m_amount->setValue(static_cast<double>(expense.amountCents) / 100.0);
    m_date->setDate(expense.date);
    m_label->setText(expense.label);
    selectCategory(expense.categoryId);

    m_save->setText(tr("Mettre à jour"));
    m_cancel->setVisible(true);
    m_amount->setFocus();
}

void EntryTab::leaveEditMode()
{
    m_editingId.reset();
    m_amount->setValue(0.0);
    m_date->setDate(QDate::currentDate());
    m_label->clear();

    m_save->setText(tr("Enregistrer"));
    m_cancel->setVisible(false);
}

void EntryTab::updateButtonsState()
{
    const bool hasSelection = m_table->currentRow() >= 0 && !m_table->selectedItems().isEmpty();
    m_edit->setEnabled(hasSelection);
    m_delete->setEnabled(hasSelection);
}

void EntryTab::showFeedback(const QString &text, bool error)
{
    m_feedback->setText(text);
    m_feedback->setStyleSheet(error ? QStringLiteral("color: #c0392b;")
                                    : QStringLiteral("color: #27ae60;"));
    QTimer::singleShot(4000, m_feedback, [label = m_feedback] { label->clear(); });
}

} // namespace grossbuch
