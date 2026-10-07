#include "core/RecurringRepository.h"

#include "core/Database.h"
#include "core/SyncMeta.h"

#include <QDate>
#include <QSqlQuery>
#include <QVariant>
#include <QtGlobal>

namespace grossbuch {

namespace {

// Mois précédant (year, month), en gérant le passage d'année.
std::pair<int, int> previousMonth(int year, int month)
{
    const int ordinal = year * 12 + (month - 1) - 1;
    return {ordinal / 12, ordinal % 12 + 1};
}

// Numéro de mois absolu (… , janv. 2025 = 24300, …) et conversion inverse,
// pour itérer simplement sur une plage de mois.
int ordinalOf(int year, int month)
{
    return year * 12 + (month - 1);
}

std::pair<int, int> fromOrdinal(int ordinal)
{
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
    recurring.uuid = query.value(10).toString();
    recurring.createdAt = query.value(11).toString();
    recurring.updatedAt = query.value(12).toString();
    return recurring;
}

constexpr auto kSelectColumns = "id, amount, label, category_id, day_of_month, "
                                "start_year, start_month, active, last_year, last_month, "
                                "uuid, created_at, updated_at";

} // namespace

RecurringRepository::RecurringRepository(Database &database) : m_database(database)
{
}

std::optional<int> RecurringRepository::add(const RecurringExpense &recurring)
{
    // Repère initial = mois précédant le début, pour que la matérialisation
    // rattrape l'historique depuis le mois de début (voir docs/adr/0010).
    const auto [lastYear, lastMonth] = previousMonth(recurring.startYear, recurring.startMonth);

    const QString now = nowTimestampUtc();
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "INSERT INTO recurring_expenses(amount, label, category_id, day_of_month, "
        "start_year, start_month, active, last_year, last_month, uuid, created_at, updated_at, "
        "deleted) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 0)"));
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
    query.addBindValue(newUuid());
    query.addBindValue(now);
    query.addBindValue(now);
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
        "day_of_month = ?, updated_at = ? WHERE id = ? AND deleted = 0"));
    query.addBindValue(recurring.amountCents);
    query.addBindValue(recurring.label.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                                 : QVariant(recurring.label));
    query.addBindValue(recurring.categoryId);
    query.addBindValue(recurring.dayOfMonth);
    query.addBindValue(nowTimestampUtc());
    query.addBindValue(recurring.id);
    return query.exec() && query.numRowsAffected() > 0;
}

bool RecurringRepository::deactivate(int id)
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral(
        "UPDATE recurring_expenses SET active = 0, updated_at = ? WHERE id = ? AND deleted = 0"));
    query.addBindValue(nowTimestampUtc());
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
        "UPDATE recurring_expenses SET active = 1, last_year = ?, last_month = ?, updated_at = ? "
        "WHERE id = ? AND deleted = 0"));
    query.addBindValue(lastYear);
    query.addBindValue(lastMonth);
    query.addBindValue(nowTimestampUtc());
    query.addBindValue(id);
    return query.exec() && query.numRowsAffected() > 0;
}

QVector<RecurringExpense> RecurringRepository::all() const
{
    QVector<RecurringExpense> recurrings;
    QSqlQuery query(m_database.connection());
    if (!query.exec(QStringLiteral("SELECT %1 FROM recurring_expenses WHERE deleted = 0 "
                                   "ORDER BY label, id")
                        .arg(QLatin1String(kSelectColumns))))
        return recurrings;
    while (query.next())
        recurrings.append(recurringFromQuery(query));
    return recurrings;
}

std::optional<RecurringExpense> RecurringRepository::byId(int id) const
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("SELECT %1 FROM recurring_expenses WHERE id = ? AND deleted = 0")
                      .arg(QLatin1String(kSelectColumns)));
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return recurringFromQuery(query);
}

int RecurringRepository::materializeDueOccurrences(const QDate &asOf)
{
    const int asOfOrdinal = ordinalOf(asOf.year(), asOf.month());
    const QVector<RecurringExpense> models = all();

    QSqlDatabase db = m_database.connection();
    if (!db.transaction())
        return 0;

    int created = 0;
    for (const RecurringExpense &model : models) {
        if (!model.active)
            continue;

        const int startOrdinal = ordinalOf(model.startYear, model.startMonth);
        // On ne génère jamais avant le début, même si le repère est incohérent.
        int repere = ordinalOf(model.lastYear, model.lastMonth);
        if (repere < startOrdinal - 1)
            repere = startOrdinal - 1;
        if (asOfOrdinal <= repere)
            continue; // rien de dû (inclut les modèles commençant dans le futur)

        for (int ordinal = repere + 1; ordinal <= asOfOrdinal; ++ordinal) {
            const auto [year, month] = fromOrdinal(ordinal);
            const int lastDay = QDate(year, month, 1).daysInMonth();
            const QDate date(year, month, qBound(1, model.dayOfMonth, lastDay));

            QSqlQuery insert(db);
            insert.prepare(QStringLiteral(
                "INSERT OR IGNORE INTO expenses(amount, date, label, category_id, recurring_id, "
                "uuid, created_at, updated_at, deleted) VALUES(?, ?, ?, ?, ?, ?, ?, ?, 0)"));
            insert.addBindValue(model.amountCents);
            insert.addBindValue(date.toString(Qt::ISODate));
            insert.addBindValue(model.label.isEmpty() ? QVariant(QMetaType(QMetaType::QString))
                                                      : QVariant(model.label));
            insert.addBindValue(model.categoryId);
            insert.addBindValue(model.id);
            const QString now = nowTimestampUtc();
            insert.addBindValue(occurrenceUuid(model.uuid, year, month));
            insert.addBindValue(now);
            insert.addBindValue(now);
            if (!insert.exec()) {
                db.rollback();
                return 0;
            }
            // OR IGNORE : une occurrence déjà présente (générée, reçue par fusion ou
            // supprimée en tombstone) n'est pas recréée. Voir docs/adr/0012.
            if (insert.numRowsAffected() > 0)
                ++created;
        }

        const auto [lastYear, lastMonth] = fromOrdinal(asOfOrdinal);
        QSqlQuery advance(db);
        advance.prepare(QStringLiteral(
            "UPDATE recurring_expenses SET last_year = ?, last_month = ? WHERE id = ?"));
        advance.addBindValue(lastYear);
        advance.addBindValue(lastMonth);
        advance.addBindValue(model.id);
        if (!advance.exec()) {
            db.rollback();
            return 0;
        }
    }

    if (!db.commit())
        return 0;
    return created;
}

} // namespace grossbuch
