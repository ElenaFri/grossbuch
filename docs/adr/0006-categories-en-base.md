# 0006 - Catégories stockées en base et pré-remplies

Date : 2026-10-05

## Statut

Accepté

## Contexte

L'application repose sur une hiérarchie de catégories à deux niveaux (catégories racines et sous-catégories). Coder ces catégories en dur dans le code obligerait à recompiler pour toute modification, et compliquerait l'association des dépenses à leurs catégories.

## Décision

Les catégories sont stockées en base, dans une table dédiée, avec une hiérarchie à deux niveaux exprimée par une colonne `parent_id`. La hiérarchie initiale est insérée automatiquement (seed) lors du premier lancement, si la table est vide.

Une dépense se rattache toujours à une catégorie « sélectionnable » : la sous-catégorie quand la catégorie en possède, ou la catégorie racine elle-même quand elle n'a pas d'enfant (par exemple Cadeaux et dons, Voyages). La colonne `expenses.category_id` peut donc référencer soit une sous-catégorie, soit une racine sans enfant. Le menu de saisie ne propose que ces éléments sélectionnables, sans créer de sous-catégorie implicite artificielle.

Schéma concerné :

```sql
-- Catégories et sous-catégories (hiérarchie à 2 niveaux via parent_id)
CREATE TABLE categories (
    id        INTEGER PRIMARY KEY,
    name      TEXT    NOT NULL,
    parent_id INTEGER REFERENCES categories(id) -- NULL = catégorie racine
);

-- Dépenses
CREATE TABLE expenses (
    id          INTEGER PRIMARY KEY,
    amount      INTEGER NOT NULL,   -- en centimes (voir ADR 0003)
    date        TEXT    NOT NULL,   -- ISO-8601 (YYYY-MM-DD)
    label       TEXT,               -- libellé optionnel
    category_id INTEGER NOT NULL REFERENCES categories(id)
);

CREATE INDEX idx_expenses_date     ON expenses(date);
CREATE INDEX idx_expenses_category ON expenses(category_id);
```

## Conséquences

- Les catégories peuvent évoluer sans recompilation (même si elles restent figées pour l'instant, voir le plan).
- Une dépense pointe toujours vers une feuille sélectionnable (sous-catégorie ou racine sans enfant), ce qui garde les agrégations simples et sans ambiguïté.
- Le récapitulatif mensuel agrège par catégorie racine (avec le détail par sous-catégorie) ; les graphiques annuels n'utilisent que le total général.
- Une table de versions de schéma permettra des migrations futures (ajout de colonnes, nouvelles catégories par défaut).
- La liste initiale des catégories est documentée dans le README.
