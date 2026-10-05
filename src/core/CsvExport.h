#pragma once

#include "core/Category.h"

#include <QHash>
#include <QString>
#include <QVector>
#include <QtGlobal>

namespace grossbuch {

// Génère le contenu CSV du récapitulatif à partir des catégories et des montants
// agrégés par identifiant de catégorie (en centimes). Format déterministe,
// indépendant de la locale : séparateur « ; », décimale « , », en-tête
// « Catégorie;Sous-catégorie;Montant (€) ». Seules les lignes de montant non nul
// sont émises (racines triées par nom, puis sous-catégories triées) ; une racine
// sans enfant produit « Racine;;montant ». Une ligne « Total;;grand » clôt le
// document. Voir Phase 7 du plan.
QString summaryToCsv(const QVector<Category> &categories,
                     const QHash<int, qint64> &amountsByCategory);

} // namespace grossbuch
