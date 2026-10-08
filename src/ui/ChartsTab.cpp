#include "ui/ChartsTab.h"

#include "core/ExpenseRepository.h"

#include <QBarCategoryAxis>
#include <QChart>
#include <QChartView>
#include <QCheckBox>
#include <QDate>
#include <QHBoxLayout>
#include <QLabel>
#include <QLegend>
#include <QLineSeries>
#include <QLocale>
#include <QPainter>
#include <QSet>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace grossbuch {

namespace {

QStringList shortMonthNames()
{
    const QLocale locale;
    QStringList names;
    for (int month = 1; month <= 12; ++month)
        names << locale.standaloneMonthName(month, QLocale::ShortFormat);
    return names;
}

} // namespace

ChartsTab::ChartsTab(ExpenseRepository &expenses, QWidget *parent)
    : QWidget(parent), m_expenses(expenses)
{
    m_chart = new QChart;
    m_chart->legend()->setVisible(true);
    m_chart->legend()->setAlignment(Qt::AlignBottom);

    m_axisX = new QBarCategoryAxis;
    m_axisX->append(shortMonthNames());
    m_chart->addAxis(m_axisX, Qt::AlignBottom);

    m_axisY = new QValueAxis;
    // Le symbole € ne passe pas par le format d'étiquette (rendu type printf) :
    // on le place dans le titre de l'axe, rendu normalement en UTF-8.
    m_axisY->setLabelFormat(QStringLiteral("%.0f"));
    m_axisY->setTitleText(tr("Montant mensuel (€)"));
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    m_chartView = new QChartView(m_chart);
    m_chartView->setObjectName(QStringLiteral("chartView"));
    m_chartView->setRenderHint(QPainter::Antialiasing);

    auto *controls = new QHBoxLayout;
    controls->addWidget(new QLabel(tr("Années :")));
    m_yearsLayout = new QHBoxLayout;
    controls->addLayout(m_yearsLayout);
    controls->addStretch();

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(controls);
    layout->addWidget(m_chartView, 1);

    refresh();
}

void ChartsTab::refresh()
{
    syncYearCheckboxes();
    rebuildChart();
}

void ChartsTab::syncYearCheckboxes()
{
    const QVector<int> years = m_expenses.availableYears(); // ordre croissant

    // Conserve l'état coché actuel avant reconstruction.
    QSet<int> checked;
    for (auto it = m_yearChecks.cbegin(); it != m_yearChecks.cend(); ++it) {
        if (it.value()->isChecked())
            checked.insert(it.key());
    }

    // Au tout premier remplissage avec des données, on n'affiche que l'année en
    // cours (ou, à défaut de données cette année-là, la plus récente disponible)
    // pour un graphique épuré au lancement. Voir docs/adr/0017.
    if (!m_defaultsApplied && !years.isEmpty()) {
        checked.clear();
        const int currentYear = QDate::currentDate().year();
        const int defaultYear = years.contains(currentYear) ? currentYear : years.last();
        checked.insert(defaultYear);
        m_defaultsApplied = true;
    }

    // Vide les cases existantes.
    while (QLayoutItem *item = m_yearsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    m_yearChecks.clear();

    // Recrée une case par année, les plus récentes à gauche.
    for (int i = years.size() - 1; i >= 0; --i) {
        const int year = years.at(i);
        auto *box = new QCheckBox(QString::number(year));
        box->setObjectName(QStringLiteral("year_%1").arg(year));
        box->setChecked(checked.contains(year));
        connect(box, &QCheckBox::toggled, this, &ChartsTab::rebuildChart);
        m_yearsLayout->addWidget(box);
        m_yearChecks.insert(year, box);
    }
}

void ChartsTab::rebuildChart()
{
    m_chart->removeAllSeries();

    // Années cochées, par ordre croissant pour une légende lisible.
    QVector<int> years = m_yearChecks.keys().toVector();
    std::sort(years.begin(), years.end());

    double maxAmount = 0.0;
    for (int year : years) {
        if (!m_yearChecks.value(year)->isChecked())
            continue;

        auto *series = new QLineSeries;
        series->setName(QString::number(year));
        const std::array<qint64, 12> totals = m_expenses.monthlyTotals(year);
        for (int month = 0; month < 12; ++month) {
            const qint64 cents = totals[static_cast<std::size_t>(month)];
            // Les mois sans dépense ne sont pas traçés du tout.
            if (cents == 0)
                continue;
            const double euros = static_cast<double>(cents) / 100.0;
            series->append(month, euros);
            maxAmount = std::max(maxAmount, euros);
        }
        m_chart->addSeries(series);
        series->attachAxis(m_axisX);
        series->attachAxis(m_axisY);
    }

    // Marge de 10 % au-dessus du maximum ; plage par défaut si aucune donnée.
    m_axisY->setRange(0.0, maxAmount > 0.0 ? maxAmount * 1.1 : 100.0);
    // Arrondit la plage et le pas sur des valeurs rondes et lisibles.
    m_axisY->applyNiceNumbers();
}

} // namespace grossbuch
