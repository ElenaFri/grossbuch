#pragma once

#include <QHash>
#include <QString>
#include <QVector>

class QComboBox;
class QLabel;

namespace grossbuch {

struct Category;

// Fonctions utilitaires partagées par les onglets de l'interface, pour éviter la
// duplication (formatage monétaire, peuplement du sélecteur de catégories et noms
// d'affichage hiérarchiques). Voir docs/adr/0004.

// Formate un montant en centimes sous forme de chaîne monétaire localisée.
QString formatMoney(qint64 cents);

// Indexe les catégories par identifiant pour une recherche rapide.
QHash<int, Category> categoriesById(const QVector<Category> &categories);

// Nom lisible d'une catégorie : « Parent / Enfant » pour une sous-catégorie, le
// seul nom pour une racine, une chaîne vide si l'identifiant est inconnu.
QString categoryDisplayName(const QHash<int, Category> &byId, int categoryId);

// Remplit un combo avec les catégories racines comme en-têtes non sélectionnables
// et les sous-catégories en éléments indentés sélectionnables ; une racine sans
// enfant est directement sélectionnable. Préserve la catégorie précédemment
// sélectionnée et, à défaut, sélectionne la première entrée sélectionnable.
void populateCategoryCombo(QComboBox *combo, const QVector<Category> &categories);

// Sélectionne l'entrée du combo dont la donnée vaut categoryId, si elle existe.
void selectComboCategory(QComboBox *combo, int categoryId);

// Affiche un message transitoire dans un label (vert par défaut, rouge si error),
// effacé automatiquement après quelques secondes.
void showFeedback(QLabel *label, const QString &text, bool error = false);

} // namespace grossbuch
