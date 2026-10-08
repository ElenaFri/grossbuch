#pragma once

#include <QMainWindow>
#include <QSettings>

class QAction;
class QActionGroup;
class QKeySequence;
class QMenu;
class QStackedWidget;

namespace grossbuch {

class Database;
class CategoryRepository;
class ExpenseRepository;
class RecurringRepository;
class DataController;
class SyncController;
class EntryTab;
class RecurringTab;
class SummaryTab;
class ChartsTab;

// Fenêtre principale pilotée par une barre de menus (Fichier, Édition, Affichage,
// Aide). Le contenu est une pile de vues (QStackedWidget) dont une seule est
// affichée à la fois ; la navigation passe exclusivement par les menus et les
// raccourcis Ctrl+1 à Ctrl+4. Au lancement, la vue Graphiques est affichée. Les
// dépôts du cœur métier sont injectés par référence. Voir docs/adr/0004 et 0016.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(Database &database, CategoryRepository &categories, ExpenseRepository &expenses,
               RecurringRepository &recurring, QWidget *parent = nullptr);

protected:
    // Sauvegarde la géométrie de la fenêtre.
    void closeEvent(QCloseEvent *event) override;

private slots:
    // Rafraîchit les vues dépendantes après une modification des dépenses.
    void onExpensesChanged();

    // Rafraîchit les vues après une modification des paiements récurrents
    // (la matérialisation a pu créer ou retirer des occurrences).
    void onRecurringChanged();

    // Actions du menu Fichier.
    void onImport();
    void onExport();
    void onRestore();

    // Actions de synchronisation distante (menu Fichier).
    void onSyncNow();
    void onConfigureSync();

    // Actions du menu Aide.
    void onAbout();
    void onGuide();

private:
    void createMenus();
    void setCurrentView(int index);
    void refreshAllViews();
    QAction *addViewAction(QMenu *menu, const QString &text, int index,
                           const QKeySequence &shortcut);

    Database &m_database;
    CategoryRepository &m_categories;
    ExpenseRepository &m_expenses;
    RecurringRepository &m_recurring;
    DataController *m_dataController = nullptr;
    SyncController *m_syncController = nullptr;

    // Stocke l'état de l'interface (géométrie) dans ~/.config, séparément des
    // données (constructeur explicite org/app : ne touche pas au chemin
    // AppDataLocation de la base). Voir Phase 7.
    QSettings m_settings{QStringLiteral("grossbuch"), QStringLiteral("grossbuch")};

    QStackedWidget *m_views = nullptr;
    QActionGroup *m_viewActions = nullptr;

    EntryTab *m_entryTab = nullptr;
    RecurringTab *m_recurringTab = nullptr;
    SummaryTab *m_summaryTab = nullptr;
    ChartsTab *m_chartsTab = nullptr;
};

} // namespace grossbuch
