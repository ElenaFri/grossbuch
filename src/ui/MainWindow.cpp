#include "ui/MainWindow.h"

#include "core/CategoryRepository.h"
#include "core/ExpenseRepository.h"
#include "ui/EntryTab.h"
#include "ui/SummaryTab.h"

#include <QIcon>
#include <QLabel>
#include <QTabWidget>

namespace grossbuch {

namespace {

// Contenu temporaire des onglets encore à construire (Phase 6).
QWidget *makePlaceholder(const QString &text)
{
    auto *label = new QLabel(text);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

} // namespace

MainWindow::MainWindow(CategoryRepository &categories, ExpenseRepository &expenses, QWidget *parent)
    : QMainWindow(parent), m_categories(categories), m_expenses(expenses)
{
    setWindowTitle(QStringLiteral("grossbuch"));
    // Icône de thème en attendant l'icône dédiée (Phase 8).
    setWindowIcon(QIcon::fromTheme(QStringLiteral("accessories-calculator")));
    resize(900, 600);

    m_entryTab = new EntryTab(m_categories, m_expenses);
    m_summaryTab = new SummaryTab(m_categories, m_expenses);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(m_entryTab, tr("Saisie"));
    tabs->addTab(m_summaryTab, tr("Récapitulatif"));
    tabs->addTab(makePlaceholder(tr("Graphiques annuels")), tr("Graphiques"));
    setCentralWidget(tabs);

    connect(m_entryTab, &EntryTab::expensesChanged, this, &MainWindow::onExpensesChanged);
}

void MainWindow::onExpensesChanged()
{
    // L'onglet Graphiques sera rafraîchi ici une fois construit (Phase 6).
    m_summaryTab->refresh();
}

} // namespace grossbuch
