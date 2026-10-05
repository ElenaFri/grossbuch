#pragma once

#include "core/Category.h"

#include <QVector>

#include <optional>

namespace grossbuch {

class Database;

// Lecture des catégories. Les catégories étant figées après le seed, ce dépôt
// n'expose que des opérations de consultation. Voir docs/adr/0006.
class CategoryRepository
{
public:
    explicit CategoryRepository(Database &database);

    // Toutes les catégories (racines et sous-catégories), triées par nom.
    QVector<Category> all() const;

    // Catégories sélectionnables pour une dépense : sous-catégories, plus les
    // catégories racines sans enfant.
    QVector<Category> selectable() const;

    std::optional<Category> byId(int id) const;

private:
    Database &m_database;
};

} // namespace grossbuch
