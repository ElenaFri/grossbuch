#include "ui/MainWindow.h"

#include "core/CategoryRepository.h"
#include "core/ExpenseRepository.h"

#include <QIcon>
#include <QLabel>
#include <QTabWidget>

namespace grossbuch {

namespace {

// Contenu temporaire des onglets, remplacé par les vrais widgets aux Phases 4 à 6.
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

    auto *tabs = new QTabWidget(this);
    tabs->addTab(makePlaceholder(tr("Saisie d'une dépense")), tr("Saisie"));
    tabs->addTab(makePlaceholder(tr("Récapitulatif mensuel")), tr("Récapitulatif"));
    tabs->addTab(makePlaceholder(tr("Graphiques annuels")), tr("Graphiques"));
    setCentralWidget(tabs);
}

} // namespace grossbuch
