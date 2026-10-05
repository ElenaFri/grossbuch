# 0004 - Séparation du cœur métier et de l'interface

Date : 2026-10-05

## Statut

Accepté

## Contexte

Nous voulons une application maintenable et testable. Mêler la logique métier (accès aux données, agrégations) avec le code d'interface Qt Widgets rendrait les tests unitaires difficiles et coûterait cher à faire évoluer.

## Décision

Le code est séparé en deux couches. Le module `core` contient la logique métier et ne dépend pas de Qt Widgets (il peut utiliser Qt Core et Qt SQL). Le module `ui` contient l'interface Qt Widgets et consomme le `core` via une façade `ExpenseRepository`.

Arborescence cible :

```
grossbuch/
├── CMakeLists.txt            # Configuration racine + CPack
├── cmake/                    # Modules CMake, fichiers packaging
├── src/
│   ├── core/                 # Logique métier (pas de dépendance UI)
│   │   ├── Category.*        # Catégories & sous-catégories
│   │   ├── Expense.*         # Entité « dépense »
│   │   ├── Database.*        # Accès SQLite (ouverture, schéma, migrations)
│   │   └── ExpenseRepository.* # CRUD + agrégations (mois, année)
│   ├── ui/                   # Interface Qt
│   │   ├── MainWindow.*      # Fenêtre + QTabWidget
│   │   ├── EntryTab.*        # Onglet saisie
│   │   ├── SummaryTab.*      # Onglet récapitulatif mensuel
│   │   └── ChartsTab.*       # Onglet graphiques annuels
│   └── main.cpp              # Point d'entrée
├── tests/                    # Tests unitaires (Qt Test) du core
├── packaging/                # .desktop, icônes, métadonnées
└── README.md
```

## Conséquences

- Le `core` est testable unitairement avec Qt Test, sans instancier d'interface graphique.
- L'interface peut évoluer (ou être remplacée) sans toucher à la logique métier.
- Un léger surcoût de structuration est accepté en échange de cette séparation, justifié par la maintenabilité à long terme.
