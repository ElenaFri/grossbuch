#include "ui/SummaryTab.h"

#include "core/Category.h"
#include "core/CategoryRepository.h"
#include "core/CsvExport.h"
#include "core/ExpenseRepository.h"
#include "ui/UiHelpers.h"

#include <QComboBox>
#include <QDate>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPair>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

namespace grossbuch {

namespace {

void setAmount(QTreeWidgetItem *item, qint64 cents)
{
    item->setText(1, formatMoney(cents));
    item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
}

// Valeurs du sélecteur de mode.
constexpr int ModeMonth = 0;
constexpr int ModeYear = 1;

} // namespace

SummaryTab::SummaryTab(CategoryRepository &categories, ExpenseRepository &expenses, QWidget *parent)
    : QWidget(parent), m_categories(categories), m_expenses(expenses)
{
    // --- Sélecteur de période ---
    m_mode = new QComboBox;
    m_mode->setObjectName(QStringLiteral("modeCombo"));
    m_mode->addItem(tr("Mois"), ModeMonth);
    m_mode->addItem(tr("Année"), ModeYear);

    m_month = new QComboBox;
    m_month->setObjectName(QStringLiteral("monthCombo"));
    const QLocale locale;
    for (int month = 1; month <= 12; ++month)
        m_month->addItem(locale.standaloneMonthName(month), month);

    m_year = new QComboBox;
    m_year->setObjectName(QStringLiteral("yearCombo"));

    auto *exportButton = new QPushButton(tr("Exporter en CSV…"));
    exportButton->setObjectName(QStringLiteral("exportButton"));

    auto *selectors = new QHBoxLayout;
    selectors->addWidget(new QLabel(tr("Période :")));
    selectors->addWidget(m_mode);
    selectors->addWidget(m_month);
    selectors->addWidget(m_year);
    selectors->addStretch();
    selectors->addWidget(exportButton);

    // --- Tableau hiérarchique ---
    m_tree = new QTreeWidget;
    m_tree->setObjectName(QStringLiteral("summaryTree"));
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({tr("Catégorie"), tr("Montant")});
    m_tree->setRootIsDecorated(true);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    m_total = new QLabel;
    m_total->setObjectName(QStringLiteral("totalLabel"));

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(selectors);
    layout->addWidget(m_tree);
    layout->addWidget(m_total);

    // Par défaut : mois en cours.
    const QDate today = QDate::currentDate();
    m_month->setCurrentIndex(m_month->findData(today.month()));

    connect(m_mode, &QComboBox::currentIndexChanged, this, &SummaryTab::updateView);
    connect(m_month, &QComboBox::currentIndexChanged, this, &SummaryTab::updateView);
    connect(m_year, &QComboBox::currentIndexChanged, this, &SummaryTab::updateView);
    connect(exportButton, &QPushButton::clicked, this, &SummaryTab::onExport);

    refresh();
}

void SummaryTab::refresh()
{
    populateYears();
    updateView();
}

void SummaryTab::populateYears()
{
    const int previous = m_year->currentData().isValid() ? m_year->currentData().toInt() : 0;
    const int currentYear = QDate::currentDate().year();

    QVector<int> years = m_expenses.availableYears();
    if (!years.contains(currentYear))
        years.append(currentYear);
    std::sort(years.begin(), years.end(), std::greater<int>());

    // Bloque les signaux pour ne pas déclencher plusieurs recalculs pendant le
    // remplissage ; le recalcul est fait une seule fois par refresh().
    const QSignalBlocker blocker(m_year);
    m_year->clear();
    for (int year : years)
        m_year->addItem(QString::number(year), year);

    const int target = previous > 0 ? previous : currentYear;
    const int index = m_year->findData(target);
    m_year->setCurrentIndex(index >= 0 ? index : 0);
}

void SummaryTab::updateView()
{
    const bool monthly = m_mode->currentData().toInt() == ModeMonth;
    m_month->setVisible(monthly);

    const int year = m_year->currentData().toInt();
    if (year <= 0)
        return;

    const QVector<CategoryTotal> totals =
        monthly ? m_expenses.totalsByCategory(year, m_month->currentData().toInt())
                : m_expenses.totalsByCategoryForYear(year);

    QHash<int, qint64> byCategory;
    for (const CategoryTotal &total : totals)
        byCategory.insert(total.categoryId, total.amountCents);
    m_currentAmounts = byCategory;

    const qint64 grandTotal = populateTree(byCategory);

    const QLocale locale;
    QString period;
    if (monthly) {
        const QString name = locale.standaloneMonthName(m_month->currentData().toInt());
        // Élision : « d'octobre », mais « de mars ».
        const bool vowel = !name.isEmpty()
                           && QStringLiteral("aàâeéèêiîou")
                                  .contains(name.at(0).toLower());
        period = (vowel ? QStringLiteral("d'") : QStringLiteral("de "))
                 + QStringLiteral("%1 %2").arg(name).arg(year);
    } else {
        period = tr("de l'année %1").arg(year);
    }
    m_total->setText(tr("Total %1 : %2").arg(period, formatMoney(grandTotal)));
}

void SummaryTab::onExport()
{
    const QString suggested =
        m_mode->currentData().toInt() == ModeMonth
            ? tr("recapitulatif-%1-%2.csv")
                  .arg(m_year->currentData().toInt())
                  .arg(m_month->currentData().toInt(), 2, 10, QLatin1Char('0'))
            : tr("recapitulatif-%1.csv").arg(m_year->currentData().toInt());

    const QString path = QFileDialog::getSaveFileName(
        this, tr("Exporter le récapitulatif"), suggested,
        tr("Fichiers CSV (*.csv)"));
    if (path.isEmpty())
        return;

    const QString csv = summaryToCsv(m_categories.all(), m_currentAmounts);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export impossible"),
                             tr("Impossible d'écrire le fichier :\n%1").arg(path));
        return;
    }
    // BOM UTF-8 pour que les accents et le symbole € s'affichent dans Excel.
    file.write("\xEF\xBB\xBF");
    file.write(csv.toUtf8());
    if (!file.commit()) {
        QMessageBox::warning(this, tr("Export impossible"),
                             tr("Impossible d'écrire le fichier :\n%1").arg(path));
    }
}

qint64 SummaryTab::populateTree(const QHash<int, qint64> &byCategory)
{
    // Construit la hiérarchie des catégories.
    QVector<Category> roots;
    QHash<int, QVector<Category>> childrenByParent;
    for (const Category &category : m_categories.all()) {
        if (category.isRoot())
            roots.append(category);
        else
            childrenByParent[category.parentId.value()].append(category);
    }

    auto byName = [](const Category &a, const Category &b) { return a.name < b.name; };
    std::sort(roots.begin(), roots.end(), byName);

    m_tree->clear();
    qint64 grandTotal = 0;
    for (const Category &root : roots)
        grandTotal += appendRootItem(root, childrenByParent.value(root.id), byCategory);

    m_tree->expandAll();
    return grandTotal;
}

qint64 SummaryTab::appendRootItem(const Category &root, QVector<Category> children,
                                  const QHash<int, qint64> &byCategory)
{
    if (children.isEmpty()) {
        // Racine sans enfant : les dépenses lui sont rattachées directement.
        const qint64 amount = byCategory.value(root.id);
        if (amount == 0)
            return 0;
        auto *item = new QTreeWidgetItem(m_tree);
        item->setText(0, root.name);
        setAmount(item, amount);
        return amount;
    }

    // Racine avec enfants : son total est la somme des sous-catégories dépensées.
    std::sort(children.begin(), children.end(),
              [](const Category &a, const Category &b) { return a.name < b.name; });

    QVector<QPair<QString, qint64>> rows;
    qint64 rootTotal = 0;
    for (const Category &child : children) {
        const qint64 amount = byCategory.value(child.id);
        if (amount == 0)
            continue;
        rows.append({child.name, amount});
        rootTotal += amount;
    }
    if (rootTotal == 0)
        return 0;

    auto *rootItem = new QTreeWidgetItem(m_tree);
    rootItem->setText(0, root.name);
    setAmount(rootItem, rootTotal);
    for (const auto &row : rows) {
        auto *childItem = new QTreeWidgetItem(rootItem);
        childItem->setText(0, row.first);
        setAmount(childItem, row.second);
    }
    return rootTotal;
}

} // namespace grossbuch
