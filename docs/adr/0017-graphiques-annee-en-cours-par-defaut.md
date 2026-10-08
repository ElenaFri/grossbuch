# 0017 - Affichage par défaut de la seule année en cours dans les graphiques

Date : 2026-10-08

## Statut

Accepté

Remplace partiellement l'ADR 0007 (uniquement la règle d'affichage par défaut des trois dernières années).

## Contexte

L'ADR 0007 prévoyait d'afficher par défaut les trois dernières années superposées dans l'onglet Graphiques, un sélecteur permettant d'ajuster les années visibles. Par ailleurs, l'application ouvre désormais la vue Graphiques au lancement (voir ADR 0016). Afficher d'emblée trois courbes superposées surcharge la vue d'accueil et nuit à la lisibilité immédiate, alors que l'intérêt premier à l'ouverture est de voir rapidement la dépense de l'année en cours.

## Décision

À l'ouverture, les graphiques n'affichent par défaut que la courbe de l'année en cours. Si aucune dépense n'existe pour l'année en cours (début d'année, ou base plus ancienne), on retombe sur l'année disponible la plus récente plutôt que sur un graphique vide. Le sélecteur d'années reste inchangé : l'utilisateur peut cocher autant d'années qu'il le souhaite pour comparer les courbes superposées, conformément à l'esprit de l'ADR 0007.

## Conséquences

- La vue d'accueil est épurée : une seule courbe, celle qui intéresse le plus l'utilisateur au quotidien.
- La comparaison pluriannuelle reste possible à la demande, sans perte de fonctionnalité, via les cases à cocher.
- La capacité de superposition décrite par l'ADR 0007 demeure ; seule sa valeur par défaut change.
