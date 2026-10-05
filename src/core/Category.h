#pragma once

#include <QString>

#include <optional>

namespace grossbuch {

// Catégorie de dépense. La hiérarchie est à deux niveaux : une catégorie racine
// (parentId vide) et ses éventuelles sous-catégories. Voir docs/adr/0006.
struct Category
{
    int id = 0;
    QString name;
    std::optional<int> parentId; // std::nullopt = catégorie racine

    bool isRoot() const { return !parentId.has_value(); }
};

} // namespace grossbuch
