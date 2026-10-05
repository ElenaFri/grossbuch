# 0003 - Montants stockés en centimes entiers

Date : 2026-10-05

## Statut

Accepté

## Contexte

Une application de comptabilité manipule des sommes d'argent. Les types à virgule flottante (`float`, `double`) introduisent des erreurs d'arrondi qui faussent les totaux après de nombreuses additions, ce qui est inacceptable pour des montants financiers.

## Décision

Tous les montants sont stockés et manipulés en centimes, sous forme d'entiers. La conversion en valeur affichée (euros, avec séparateurs et symbole) se fait uniquement au niveau de la présentation, via `QLocale`.

Le schéma SQLite utilise donc une colonne `amount INTEGER NOT NULL` exprimée en centimes.

## Conséquences

- Les additions et agrégations sont exactes, sans dérive d'arrondi.
- La logique d'affichage et de saisie doit convertir entre euros (interface) et centimes (stockage).
- Un montant négatif reste représentable si l'on souhaite plus tard gérer des remboursements ou avoirs.
