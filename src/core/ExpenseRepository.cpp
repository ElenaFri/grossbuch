#include "core/ExpenseRepository.h"

#include "core/Database.h"

#include <QDate>
#include <QSqlQuery>
#include <QVariant>

namespace grossbuch {

namespace {

// Bornes [début, fin[ d'un mois, au format ISO (yyyy-MM-dd), adaptées à un
// filtrage par intervalle qui tire parti de l'index sur la colonne date.
std::pair<QString, QString> monthBounds(int year, int month)
{
    const QDate start(year, month, 1);
    const QDate end = start.addMonths(1);
    return {start.toString(Qt::ISODate), end.toString(Qt::ISODate)};
}

std::pair<QString, QString> yearBounds(int year)
{
    const QDate start(year, 1, 1);
    const QDate end(year + 1, 1, 1);
    return {start.toString(Qt::ISODate), end.toString(Qt::ISODate)};
}

Expense expenseFromQuery(const QSqlQuery &query)
{
    Expense expense;
    expense.id = query.value(0).toInt();
    expense.amountCents = query.value(1).toLongLong();
    expense.date = QDate::fromString(query.value(2).toString(), Qt::ISODate);
    expense.label = query.value(3).toString();
    expense.categoryId = query.value(4).toInt();
    return expense;
}

} // namespace

ExpenseRepository::ExpenseRepository(Database &database) : m_database(database)
{
}

std::optional<int> ExpenseRepository::add(const Expense &expense)
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "INSERT INTO expenses(amount, date, label, category_id) VALUES(?, ?, ?, ?)"));
    query.addBindValue(expense.amountCents);
    query.addBindValue(expense.date.toString(Qt::ISODate));
    query.addBindValue(expense.label.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                               : QVariant(expense.label));
    query.addBindValue(expense.categoryId);
    if (!query.exec())
        return std::nullopt;
    return query.lastInsertId().toInt();
}

bool ExpenseRepository::update(const Expense &expense)
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "UPDATE expenses SET amount = ?, date = ?, label = ?, category_id = ? WHERE id = ?"));
    query.addBindValue(expense.amountCents);
    query.addBindValue(expense.date.toString(Qt::ISODate));
    query.addBindValue(expense.label.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                               : QVariant(expense.label));
    query.addBindValue(expense.categoryId);
    query.addBindValue(expense.id);
    return query.exec() && query.numRowsAffected() > 0;
}

bool ExpenseRepository::remove(int id)
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("DELETE FROM expenses WHERE id = ?"));
    query.addBindValue(id);
    return query.exec() && query.numRowsAffected() > 0;
}

QVector<Expense> ExpenseRepository::forMonth(int year, int month) const
{
    QVector<Expense> expenses;
    const auto [start, end] = monthBounds(year, month);
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "SELECT id, amount, date, label, category_id FROM expenses "
        "WHERE date >= ? AND date < ? ORDER BY date, id"));
    query.addBindValue(start);
    query.addBindValue(end);
    if (!query.exec())
        return expenses;
    while (query.next())
        expenses.append(expenseFromQuery(query));
    return expenses;
}

QVector<CategoryTotal> ExpenseRepository::totalsByCategory(int year, int month) const
{
    QVector<CategoryTotal> totals;
    const auto [start, end] = monthBounds(year, month);
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "SELECT category_id, SUM(amount) FROM expenses "
        "WHERE date >= ? AND date < ? GROUP BY category_id ORDER BY category_id"));
    query.addBindValue(start);
    query.addBindValue(end);
    if (!query.exec())
        return totals;
    while (query.next())
        totals.append(CategoryTotal{query.value(0).toInt(), query.value(1).toLongLong()});
    return totals;
}

std::array<qint64, 12> ExpenseRepository::monthlyTotals(int year) const
{
    std::array<qint64, 12> totals{};
    const auto [start, end] = yearBounds(year);
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "SELECT CAST(substr(date, 6, 2) AS INTEGER) AS month, SUM(amount) FROM expenses "
        "WHERE date >= ? AND date < ? GROUP BY month"));
    query.addBindValue(start);
    query.addBindValue(end);
    if (!query.exec())
        return totals;
    while (query.next()) {
        const int month = query.value(0).toInt();
        if (month >= 1 && month <= 12)
            totals[static_cast<std::size_t>(month - 1)] = query.value(1).toLongLong();
    }
    return totals;
}

QVector<int> ExpenseRepository::availableYears() const
{
    QVector<int> years;
    QSqlQuery query(m_database.connection());
    if (!query.exec(QStringLiteral(
            "SELECT DISTINCT CAST(substr(date, 1, 4) AS INTEGER) AS year "
            "FROM expenses ORDER BY year")))
        return years;
    while (query.next())
        years.append(query.value(0).toInt());
    return years;
}

} // namespace grossbuch
