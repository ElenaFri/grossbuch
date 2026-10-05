#pragma once

#include <QMainWindow>

namespace grossbuch {

class CategoryRepository;
class ExpenseRepository;
class EntryTab;
class SummaryTab;

// Fenêtre principale : un QTabWidget à trois onglets (Saisie, Récapitulatif,
// Graphiques). Les dépôts du cœur métier sont injectés par référence et
// consommés par les onglets. Voir docs/adr/0004.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(CategoryRepository &categories, ExpenseRepository &expenses,
               QWidget *parent = nullptr);

private slots:
    // Rafraîchit les onglets dépendants après une modification des dépenses.
    void onExpensesChanged();

private:
    CategoryRepository &m_categories;
    ExpenseRepository &m_expenses;

    EntryTab *m_entryTab = nullptr;
    SummaryTab *m_summaryTab = nullptr;
};

} // namespace grossbuch
