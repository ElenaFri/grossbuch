# 0007 - Graphiques via Qt Charts

Date : 2026-10-05

## Statut

Accepté

## Contexte

L'onglet de visualisation doit afficher l'évolution des dépenses totales mois par mois, avec une courbe par année, toutes les années superposées pour faciliter la comparaison. Il faut une bibliothèque de graphiques intégrée à la pile Qt retenue.

## Décision

Nous utilisons le module Qt Charts pour tracer les courbes. Chaque année affichée donne une série de douze points (un par mois, total toutes catégories confondues), et les séries sont superposées sur un même graphique, avec une légende et des axes (mois en abscisse, montant en ordonnée).

Pour éviter d'encombrer le graphique, seules les trois dernières années sont affichées par défaut. Un sélecteur permet d'ajuster les années visibles.

## Conséquences

- Cohérence avec le reste de la pile Qt, pas de dépendance graphique tierce.
- Qt Charts est un module séparé : le paquet de développement correspondant (`qt6-charts-dev`) doit être installé pour compiler, et la bibliothèque runtime déclarée comme dépendance du paquet `.deb`.
- Les graphiques n'utilisent que le total mensuel toutes catégories confondues, conformément au besoin ; le détail par catégorie reste réservé au récapitulatif mensuel.
- L'affichage par défaut des trois dernières années garde le graphique lisible ; le sélecteur laisse la possibilité de comparer davantage d'années ponctuellement.
