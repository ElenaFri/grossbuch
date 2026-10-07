# 0011 - Schéma synchronisable : identité des lignes, clé de catégorie, suppression logique et robustesse SQLite

Date : 2026-10-07

## Statut

Accepté

## Contexte

Les données doivent devenir pérennes et transmissibles, pour que plusieurs personnes d'un même foyer utilisent l'application avec les mêmes données, chacune sur son ordinateur. L'architecture retenue est hors ligne d'abord : chaque machine garde sa propre base locale, on n'échange jamais la base vivante, mais des instantanés que l'on fusionne. Un moteur de fusion ligne par ligne, « la modification la plus récente l'emporte », sera construit ensuite. Pour le rendre possible, le schéma doit d'abord porter une identité stable par ligne, des horodatages, un marqueur de suppression logique, et référencer les catégories d'une façon qui ne dépende pas des identifiants entiers locaux.

L'identifiant entier auto-incrémenté (clé primaire SQLite) ne convient pas pour la synchronisation : deux machines attribueraient le même entier à des lignes différentes. De même, l'identifiant entier d'une catégorie peut différer d'une base à l'autre selon l'ordre d'insertion, alors que les catégories sont figées et sémantiquement identiques partout.

## Décision

On fait évoluer le schéma en version 3, en conservant les identifiants entiers locaux comme clés primaires et comme références internes (aucune requête existante n'est réécrite autour d'eux).

Identité synchronisable des lignes. Les tables `expenses` et `recurring_expenses` reçoivent quatre colonnes : `uuid` (texte, unique, généré sans accolades), `created_at` et `updated_at` (horodatages ISO 8601 en temps universel), et `deleted` (entier, marqueur de suppression logique, 0 par défaut). L'`uuid` est l'identité stable d'une ligne à travers les machines ; `updated_at` arbitre les conflits de fusion ; `deleted` propage les suppressions.

Clé stable de catégorie. La table `categories` reçoit une colonne `key` : un identifiant textuel stable, indépendant de l'ordre d'insertion et des identifiants entiers. La clé suit le chemin de la catégorie sous forme de limace ASCII, la sous-catégorie étant préfixée par sa racine (par exemple `alimentation`, `alimentation.courses`, `maison.residence-principale`). Le fichier d'échange référencera les catégories par cette clé, jamais par l'entier local. Les clés sont définies explicitement dans le pré-remplissage, et non calculées, pour garantir leur stabilité dans le temps même si un nom d'affichage évolue.

Suppression logique. Supprimer une dépense ne l'efface plus physiquement : on positionne `deleted = 1` et on rafraîchit `updated_at`. Toutes les lectures et agrégations filtrent `deleted = 0`. Une dépense supprimée reste donc invisible à l'usage, mais son marqueur peut se propager lors d'une fusion. Les modèles de paiements récurrents portent aussi ces colonnes par cohérence, mais ne sont jamais supprimés logiquement par l'application : conformément à l'ADR 0010, un modèle se désactive, il ne se supprime pas. Les changements d'état d'un modèle (désactivation, réactivation, modification) rafraîchissent `updated_at` afin d'être synchronisables.

Robustesse SQLite. L'ouverture de la base active le mode journal WAL (meilleure concurrence lecture/écriture et robustesse en cas d'arrêt brutal) en plus des clés étrangères déjà activées. La base expose un contrôle d'intégrité (`PRAGMA integrity_check`) utilisable avant une sauvegarde ou un import.

Migration. La migration v3 ajoute les colonnes, remplit la clé de chaque catégorie existante par correspondance de nom et de hiérarchie, attribue un `uuid` et des horodatages à chaque ligne existante, puis crée les index d'unicité sur `uuid` et sur `key`. SQLite n'autorisant pas l'ajout direct d'une colonne avec contrainte d'unicité, l'unicité est posée par un index créé après remplissage. La migration est idempotente : elle ne se rejoue pas sur une base déjà en version 3 et ne régénère pas d'identité pour des lignes qui en ont déjà une.

## Conséquences

- Le schéma est prêt pour le moteur d'échange et de fusion, qui pourra travailler par `uuid`, arbitrer par `updated_at` et propager les suppressions par `deleted`, sans dépendre des entiers locaux.
- Les catégories sont référençables de façon portable par leur clé stable, condition nécessaire à un fichier d'échange lisible d'une machine à l'autre.
- Les suppressions deviennent réversibles côté stockage ; une ligne supprimée n'est plus détruite, ce qui augmente légèrement la taille de la base mais rend la synchronisation fiable.
- Le mode WAL améliore la résistance aux coupures ; il crée des fichiers annexes (`-wal`, `-shm`) qu'une sauvegarde par copie de fichier devra prendre en compte (traité par la future sauvegarde par `VACUUM INTO`).
- Les structures de données du cœur (`Expense`, `RecurringExpense`, `Category`) exposent désormais l'identité synchronisable, réutilisable par l'interface et le moteur de fusion.
- Cette décision ne remplace aucune décision antérieure ; elle prolonge les ADR 0002 (persistance) et 0006 (catégories) et prépare les ADR du moteur de fusion à venir.
