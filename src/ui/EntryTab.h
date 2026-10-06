#pragma once

#include "core/Expense.h"

#include <QVector>
#include <QWidget>

#include <optional>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace grossbuch {

class CategoryRepository;
class ExpenseRepository;

// Onglet de saisie : formulaire d'ajout d'une dépense et liste des dépenses du
// mois en cours, avec édition et suppression. Voir Phase 4 du plan.
class EntryTab : public QWidget
{
    Q_OBJECT

public:
    EntryTab(CategoryRepository &categories, ExpenseRepository &expenses,
             QWidget *parent = nullptr);

    // Recharge les catégories et la liste des dépenses du mois.
    void refresh();

signals:
    // Émis après tout ajout, modification ou suppression de dépense.
    void expensesChanged();

private slots:
    void onSave();
    void onEditSelected();
    void onDeleteSelected();
    void onCancelEdit();
    void updateButtonsState();

private:
    void populateCategories();
    void reloadExpenses();
    void enterEditMode(const Expense &expense);
    void leaveEditMode();

    CategoryRepository &m_categories;
    ExpenseRepository &m_expenses;

    QDoubleSpinBox *m_amount = nullptr;
    QDateEdit *m_date = nullptr;
    QLineEdit *m_label = nullptr;
    QComboBox *m_category = nullptr;
    QPushButton *m_save = nullptr;
    QPushButton *m_cancel = nullptr;
    QLabel *m_feedback = nullptr;

    QTableWidget *m_table = nullptr;
    QPushButton *m_edit = nullptr;
    QPushButton *m_delete = nullptr;

    QVector<Expense> m_monthExpenses;
    std::optional<int> m_editingId;
};

} // namespace grossbuch
