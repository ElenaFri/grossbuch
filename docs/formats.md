# Formats de données de grossbuch

Ce document décrit les formats dans lesquels grossbuch conserve et échange ses données, afin qu'elles restent lisibles et exploitables à long terme, indépendamment de l'application. Il complète les ADR 0011, 0012 et 0013.

## Base de données SQLite (schéma version 3)

Les données vivantes sont stockées dans une base SQLite unique, par défaut `~/.local/share/grossbuch/grossbuch.db`. Le journal est en mode WAL. La version du schéma est inscrite dans `PRAGMA user_version` (valeur 3).

Les montants sont des entiers en centimes (voir docs/adr/0003). Les dates sont du texte ISO `AAAA-MM-JJ`. Les horodatages de synchronisation sont du texte ISO 8601 en temps universel à la seconde (par exemple `2026-10-07T14:30:00Z`). La suppression est logique : une ligne supprimée porte `deleted = 1` et n'est jamais effacée physiquement (tombstone), afin que la suppression se propage à la fusion.

### Table categories

Les catégories sont figées, pré-remplies au premier lancement et jamais modifiées par l'utilisateur.

| Colonne | Type | Rôle |
| --- | --- | --- |
| id | INTEGER PK | Identifiant local (peut différer d'une machine à l'autre) |
| name | TEXT | Libellé affiché |
| parent_id | INTEGER | Catégorie racine d'une sous-catégorie, ou nul pour une racine |
| key | TEXT | Clé stable, identique sur toutes les machines, index unique |

La clé est le pivot d'échange entre machines : une racine a pour clé son slug (`alimentation`, `maison`, `voyages`…), une sous-catégorie a pour clé `racine.slug` (`alimentation.courses`, `deplacements.transports-en-commun`…). Une dépense se rattache soit à une sous-catégorie, soit à une racine sans enfant.

### Table expenses

| Colonne | Type | Rôle |
| --- | --- | --- |
| id | INTEGER PK | Identifiant local |
| amount | INTEGER | Montant en centimes |
| date | TEXT | Date de la dépense (ISO `AAAA-MM-JJ`) |
| label | TEXT | Libellé libre, ou nul |
| category_id | INTEGER | Référence `categories.id` |
| recurring_id | INTEGER | Modèle récurrent d'origine, ou nul (saisie manuelle) |
| uuid | TEXT | Identité stable, index unique |
| created_at | TEXT | Horodatage de création (ISO UTC) |
| updated_at | TEXT | Horodatage de dernière modification (ISO UTC) |
| deleted | INTEGER | 0 ou 1 (tombstone) |

### Table recurring_expenses

Modèles de paiements récurrents (voir docs/adr/0010). Une occurrence matérialisée est une ligne de `expenses` reliée par `recurring_id`.

| Colonne | Type | Rôle |
| --- | --- | --- |
| id | INTEGER PK | Identifiant local |
| amount | INTEGER | Montant en centimes |
| label | TEXT | Libellé libre, ou nul |
| category_id | INTEGER | Référence `categories.id` |
| day_of_month | INTEGER | Jour d'échéance, borné à 1..28 |
| start_year, start_month | INTEGER | Premier mois concerné |
| active | INTEGER | 1 si le modèle génère encore des occurrences |
| last_year, last_month | INTEGER | Dernier mois matérialisé (0/0 = aucun) |
| uuid | TEXT | Identité stable, index unique |
| created_at, updated_at | TEXT | Horodatages (ISO UTC) |
| deleted | INTEGER | 0 ou 1 (un modèle se désactive plutôt qu'il ne se supprime) |

## Fichier d'échange JSON

Le fichier d'échange (voir docs/adr/0012) est un instantané portable servant aussi de sauvegarde lisible. C'est un objet JSON.

```json
{
  "format": "grossbuch-exchange",
  "formatVersion": 1,
  "exportedAt": "2026-10-07T14:30:00Z",
  "expenses": [
    {
      "uuid": "…",
      "amount": 1599,
      "date": "2025-03-15",
      "label": "Marché",
      "category": "alimentation.courses",
      "recurring": null,
      "createdAt": "2026-10-07T14:00:00Z",
      "updatedAt": "2026-10-07T14:00:00Z",
      "deleted": false
    }
  ],
  "recurring": [
    {
      "uuid": "…",
      "amount": 5000,
      "label": "Abonnement",
      "category": "deplacements.train",
      "dayOfMonth": 5,
      "startYear": 2025,
      "startMonth": 1,
      "active": true,
      "lastYear": 2026,
      "lastMonth": 9,
      "createdAt": "2026-10-07T14:00:00Z",
      "updatedAt": "2026-10-07T14:00:00Z",
      "deleted": false
    }
  ]
}
```

Points essentiels pour la relecture à long terme :

- Les catégories sont référencées par leur clé stable (`category`), jamais par un identifiant entier local.
- Le champ `recurring` d'une dépense porte l'`uuid` du modèle d'origine (ou `null`), et non un identifiant local.
- Les montants sont des entiers en centimes.
- Les tombstones (`deleted: true`) sont inclus : c'est ainsi que les suppressions se propagent à la fusion.
- La fusion applique la règle « la modification la plus récente l'emporte » par `uuid`, en comparant `updatedAt`. Réimporter le même fichier ne modifie rien (idempotence).

## Sauvegardes

Les sauvegardes (voir docs/adr/0013) sont des fichiers SQLite autonomes au même schéma que la base vivante, produits par `VACUUM INTO`, nommés `grossbuch-AAAAMMJJ-HHMMSS-mmm.db` et rangés dans `~/.local/share/grossbuch/backups/`. Elles s'ouvrent avec n'importe quel outil SQLite et se restaurent en remplaçant le fichier de base vivant.
