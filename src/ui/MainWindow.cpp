#include "ui/MainWindow.h"

#include "core/CategoryRepository.h"
#include "core/ExpenseRepository.h"
#include "ui/EntryTab.h"

#include <QIcon>
#include <QLabel>
#include <QTabWidget>

namespace grossbuch {

namespace {

// Contenu temporaire des onglets encore à construire (Phases 5 et 6).
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

    auto *tabs = new QTabWidget(this);
    tabs->addTab(m_entryTab, tr("Saisie"));
    tabs->addTab(makePlaceholder(tr("Récapitulatif mensuel")), tr("Récapitulatif"));
    tabs->addTab(makePlaceholder(tr("Graphiques annuels")), tr("Graphiques"));
    setCentralWidget(tabs);

    connect(m_entryTab, &EntryTab::expensesChanged, this, &MainWindow::onExpensesChanged);
}

void MainWindow::onExpensesChanged()
{
    // Les onglets Récapitulatif et Graphiques seront rafraîchis ici une fois
    // construits (Phases 5 et 6).
}

} // namespace grossbuch
