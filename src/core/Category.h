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
    QString key;                 // clé textuelle stable (voir docs/adr/0011)

    bool isRoot() const { return !parentId.has_value(); }
};

} // namespace grossbuch
