#pragma once

#include "core/Category.h"

#include <QHash>
#include <QVector>
#include <QWidget>
#include <QtGlobal>

class QComboBox;
class QLabel;
class QTreeWidget;

namespace grossbuch {

class CategoryRepository;
class ExpenseRepository;

// Onglet Récapitulatif : agrège les dépenses par catégorie racine et
// sous-catégorie, sous forme de tableau hiérarchique, pour un mois donné ou une
// année entière au choix. Voir docs/adr/0008 et Phase 5 du plan.
class SummaryTab : public QWidget
{
    Q_OBJECT

public:
    SummaryTab(CategoryRepository &categories, ExpenseRepository &expenses,
               QWidget *parent = nullptr);

    // Recharge les années disponibles et recalcule le tableau.
    void refresh();

private slots:
    void updateView();
    void onExport();

private:
    void populateYears();
    qint64 populateTree(const QHash<int, qint64> &byCategory);
    qint64 appendRootItem(const Category &root, QVector<Category> children,
                          const QHash<int, qint64> &byCategory);

    CategoryRepository &m_categories;
    ExpenseRepository &m_expenses;

    QComboBox *m_mode = nullptr;
    QComboBox *m_month = nullptr;
    QComboBox *m_year = nullptr;
    QTreeWidget *m_tree = nullptr;
    QLabel *m_total = nullptr;

    // Montants de la période actuellement affichée, par identifiant de catégorie ;
    // sert de source pour l'export CSV sans recalculer l'agrégation.
    QHash<int, qint64> m_currentAmounts;
};

} // namespace grossbuch
