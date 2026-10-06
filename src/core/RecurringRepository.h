#pragma once

#include "core/RecurringExpense.h"

#include <QDate>
#include <QVector>

#include <optional>

namespace grossbuch {

class Database;

// Gestion des modèles de paiements récurrents (création, modification pour
// l'avenir, désactivation/réactivation, consultation). La matérialisation des
// occurrences en dépenses réelles est traitée séparément. Voir docs/adr/0010.
class RecurringRepository
{
public:
    explicit RecurringRepository(Database &database);

    // Crée un paiement récurrent et renvoie son identifiant, ou std::nullopt en
    // cas d'échec. Le repère de dernier mois matérialisé est positionné au mois
    // précédant le mois de début, afin que la prochaine matérialisation rattrape
    // l'historique depuis le mois de début.
    std::optional<int> add(const RecurringExpense &recurring);

    // Modifie le montant, le libellé, la catégorie et le jour du mois. N'affecte
    // que les occurrences futures : les occurrences déjà générées et le repère ne
    // sont jamais touchés. Renvoie faux si l'identifiant n'existe pas.
    bool update(const RecurringExpense &recurring);

    // Désactive le paiement : plus aucune occurrence n'est générée, l'historique
    // est conservé. Renvoie faux si l'identifiant n'existe pas.
    bool deactivate(int id);

    // Réactive le paiement. Le repère est avancé au mois précédant asOf de sorte
    // que la matérialisation reprenne au mois courant, sans rattraper la pause.
    bool reactivate(int id, const QDate &asOf);

    // Tous les paiements récurrents, triés par libellé puis identifiant.
    QVector<RecurringExpense> all() const;

    std::optional<RecurringExpense> byId(int id) const;

    // Matérialise les occurrences dues pour tous les paiements actifs : crée une
    // vraie dépense (liée au modèle via recurring_id) pour chaque mois allant du
    // mois suivant le repère jusqu'au mois de asOf inclus, puis avance le repère.
    // Aucun mois futur n'est généré ; l'opération est idempotente et ne recrée
    // jamais une occurrence supprimée (le repère n'est jamais ramené en arrière).
    // Renvoie le nombre d'occurrences créées (0 si rien n'est dû ou en cas
    // d'échec, auquel cas la transaction est annulée). Voir docs/adr/0010.
    int materializeDueOccurrences(const QDate &asOf);

private:
    Database &m_database;
};

} // namespace grossbuch
