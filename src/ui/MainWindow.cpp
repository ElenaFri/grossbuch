#include "ui/MainWindow.h"

#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/DataController.h"
#include "core/ExpenseRepository.h"
#include "ui/AboutDialog.h"
#include "ui/ChartsTab.h"
#include "ui/EntryTab.h"
#include "ui/GuideDialog.h"
#include "ui/RecurringTab.h"
#include "ui/RestoreDialog.h"
#include "ui/SummaryTab.h"
#include "ui/SyncController.h"
#include "ui/SyncDialog.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStatusBar>

namespace grossbuch {

// Index des vues dans la pile (et valeurs portées par les actions de navigation).
namespace {
constexpr int ViewEntry = 0;
constexpr int ViewRecurring = 1;
constexpr int ViewSummary = 2;
constexpr int ViewCharts = 3;
} // namespace

MainWindow::MainWindow(Database &database, CategoryRepository &categories,
                       ExpenseRepository &expenses, RecurringRepository &recurring, QWidget *parent)
    : QMainWindow(parent), m_database(database), m_categories(categories), m_expenses(expenses),
      m_recurring(recurring)
{
    setWindowTitle(QStringLiteral("grossbuch"));
    setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-calculator")));
    resize(900, 600);

    m_dataController = new DataController(m_database, BackupService::defaultBackupDirectory(), this);
    m_syncController = new SyncController(m_database, BackupService::defaultBackupDirectory(), this);

    m_entryTab = new EntryTab(m_categories, m_expenses);
    m_recurringTab = new RecurringTab(m_categories, m_recurring);
    m_summaryTab = new SummaryTab(m_categories, m_expenses);
    m_chartsTab = new ChartsTab(m_expenses);

    m_views = new QStackedWidget(this);
    m_views->insertWidget(ViewEntry, m_entryTab);
    m_views->insertWidget(ViewRecurring, m_recurringTab);
    m_views->insertWidget(ViewSummary, m_summaryTab);
    m_views->insertWidget(ViewCharts, m_chartsTab);
    setCentralWidget(m_views);

    createMenus();

    connect(m_entryTab, &EntryTab::expensesChanged, this, &MainWindow::onExpensesChanged);
    connect(m_recurringTab, &RecurringTab::recurringChanged, this,
            &MainWindow::onRecurringChanged);
    connect(m_dataController, &DataController::dataChanged, this, &MainWindow::refreshAllViews);
    connect(m_syncController, &SyncController::imported, this,
            [this](const SyncService::Result &result) {
                refreshAllViews();
                statusBar()->showMessage(result.message, 5000);
            });

    // Restaure la géométrie de la dernière session.
    const QByteArray geometry = m_settings.value(QStringLiteral("ui/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    // Au lancement, on affiche toujours les graphiques (année en cours comprise).
    // Les autres vues s'ouvrent à la demande via les menus.
    setCurrentView(ViewCharts);

    // Synchronisation automatique à l'ouverture : import silencieux si configuré.
    if (m_syncController->autoSync() && m_syncController->isConfigured()) {
        const SyncService::Result result = m_syncController->importNow();
        if (!result.message.isEmpty())
            statusBar()->showMessage(result.message, 5000);
    }
}

void MainWindow::createMenus()
{
    // --- Fichier ------------------------------------------------------------
    QMenu *fileMenu = menuBar()->addMenu(tr("&Fichier"));
    QAction *importAction = fileMenu->addAction(tr("&Importer des données…"));
    importAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_O));
    connect(importAction, &QAction::triggered, this, &MainWindow::onImport);

    QAction *exportAction = fileMenu->addAction(tr("&Exporter des données…"));
    exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExport);

    QAction *restoreAction = fileMenu->addAction(tr("&Restaurer une sauvegarde…"));
    connect(restoreAction, &QAction::triggered, this, &MainWindow::onRestore);

    fileMenu->addSeparator();
    QAction *syncNowAction = fileMenu->addAction(tr("&Synchroniser maintenant"));
    connect(syncNowAction, &QAction::triggered, this, &MainWindow::onSyncNow);

    QAction *configureSyncAction = fileMenu->addAction(tr("&Configurer la synchronisation…"));
    connect(configureSyncAction, &QAction::triggered, this, &MainWindow::onConfigureSync);

    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(tr("&Quitter"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &MainWindow::close);

    // Groupe exclusif pour les vues : une seule cochée à la fois, dans les menus
    // Édition/Affichage comme dans la barre d'outils.
    m_viewActions = new QActionGroup(this);
    m_viewActions->setExclusive(true);

    // --- Édition (vues de saisie) ------------------------------------------
    QMenu *editMenu = menuBar()->addMenu(tr("&Édition"));
    addViewAction(editMenu, tr("&Saisie des dépenses"), ViewEntry,
                  QKeySequence(Qt::CTRL | Qt::Key_1));
    addViewAction(editMenu, tr("&Dépenses récurrentes"), ViewRecurring,
                  QKeySequence(Qt::CTRL | Qt::Key_2));

    // --- Affichage (vues de consultation) ----------------------------------
    QMenu *viewMenu = menuBar()->addMenu(tr("&Affichage"));
    addViewAction(viewMenu, tr("&Récapitulatif"), ViewSummary,
                  QKeySequence(Qt::CTRL | Qt::Key_3));
    addViewAction(viewMenu, tr("&Graphiques"), ViewCharts, QKeySequence(Qt::CTRL | Qt::Key_4));

    // --- Aide ---------------------------------------------------------------
    QMenu *helpMenu = menuBar()->addMenu(tr("&Aide"));
    QAction *guideAction = helpMenu->addAction(tr("&Guide d'utilisation"));
    guideAction->setShortcut(QKeySequence::HelpContents);
    connect(guideAction, &QAction::triggered, this, &MainWindow::onGuide);

    QAction *aboutAction = helpMenu->addAction(tr("À &propos de grossbuch"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
}

QAction *MainWindow::addViewAction(QMenu *menu, const QString &text, int index,
                                   const QKeySequence &shortcut)
{
    QAction *action = menu->addAction(text);
    action->setCheckable(true);
    action->setShortcut(shortcut);
    action->setData(index);
    m_viewActions->addAction(action);
    connect(action, &QAction::triggered, this, [this, index]() { setCurrentView(index); });
    return action;
}

void MainWindow::setCurrentView(int index)
{
    if (index < 0 || index >= m_views->count())
        return;
    m_views->setCurrentIndex(index);
    // Synchronise la coche de l'action correspondante (sans break : S1751).
    const QList<QAction *> actions = m_viewActions->actions();
    for (QAction *action : actions)
        action->setChecked(action->data().toInt() == index);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Synchronisation automatique à la fermeture : export silencieux si configuré.
    if (m_syncController->autoSync() && m_syncController->isConfigured())
        m_syncController->exportNow();

    m_settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    QMainWindow::closeEvent(event);
}

void MainWindow::onExpensesChanged()
{
    m_summaryTab->refresh();
    m_chartsTab->refresh();
}

void MainWindow::onRecurringChanged()
{
    // La matérialisation a pu modifier les dépenses du mois courant : on rafraîchit
    // aussi la saisie, en plus du récapitulatif et des graphiques.
    m_entryTab->refresh();
    m_summaryTab->refresh();
    m_chartsTab->refresh();
}

void MainWindow::refreshAllViews()
{
    m_entryTab->refresh();
    m_recurringTab->refresh();
    m_summaryTab->refresh();
    m_chartsTab->refresh();
}

// Synchronisation manuelle : import (fusion) puis export de l'instantané local.
// Non modal (aucune boîte de dialogue). Un import qui modifie la base émet
// imported() (rafraîchissement et message gérés par le connecteur du constructeur) ;
// sinon on affiche simplement le bilan en barre d'état.
void MainWindow::onSyncNow()
{
    if (!m_syncController->isConfigured()) {
        statusBar()->showMessage(tr("Synchronisation non configurée."), 5000);
        return;
    }

    const SyncService::Result imported = m_syncController->importNow();
    m_syncController->exportNow();
    if (!imported.changed)
        statusBar()->showMessage(imported.message, 5000);
}

// Les slots suivants pilotent des dialogues natifs modaux (QFileDialog,
// QMessageBox) et, pour la restauration, relancent l'application via QProcess.
// Ils ne sont pas couvrables sans automatisation fragile de fenêtres modales :
// exclus de la mesure de couverture. Voir docs/adr/0019.
// LCOV_EXCL_START
void MainWindow::onExport()
{
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString defaultName =
        QStringLiteral("grossbuch_backup_%1.json")
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString suggested = QDir(documents).filePath(defaultName);

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Exporter les données"), suggested, tr("Fichier d'échange (*.json)"));
    if (path.isEmpty())
        return;

    const DataController::Result result = m_dataController->exportToFile(path);
    if (result.ok)
        QMessageBox::information(this, tr("Exporter les données"), result.message);
    else
        QMessageBox::warning(this, tr("Export impossible"), result.message);
}

void MainWindow::onImport()
{
    const QMessageBox::StandardButton confirm = QMessageBox::question(
        this, tr("Importer des données"),
        tr("L'import fusionne le fichier choisi avec vos données actuelles.\n"
           "Une sauvegarde complète est créée automatiquement au préalable.\n\n"
           "Continuer ?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (confirm != QMessageBox::Yes)
        return;

    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Importer des données"), documents, tr("Fichier d'échange (*.json)"));
    if (path.isEmpty())
        return;

    // Un import réussi émet dataChanged() : les vues se rafraîchissent via le signal.
    const DataController::Result result = m_dataController->importFromFile(path);
    if (result.ok)
        QMessageBox::information(this, tr("Importer des données"), result.message);
    else
        QMessageBox::warning(this, tr("Import impossible"), result.message);
}

void MainWindow::onRestore()
{
    RestoreDialog dialog(*m_dataController, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    // La restauration a réussi : la base est fermée, on redémarre l'application.
    QProcess::startDetached(QApplication::applicationFilePath(), QApplication::arguments());
    QApplication::quit();
}

void MainWindow::onAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::onGuide()
{
    GuideDialog dialog(this);
    dialog.exec();
}

void MainWindow::onConfigureSync()
{
    SyncDialog dialog(*m_syncController, this);
    dialog.exec();
}
// LCOV_EXCL_STOP

} // namespace grossbuch
