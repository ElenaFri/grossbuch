#pragma once

#include <QString>
#include <QtGlobal>

namespace grossbuch {

// Modèle de paiement récurrent : une dépense qui revient chaque mois et dont
// l'application matérialise une occurrence par mois. Le montant est en centimes
// (voir docs/adr/0003) ; categoryId référence une catégorie sélectionnable
// (voir docs/adr/0006). Le repère lastYear/lastMonth mémorise le dernier mois
// déjà matérialisé (0/0 = aucun), afin que la génération soit idempotente et ne
// recrée jamais le passé. Voir docs/adr/0010.
struct RecurringExpense
{
    int id = 0;
    qint64 amountCents = 0;
    QString label;
    int categoryId = 0;
    int dayOfMonth = 1; // borné à 1..28 (évite les problèmes de fin de mois)
    int startYear = 0;
    int startMonth = 0; // 1..12
    bool active = true;
    int lastYear = 0;  // dernier mois matérialisé (0 = aucun)
    int lastMonth = 0;
};

} // namespace grossbuch
