# Architecture Decision Records (ADR)

Ce dossier regroupe les décisions d'architecture de grossbuch, au format proposé par Michael Nygard. Chaque fichier documente une décision : son contexte, le choix retenu et ses conséquences.

Convention de nommage : `NNNN-titre-court.md`, numérotation incrémentale, jamais réutilisée. Une décision qui en remplace une autre ne modifie pas l'ancienne : elle crée un nouvel ADR et passe le statut de l'ancien à « Remplacé par … ».

Statuts possibles : Proposé, Accepté, Déprécié, Remplacé.

## Index

- [0001 - Langage C++17 et framework Qt 6 Widgets](0001-langage-et-framework-ui.md)
- [0002 - Persistance via SQLite et Qt SQL](0002-persistance-sqlite.md)
- [0003 - Montants stockés en centimes entiers](0003-montants-en-centimes.md)
- [0004 - Séparation du cœur métier et de l'interface](0004-separation-core-ui.md)
- [0005 - Build CMake et paquet .deb via CPack](0005-build-cmake-et-paquet-deb.md)
- [0006 - Catégories stockées en base et pré-remplies](0006-categories-en-base.md)
- [0007 - Graphiques via Qt Charts](0007-graphiques-qt-charts.md)
- [0008 - Récapitulatif par catégorie, au mois ou à l'année](0008-recapitulatif-mois-ou-annee.md)
- [0009 - Finitions : locale forcée, état de l'UI et export CSV](0009-finitions-locale-etat-ui-csv.md)
- [0010 - Paiements récurrents matérialisés en dépenses](0010-paiements-recurrents.md)
- [0011 - Schéma synchronisable : identité des lignes, clé de catégorie, suppression logique et robustesse SQLite](0011-schema-synchronisable.md)
- [0012 - Format de fichier d'échange et moteur de fusion](0012-format-echange-et-fusion.md)
- [0013 - Sauvegardes automatiques et pérennité des données](0013-sauvegardes-et-perennite.md)
- [0014 - Retrait de l'export CSV du récapitulatif](0014-retrait-export-csv.md)
- [0015 - Dépendances .deb portables et indépendantes de la machine de build](0015-paquet-deb-portable.md)
- [0016 - Navigation par barre de menus et vues empilées](0016-navigation-par-menus.md)
- [0017 - Affichage par défaut de la seule année en cours dans les graphiques](0017-graphiques-annee-en-cours-par-defaut.md)
- [0018 - Neutralisation documentée de faux positifs SonarCloud](0018-neutralisation-faux-positifs-sonar.md)
- [0019 - Exclusions de couverture pour le code intestable par nature](0019-exclusions-de-couverture.md)
