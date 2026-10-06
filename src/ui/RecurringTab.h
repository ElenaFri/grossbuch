#pragma once

#include "core/RecurringExpense.h"

#include <QVector>
#include <QWidget>

#include <optional>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace grossbuch {

class CategoryRepository;
class RecurringRepository;

// Onglet Paiements récurrents : formulaire de définition d'un paiement qui
// revient chaque mois, et liste des paiements existants avec modification (pour
// l'avenir) et désactivation/réactivation. Voir docs/adr/0010 et Phase 8 du plan.
class RecurringTab : public QWidget
{
    Q_OBJECT

public:
    RecurringTab(CategoryRepository &categories, RecurringRepository &recurring,
                 QWidget *parent = nullptr);

    // Recharge les catégories et la liste des paiements récurrents.
    void refresh();

signals:
    // Émis quand un paiement est créé, modifié ou (dés)activé : les occurrences
    // matérialisées peuvent avoir changé, les autres onglets doivent se rafraîchir.
    void recurringChanged();

private slots:
    void onSave();
    void onEditSelected();
    void onToggleActiveSelected();
    void onCancelEdit();
    void updateButtonsState();

private:
    void populateCategories();
    void reloadList();
    void enterEditMode(const RecurringExpense &recurring);
    void leaveEditMode();

    CategoryRepository &m_categories;
    RecurringRepository &m_recurring;

    QDoubleSpinBox *m_amount = nullptr;
    QComboBox *m_category = nullptr;
    QLineEdit *m_label = nullptr;
    QSpinBox *m_day = nullptr;
    QDateEdit *m_start = nullptr;
    QPushButton *m_save = nullptr;
    QPushButton *m_cancel = nullptr;
    QLabel *m_feedback = nullptr;

    QTableWidget *m_table = nullptr;
    QPushButton *m_edit = nullptr;
    QPushButton *m_toggle = nullptr;

    QVector<RecurringExpense> m_models;
    std::optional<int> m_editingId;
};

} // namespace grossbuch
