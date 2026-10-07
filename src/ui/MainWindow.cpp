#include "ui/MainWindow.h"

#include "core/CategoryRepository.h"
#include "core/Database.h"
#include "core/DataController.h"
#include "core/ExpenseRepository.h"
#include "ui/ChartsTab.h"
#include "ui/DataTab.h"
#include "ui/EntryTab.h"
#include "ui/RecurringTab.h"
#include "ui/SummaryTab.h"

#include <QApplication>
#include <QCloseEvent>
#include <QIcon>
#include <QProcess>
#include <QTabWidget>

namespace grossbuch {

MainWindow::MainWindow(Database &database, CategoryRepository &categories,
                       ExpenseRepository &expenses, RecurringRepository &recurring, QWidget *parent)
    : QMainWindow(parent), m_database(database), m_categories(categories), m_expenses(expenses),
      m_recurring(recurring)
{
    setWindowTitle(QStringLiteral("grossbuch"));
    // Icône de thème en attendant l'icône dédiée (Phase 9).
    setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-calculator")));
    resize(900, 600);

    m_dataController = new DataController(m_database, BackupService::defaultBackupDirectory(), this);

    m_entryTab = new EntryTab(m_categories, m_expenses);
    m_recurringTab = new RecurringTab(m_categories, m_recurring);
    m_summaryTab = new SummaryTab(m_categories, m_expenses);
    m_chartsTab = new ChartsTab(m_expenses);
    m_dataTab = new DataTab(*m_dataController);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(m_entryTab, tr("Saisie"));
    m_tabs->addTab(m_recurringTab, tr("Récurrents"));
    m_tabs->addTab(m_summaryTab, tr("Récapitulatif"));
    m_tabs->addTab(m_chartsTab, tr("Graphiques"));
    m_tabs->addTab(m_dataTab, tr("Données"));
    setCentralWidget(m_tabs);

    connect(m_entryTab, &EntryTab::expensesChanged, this, &MainWindow::onExpensesChanged);
    connect(m_recurringTab, &RecurringTab::recurringChanged, this,
            &MainWindow::onRecurringChanged);
    connect(m_dataController, &DataController::dataChanged, this, &MainWindow::refreshAllTabs);
    connect(m_dataTab, &DataTab::restoreCompleted, this, &MainWindow::onRestoreCompleted);

    // Restaure l'état sauvegardé lors de la dernière session.
    const QByteArray geometry = m_settings.value(QStringLiteral("ui/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    const int lastTab = m_settings.value(QStringLiteral("ui/currentTab"), 0).toInt();
    if (lastTab >= 0 && lastTab < m_tabs->count())
        m_tabs->setCurrentIndex(lastTab);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_settings.setValue(QStringLiteral("ui/geometry"), saveGeometry());
    m_settings.setValue(QStringLiteral("ui/currentTab"), m_tabs->currentIndex());
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

void MainWindow::refreshAllTabs()
{
    m_entryTab->refresh();
    m_recurringTab->refresh();
    m_summaryTab->refresh();
    m_chartsTab->refresh();
    m_dataTab->refresh();
}

void MainWindow::onRestoreCompleted()
{
    QProcess::startDetached(QApplication::applicationFilePath(), QApplication::arguments());
    QApplication::quit();
}

} // namespace grossbuch
