#pragma once

#include <QMainWindow>

namespace grossbuch {

class CategoryRepository;
class ExpenseRepository;

// Fenêtre principale : un QTabWidget à trois onglets (Saisie, Récapitulatif,
// Graphiques). Les dépôts du cœur métier sont injectés par référence et seront
// consommés par les onglets au fil des phases suivantes. Voir docs/adr/0004.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(CategoryRepository &categories, ExpenseRepository &expenses,
               QWidget *parent = nullptr);

private:
    CategoryRepository &m_categories;
    ExpenseRepository &m_expenses;
};

} // namespace grossbuch
