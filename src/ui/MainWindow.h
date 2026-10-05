#pragma once

#include <QMainWindow>
#include <QSettings>

class QTabWidget;

namespace grossbuch {

class CategoryRepository;
class ExpenseRepository;
class EntryTab;
class SummaryTab;
class ChartsTab;

// Fenêtre principale : un QTabWidget à trois onglets (Saisie, Récapitulatif,
// Graphiques). Les dépôts du cœur métier sont injectés par référence et
// consommés par les onglets. Voir docs/adr/0004.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(CategoryRepository &categories, ExpenseRepository &expenses,
               QWidget *parent = nullptr);

protected:
    // Sauvegarde la géométrie et l'onglet courant à la fermeture.
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Rafraîchit les onglets dépendants après une modification des dépenses.
    void onExpensesChanged();

private:
    CategoryRepository &m_categories;
    ExpenseRepository &m_expenses;

    // Stocke l'état de l'interface (géométrie, dernier onglet) dans ~/.config,
    // séparément des données (constructeur explicite org/app : ne touche pas au
    // chemin AppDataLocation de la base). Voir Phase 7.
    QSettings m_settings{QStringLiteral("grossbuch"), QStringLiteral("grossbuch")};

    QTabWidget *m_tabs = nullptr;
    EntryTab *m_entryTab = nullptr;
    SummaryTab *m_summaryTab = nullptr;
    ChartsTab *m_chartsTab = nullptr;
};

} // namespace grossbuch
