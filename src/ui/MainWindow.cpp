#include "ui/MainWindow.h"

#include "core/CategoryRepository.h"
#include "core/ExpenseRepository.h"
#include "ui/ChartsTab.h"
#include "ui/EntryTab.h"
#include "ui/SummaryTab.h"

#include <QCloseEvent>
#include <QIcon>
#include <QTabWidget>

namespace grossbuch {

MainWindow::MainWindow(CategoryRepository &categories, ExpenseRepository &expenses, QWidget *parent)
    : QMainWindow(parent), m_categories(categories), m_expenses(expenses)
{
    setWindowTitle(QStringLiteral("grossbuch"));
    // Icône de thème en attendant l'icône dédiée (Phase 8).
    setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-calculator")));
    resize(900, 600);

    m_entryTab = new EntryTab(m_categories, m_expenses);
    m_summaryTab = new SummaryTab(m_categories, m_expenses);
    m_chartsTab = new ChartsTab(m_expenses);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(m_entryTab, tr("Saisie"));
    m_tabs->addTab(m_summaryTab, tr("Récapitulatif"));
    m_tabs->addTab(m_chartsTab, tr("Graphiques"));
    setCentralWidget(m_tabs);

    connect(m_entryTab, &EntryTab::expensesChanged, this, &MainWindow::onExpensesChanged);

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

} // namespace grossbuch
