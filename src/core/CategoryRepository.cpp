#include "core/CategoryRepository.h"

#include "core/Database.h"

#include <QSqlQuery>
#include <QVariant>

namespace grossbuch {

namespace {

Category categoryFromQuery(const QSqlQuery &query)
{
    Category category;
    category.id = query.value(0).toInt();
    category.name = query.value(1).toString();
    const QVariant parent = query.value(2);
    if (!parent.isNull())
        category.parentId = parent.toInt();
    category.key = query.value(3).toString();
    return category;
}

} // namespace

CategoryRepository::CategoryRepository(Database &database) : m_database(database)
{
}

QVector<Category> CategoryRepository::all() const
{
    QVector<Category> categories;
    QSqlQuery query(m_database.connection());
    if (!query.exec(QStringLiteral("SELECT id, name, parent_id, key FROM categories ORDER BY name")))
        return categories;
    while (query.next())
        categories.append(categoryFromQuery(query));
    return categories;
}

QVector<Category> CategoryRepository::selectable() const
{
    QVector<Category> categories;
    QSqlQuery query(m_database.connection());
    // Une catégorie est sélectionnable si aucune autre catégorie ne la prend
    // pour parent (donc : sous-catégories et racines sans enfant).
    const QString sql = QStringLiteral(
        "SELECT id, name, parent_id, key FROM categories c "
        "WHERE NOT EXISTS (SELECT 1 FROM categories ch WHERE ch.parent_id = c.id) "
        "ORDER BY name");
    if (!query.exec(sql))
        return categories;
    while (query.next())
        categories.append(categoryFromQuery(query));
    return categories;
}

std::optional<Category> CategoryRepository::byId(int id) const
{
    QSqlQuery query(m_database.connection());
    query.prepare(QStringLiteral("SELECT id, name, parent_id, key FROM categories WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec() || !query.next())
        return std::nullopt;
    return categoryFromQuery(query);
}

} // namespace grossbuch
