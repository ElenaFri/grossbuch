#pragma once

#include <QHash>
#include <QWidget>

class QCheckBox;
class QChart;
class QChartView;
class QBarCategoryAxis;
class QValueAxis;
class QHBoxLayout;

namespace grossbuch {

class ExpenseRepository;

// Onglet Graphiques : une courbe par année (douze points, un par mois) sur le
// total mensuel toutes catégories confondues, les années superposées pour
// comparer d'une année sur l'autre. Les trois dernières années sont affichées
// par défaut, un sélecteur permet d'ajuster. Voir docs/adr/0007 et Phase 6.
class ChartsTab : public QWidget
{
    Q_OBJECT

public:
    explicit ChartsTab(ExpenseRepository &expenses, QWidget *parent = nullptr);

    // Resynchronise la liste des années disponibles et retrace les courbes.
    void refresh();

private slots:
    void rebuildChart();

private:
    void syncYearCheckboxes();

    ExpenseRepository &m_expenses;

    QChart *m_chart = nullptr;
    QChartView *m_chartView = nullptr;
    QBarCategoryAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;

    QHBoxLayout *m_yearsLayout = nullptr;
    QHash<int, QCheckBox *> m_yearChecks;
    bool m_defaultsApplied = false;
};

} // namespace grossbuch
