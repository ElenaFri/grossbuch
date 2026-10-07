#pragma once

#include <QDate>
#include <QString>
#include <QtGlobal>

namespace grossbuch {

// Une dépense isolée. Le montant est exprimé en centimes (entier) pour éviter
// toute erreur d'arrondi. Voir docs/adr/0003. categoryId référence une catégorie
// sélectionnable (sous-catégorie, ou racine sans enfant). Voir docs/adr/0006.
struct Expense
{
    int id = 0;
    qint64 amountCents = 0;
    QDate date;
    QString label;
    int categoryId = 0;

    // Identité synchronisable (voir docs/adr/0011).
    QString uuid;
    QString createdAt;
    QString updatedAt;
    bool deleted = false;
};

} // namespace grossbuch
