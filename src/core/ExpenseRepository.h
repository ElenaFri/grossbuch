#pragma once

#include "core/Expense.h"

#include <QVector>
#include <QtGlobal>

#include <array>
#include <optional>

namespace grossbuch {

class Database;

// Total des dépenses pour une catégorie donnée sur une période.
struct CategoryTotal
{
    int categoryId = 0;
    qint64 amountCents = 0;
};

// CRUD et agrégations sur les dépenses. Les agrégations sont effectuées en SQL.
// Voir docs/adr/0002 et docs/adr/0004.
class ExpenseRepository
{
public:
    explicit ExpenseRepository(Database &database);

    // Ajoute une dépense ; renvoie son identifiant, ou std::nullopt en cas d'échec.
    std::optional<int> add(const Expense &expense);
    bool update(const Expense &expense);
    bool remove(int id);

    // Dépenses d'un mois donné (month : 1 à 12), triées par date puis identifiant.
    QVector<Expense> forMonth(int year, int month) const;

    // Total par catégorie (feuille) sur un mois donné.
    QVector<CategoryTotal> totalsByCategory(int year, int month) const;

    // Total par catégorie (feuille) sur une année entière.
    QVector<CategoryTotal> totalsByCategoryForYear(int year) const;

    // Totaux mensuels d'une année (index 0 = janvier ... 11 = décembre), en centimes.
    std::array<qint64, 12> monthlyTotals(int year) const;

    // Années pour lesquelles au moins une dépense existe, par ordre croissant.
    QVector<int> availableYears() const;

private:
    QVector<CategoryTotal> totalsByCategoryBetween(const QString &start,
                                                   const QString &end) const;

    Database &m_database;
};

} // namespace grossbuch
