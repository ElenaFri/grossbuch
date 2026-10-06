# 0010 - Paiements récurrents matérialisés en dépenses

Date : 2026-10-06

## Statut

Accepté

## Contexte

Certaines dépenses reviennent chaque mois sans varier, ou en variant peu (loyer, abonnements, assurances). Les ressaisir manuellement tous les mois est fastidieux et source d'oublis. Un quatrième onglet, placé après « Saisie », doit permettre de définir ces paiements une seule fois et de les reporter automatiquement sur chaque mois, tout en restant modifiables et désactivables.

Deux grandes approches étaient possibles. La première, purement virtuelle, garde les modèles à part et projette les occurrences au moment de l'affichage et des agrégations. La seconde, par matérialisation, génère de vraies lignes de dépense pour chaque mois. L'approche virtuelle complique toutes les agrégations existantes (récapitulatif, graphiques) et rend ambiguë la notion de « montant qui varie peu » pour un mois donné.

## Décision

Un paiement récurrent est un modèle stocké dans une nouvelle table `recurring_expenses` : montant par défaut en centimes, catégorie, libellé, jour du mois (paramétrable par modèle, borné à 1–28 pour éviter les problèmes de fin de mois), année et mois de début, indicateur actif, et un repère du dernier mois déjà matérialisé.

La table `expenses` reçoit une colonne `recurring_id` (nullable, référence vers le modèle). Une occurrence générée porte l'identifiant de son modèle ; une dépense saisie manuellement a `recurring_id` à NULL. Les occurrences sont donc de vraies dépenses, comptées sans traitement particulier dans les onglets Saisie, Récapitulatif et Graphiques.

La migration du schéma passe en version 2 : création de la table `recurring_expenses` et ajout de la colonne `recurring_id` à `expenses`.

Règles de matérialisation, appliquées au lancement de l'application :

- À la création d'un modèle, les occurrences sont rattrapées depuis le mois de début indiqué jusqu'au mois en cours inclus.
- Ensuite, au fil du temps, seule l'occurrence du mois en cours est créée lorsque le mois change. Aucun mois futur n'est jamais généré : tant qu'on n'a pas changé de mois, il n'y a pas de donnée pour les mois suivants.
- La génération est idempotente et pilotée par le repère du dernier mois matérialisé, qui n'est jamais ramené en arrière. Conséquence voulue : une occurrence supprimée à la main dans l'onglet Saisie n'est jamais recréée.

Règles d'édition :

- Modifier un modèle (montant, catégorie, libellé, jour) n'affecte que les occurrences futures. Les occurrences déjà générées conservent leur montant payé. Le passé n'est jamais modifié automatiquement ; toute correction d'une occurrence passée se fait manuellement dans l'onglet Saisie.
- On ne supprime pas un modèle : on le désactive. Un modèle désactivé ne génère plus d'occurrence mais conserve tout son historique, et peut être réactivé. La réactivation reprend au mois en cours, sans rattraper la période d'inactivité.

## Conséquences

- Les agrégations et graphiques existants fonctionnent sans modification : une occurrence récurrente est une dépense comme une autre.
- La colonne `recurring_id` permet de retrouver les occurrences d'un modèle et de distinguer le généré du manuel.
- Le repère de dernier mois matérialisé garantit l'absence de doublon et la non-résurrection d'une occurrence supprimée, au prix d'un champ d'état par modèle.
- Le cœur métier gagne un `RecurringRepository` (création, modification pour l'avenir, (dés)activation, liste) et une routine de matérialisation idempotente, testables indépendamment de l'interface.
- Cette décision introduit la Phase 8 du plan (onglet Paiements récurrents) et décale le packaging en Phase 9. Elle ne remplace aucune décision antérieure.
