# 0008 - Récapitulatif par catégorie, au mois ou à l'année

Date : 2026-10-05

## Statut

Accepté

## Contexte

Le plan initial prévoyait un onglet Récapitulatif strictement mensuel, agrégeant les dépenses par catégorie et sous-catégorie pour le mois en cours. Or l'onglet Saisie affiche déjà la liste détaillée des dépenses du mois en cours, ce qui créait un chevauchement de portée temporelle entre les deux onglets, même si leurs vues diffèrent (détail chronologique d'un côté, agrégats de l'autre).

Un récapitulatif uniquement annuel aurait levé ce chevauchement mais aurait fait perdre la granularité mensuelle, utile au suivi d'un budget poste par poste.

## Décision

L'onglet Récapitulatif agrège les dépenses par catégorie racine et sous-catégorie, sous forme de tableau hiérarchique, avec un sélecteur de période offrant deux modes au choix : un mois donné, ou une année entière. Le mode par défaut est le mois en cours.

Le tableau affiche, par catégorie racine, un sous-total par sous-catégorie et un total de la catégorie, ainsi qu'un total général de la période. Les montants sont mis en forme en euros selon la locale.

## Conséquences

- Chaque onglet a un rôle distinct et sans recouvrement : Saisie pour le détail éditable du mois, Récapitulatif pour les agrégats par poste au mois ou à l'année, Graphiques pour les courbes annuelles sur le total toutes catégories.
- Le cœur métier expose une agrégation par catégorie sur une année entière (`totalsByCategoryForYear`) en plus de l'agrégation mensuelle existante.
- Cet ADR fait évoluer la portée décrite par le plan initial pour la phase 5 ; il ne remplace aucune décision antérieure.
