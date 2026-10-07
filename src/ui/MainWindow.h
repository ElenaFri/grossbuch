#pragma once

#include <QMainWindow>
#include <QSettings>

class QTabWidget;

namespace grossbuch {

class Database;
class CategoryRepository;
class ExpenseRepository;
class RecurringRepository;
class DataController;
class EntryTab;
class RecurringTab;
class SummaryTab;
class ChartsTab;
class DataTab;

// Fenêtre principale : un QTabWidget à cinq onglets (Saisie, Récurrents,
// Récapitulatif, Graphiques, Données). Les dépôts du cœur métier sont injectés
// par référence et consommés par les onglets. Voir docs/adr/0004.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Database &database, CategoryRepository &categories, ExpenseRepository &expenses,
               RecurringRepository &recurring, QWidget *parent = nullptr);

protected:
    // Sauvegarde la géométrie et l'onglet courant à la fermeture.
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Rafraîchit les onglets dépendants après une modification des dépenses.
    void onExpensesChanged();

    // Rafraîchit tous les onglets après une modification des paiements récurrents
    // (la matérialisation a pu créer ou retirer des occurrences).
    void onRecurringChanged();

    // Redémarre l'application après une restauration de sauvegarde.
    void onRestoreCompleted();

private:
    void refreshAllTabs();

    Database &m_database;
    CategoryRepository &m_categories;
    ExpenseRepository &m_expenses;
    RecurringRepository &m_recurring;
    DataController *m_dataController = nullptr;

    // Stocke l'état de l'interface (géométrie, dernier onglet) dans ~/.config,
    // séparément des données (constructeur explicite org/app : ne touche pas au
    // chemin AppDataLocation de la base). Voir Phase 7.
    QSettings m_settings{QStringLiteral("grossbuch"), QStringLiteral("grossbuch")};

    QTabWidget *m_tabs = nullptr;
    EntryTab *m_entryTab = nullptr;
    RecurringTab *m_recurringTab = nullptr;
    SummaryTab *m_summaryTab = nullptr;
    ChartsTab *m_chartsTab = nullptr;
    DataTab *m_dataTab = nullptr;
};

} // namespace grossbuch
