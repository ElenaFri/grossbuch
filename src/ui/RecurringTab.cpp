#include "ui/RecurringTab.h"

#include "core/Category.h"
#include "core/CategoryRepository.h"
#include "core/RecurringRepository.h"
#include "ui/UiHelpers.h"

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
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace grossbuch {

namespace {

// Jour maximal autorisé : 28, pour qu'une occurrence existe dans tous les mois
// (février compris). Voir docs/adr/0010.
constexpr int kMaxDayOfMonth = 28;

} // namespace

RecurringTab::RecurringTab(CategoryRepository &categories, RecurringRepository &recurring,
                           QWidget *parent)
    : QWidget(parent), m_categories(categories), m_recurring(recurring)
{
    // --- Formulaire ---
    m_amount = new QDoubleSpinBox;
    m_amount->setObjectName(QStringLiteral("recAmountSpin"));
    m_amount->setDecimals(2);
    m_amount->setMaximum(99999999.99);
    m_amount->setSuffix(QStringLiteral(" €"));
    m_amount->setGroupSeparatorShown(true);

    m_category = new QComboBox;
    m_category->setObjectName(QStringLiteral("recCategoryCombo"));

    m_label = new QLineEdit;
    m_label->setObjectName(QStringLiteral("recLabelEdit"));
    m_label->setPlaceholderText(tr("Libellé (ex. : loyer, abonnement)"));

    m_day = new QSpinBox;
    m_day->setObjectName(QStringLiteral("recDaySpin"));
    m_day->setRange(1, kMaxDayOfMonth);
    m_day->setValue(1);

    m_start = new QDateEdit(QDate::currentDate());
    m_start->setObjectName(QStringLiteral("recStartEdit"));
    m_start->setCalendarPopup(true);
    m_start->setDisplayFormat(QStringLiteral("MM/yyyy"));

    auto *form = new QFormLayout;
    form->addRow(tr("Montant :"), m_amount);
    form->addRow(tr("Catégorie :"), m_category);
    form->addRow(tr("Libellé :"), m_label);
    form->addRow(tr("Jour du mois :"), m_day);
    form->addRow(tr("À partir de :"), m_start);

    m_save = new QPushButton(tr("Ajouter"));
    m_save->setObjectName(QStringLiteral("recSaveButton"));
    m_save->setDefault(true);
    m_cancel = new QPushButton(tr("Annuler la modification"));
    m_cancel->setObjectName(QStringLiteral("recCancelButton"));
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

    auto *formBox = new QGroupBox(tr("Paiement récurrent"));
    formBox->setLayout(formLayout);

    // --- Liste ---
    m_table = new QTableWidget;
    m_table->setObjectName(QStringLiteral("recurringTable"));
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels(
        {tr("Libellé"), tr("Catégorie"), tr("Montant"), tr("Jour"), tr("Début"), tr("État")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    m_edit = new QPushButton(tr("Modifier"));
    m_edit->setObjectName(QStringLiteral("recEditButton"));
    m_toggle = new QPushButton(tr("Désactiver"));
    m_toggle->setObjectName(QStringLiteral("recToggleButton"));
    m_edit->setEnabled(false);
    m_toggle->setEnabled(false);

    auto *tableButtons = new QHBoxLayout;
    tableButtons->addWidget(m_edit);
    tableButtons->addWidget(m_toggle);
    tableButtons->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(formBox);
    layout->addWidget(new QLabel(tr("Paiements récurrents définis")));
    layout->addWidget(m_table);
    layout->addLayout(tableButtons);

    // --- Connexions ---
    connect(m_save, &QPushButton::clicked, this, &RecurringTab::onSave);
    connect(m_cancel, &QPushButton::clicked, this, &RecurringTab::onCancelEdit);
    connect(m_edit, &QPushButton::clicked, this, &RecurringTab::onEditSelected);
    connect(m_toggle, &QPushButton::clicked, this, &RecurringTab::onToggleActiveSelected);
    connect(m_table, &QTableWidget::itemSelectionChanged, this, &RecurringTab::updateButtonsState);
    connect(m_table, &QTableWidget::doubleClicked, this, &RecurringTab::onEditSelected);

    refresh();
}

void RecurringTab::refresh()
{
    populateCategories();
    reloadList();
}

void RecurringTab::populateCategories()
{
    populateCategoryCombo(m_category, m_categories.all());
}

void RecurringTab::reloadList()
{
    m_models = m_recurring.all();

    const QHash<int, Category> byId = categoriesById(m_categories.all());

    const QLocale locale;
    m_table->setRowCount(m_models.size());
    for (int row = 0; row < m_models.size(); ++row) {
        const RecurringExpense &model = m_models.at(row);

        auto *labelItem = new QTableWidgetItem(model.label);
        auto *categoryItem = new QTableWidgetItem(categoryDisplayName(byId, model.categoryId));
        auto *amountItem = new QTableWidgetItem(formatMoney(model.amountCents));
        amountItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        auto *dayItem = new QTableWidgetItem(QString::number(model.dayOfMonth));
        dayItem->setTextAlignment(Qt::AlignCenter);
        auto *startItem = new QTableWidgetItem(
            locale.toString(QDate(model.startYear, model.startMonth, 1),
                            QStringLiteral("MMMM yyyy")));
        auto *stateItem =
            new QTableWidgetItem(model.active ? tr("Actif") : tr("Désactivé"));

        m_table->setItem(row, 0, labelItem);
        m_table->setItem(row, 1, categoryItem);
        m_table->setItem(row, 2, amountItem);
        m_table->setItem(row, 3, dayItem);
        m_table->setItem(row, 4, startItem);
        m_table->setItem(row, 5, stateItem);
    }
    m_table->resizeColumnsToContents();
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    updateButtonsState();
}

void RecurringTab::onSave()
{
    const int categoryId = m_category->currentData().toInt();
    if (categoryId <= 0) {
        showFeedback(m_feedback, tr("Veuillez choisir une catégorie."), true);
        return;
    }
    const qint64 cents = std::llround(m_amount->value() * 100.0);
    if (cents <= 0) {
        showFeedback(m_feedback, tr("Le montant doit être supérieur à zéro."), true);
        return;
    }

    RecurringExpense model;
    model.amountCents = cents;
    model.label = m_label->text().trimmed();
    model.categoryId = categoryId;
    model.dayOfMonth = m_day->value();

    const bool editing = m_editingId.has_value();
    bool ok = false;
    if (editing) {
        model.id = *m_editingId;
        ok = m_recurring.update(model);
    } else {
        model.startYear = m_start->date().year();
        model.startMonth = m_start->date().month();
        const std::optional<int> id = m_recurring.add(model);
        ok = id.has_value();
        // Reporte immédiatement les occurrences dues (rattrapage depuis le début).
        if (ok)
            m_recurring.materializeDueOccurrences(QDate::currentDate());
    }

    if (!ok) {
        showFeedback(m_feedback, tr("Échec de l'enregistrement."), true);
        return;
    }

    leaveEditMode();
    reloadList();
    emit recurringChanged();
    showFeedback(m_feedback, editing ? tr("Paiement modifié.") : tr("Paiement ajouté."));
}

void RecurringTab::onEditSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_models.size())
        return;
    enterEditMode(m_models.at(row));
}

void RecurringTab::onToggleActiveSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= m_models.size())
        return;

    const RecurringExpense model = m_models.at(row);
    bool ok = false;
    if (model.active) {
        ok = m_recurring.deactivate(model.id);
    } else {
        ok = m_recurring.reactivate(model.id, QDate::currentDate());
        // Reprise au mois courant : matérialise l'occurrence du mois en cours.
        if (ok)
            m_recurring.materializeDueOccurrences(QDate::currentDate());
    }

    if (!ok) {
        showFeedback(m_feedback, tr("Opération impossible."), true);
        return;
    }

    reloadList();
    emit recurringChanged();
    showFeedback(m_feedback, model.active ? tr("Paiement désactivé.") : tr("Paiement réactivé."));
}

void RecurringTab::onCancelEdit()
{
    leaveEditMode();
}

void RecurringTab::enterEditMode(const RecurringExpense &recurring)
{
    m_editingId = recurring.id;
    m_amount->setValue(static_cast<double>(recurring.amountCents) / 100.0);
    m_label->setText(recurring.label);
    m_day->setValue(recurring.dayOfMonth);
    selectComboCategory(m_category, recurring.categoryId);

    // Le mois de début ne se modifie pas après coup (il fige l'historique).
    m_start->setDate(QDate(recurring.startYear, recurring.startMonth, 1));
    m_start->setEnabled(false);

    m_save->setText(tr("Mettre à jour"));
    m_cancel->setVisible(true);
    m_amount->setFocus();
}

void RecurringTab::leaveEditMode()
{
    m_editingId.reset();
    m_amount->setValue(0.0);
    m_label->clear();
    m_day->setValue(1);
    m_start->setDate(QDate::currentDate());
    m_start->setEnabled(true);

    m_save->setText(tr("Ajouter"));
    m_cancel->setVisible(false);
}

void RecurringTab::updateButtonsState()
{
    const int row = m_table->currentRow();
    const bool hasSelection = row >= 0 && row < m_models.size()
                              && !m_table->selectedItems().isEmpty();
    m_edit->setEnabled(hasSelection);
    m_toggle->setEnabled(hasSelection);
    if (hasSelection)
        m_toggle->setText(m_models.at(row).active ? tr("Désactiver") : tr("Réactiver"));
    else
        m_toggle->setText(tr("Désactiver"));
}

} // namespace grossbuch
