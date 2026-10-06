#include "core/RecurringRepository.h"

#include "core/Database.h"

#include <QDate>
#include <QSqlQuery>
#include <QVariant>

namespace grossbuch {

namespace {

// Mois précédant (year, month), en gérant le passage d'année.
std::pair<int, int> previousMonth(int year, int month)
{
    const int ordinal = year * 12 + (month - 1) - 1;
    return {ordinal / 12, ordinal % 12 + 1};
}

RecurringExpense recurringFromQuery(const QSqlQuery &query)
{
    RecurringExpense recurring;
    recurring.id = query.value(0).toInt();
    recurring.amountCents = query.value(1).toLongLong();
    recurring.label = query.value(2).toString();
    recurring.categoryId = query.value(3).toInt();
    recurring.dayOfMonth = query.value(4).toInt();
    recurring.startYear = query.value(5).toInt();
    recurring.startMonth = query.value(6).toInt();
    recurring.active = query.value(7).toInt() != 0;
    recurring.lastYear = query.value(8).toInt();
    recurring.lastMonth = query.value(9).toInt();
    return recurring;
}

constexpr auto kSelectColumns = "id, amount, label, category_id, day_of_month, "
                                "start_year, start_month, active, last_year, last_month";

} // namespace

RecurringRepository::RecurringRepository(Database &database) : m_database(database)
{
}

std::optional<int> RecurringRepository::add(const RecurringExpense &recurring)
{
    // Repère initial = mois précédant le début, pour que la matérialisation
    // rattrape l'historique depuis le mois de début (voir docs/adr/0010).
    const auto [lastYear, lastMonth] = previousMonth(recurring.startYear, recurring.startMonth);

    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "INSERT INTO recurring_expenses(amount, label, category_id, day_of_month, "
        "start_year, start_month, active, last_year, last_month) "
        "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(recurring.amountCents);
    query.addBindValue(recurring.label.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                                 : QVariant(recurring.label));
    query.addBindValue(recurring.categoryId);
    query.addBindValue(recurring.dayOfMonth);
    query.addBindValue(recurring.startYear);
    query.addBindValue(recurring.startMonth);
    query.addBindValue(recurring.active ? 1 : 0);
    query.addBindValue(lastYear);
    query.addBindValue(lastMonth);
    if (!query.exec())
        return std::nullopt;
    return query.lastInsertId().toInt();
}

bool RecurringRepository::update(const RecurringExpense &recurring)
{
    // Volontairement : ni start_*, ni active, ni last_* ne sont modifiés ici, pour
    // ne jamais altérer le passé ni le repère de matérialisation.
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "UPDATE recurring_expenses SET amount = ?, label = ?, category_id = ?, "
        "day_of_month = ? WHERE id = ?"));
    query.addBindValue(recurring.amountCents);
    query.addBindValue(recurring.label.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                                 : QVariant(recurring.label));
    query.addBindValue(recurring.categoryId);
    query.addBindValue(recurring.dayOfMonth);
    query.addBindValue(recurring.id);
    return query.exec() && query.numRowsAffected() > 0;
}

bool RecurringRepository::deactivate(int id)
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("UPDATE recurring_expenses SET active = 0 WHERE id = ?"));
    query.addBindValue(id);
    return query.exec() && query.numRowsAffected() > 0;
}

bool RecurringRepository::reactivate(int id, const QDate &asOf)
{
    // Avance le repère au mois précédant asOf : la matérialisation reprendra au
    // mois courant sans rattraper la période d'inactivité (voir docs/adr/0010).
    const auto [lastYear, lastMonth] = previousMonth(asOf.year(), asOf.month());

    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "UPDATE recurring_expenses SET active = 1, last_year = ?, last_month = ? WHERE id = ?"));
    query.addBindValue(lastYear);
    query.addBindValue(lastMonth);
    query.addBindValue(id);
    return query.exec() && query.numRowsAffected() > 0;
}

QVector<RecurringExpense> RecurringRepository::all() const
{
    QVector<RecurringExpense> recurrings;
    QSqlQuery query(m_database.connection());
    if (!query.exec(QStringLiteral("SELECT %1 FROM recurring_expenses ORDER BY label, id")
                        .arg(QLatin1String(kSelectColumns))))
        return recurrings;
    while (query.next())
        recurrings.append(recurringFromQuery(query));
    return recurrings;
}

std::optional<RecurringExpense> RecurringRepository::byId(int id) const
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("SELECT %1 FROM recurring_expenses WHERE id = ?")
                      .arg(QLatin1String(kSelectColumns)));
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return recurringFromQuery(query);
}

} // namespace grossbuch
