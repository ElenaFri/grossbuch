# 0014 - Retrait de l'export CSV du récapitulatif

Date : 2026-10-07

## Statut

Accepté

## Contexte

L'onglet Récapitulatif proposait un bouton « Exporter en CSV… » (fonction `summaryToCsv` du cœur métier, décidée dans l'ADR 0009) produisant un rapport agrégé par période lisible dans un tableur. Depuis, l'application dispose d'un export JSON complet (instantané brut pour la fusion et la sauvegarde, ADR 0012) et de sauvegardes SQLite horodatées (ADR 0013), tous deux accessibles depuis l'onglet Données. L'export CSV faisait ainsi doublon avec un besoin déjà couvert, tout en entretenant un deuxième format à maintenir et à tester.

## Décision

- L'export CSV est retiré : suppression des fichiers `src/core/CsvExport.{h,cpp}`, du bouton et de la logique `onExport` dans `SummaryTab`, ainsi que des tests unitaires associés dans `tst_core.cpp`.
- L'onglet Récapitulatif reste purement consultatif (tableau hiérarchique par catégorie et total de la période).
- Les besoins d'extraction et de transmission des données sont assurés exclusivement par l'export JSON et les sauvegardes de l'onglet Données.

## Conséquences

- Un seul format d'échange (JSON) et un seul format de sauvegarde (SQLite) à maintenir et documenter.
- La partie « Export CSV » de l'ADR 0009 est remplacée par le présent ADR ; ses décisions sur la locale forcée et la persistance de l'UI demeurent valables.
- Si un besoin d'export tableur réapparaît, il pourra être réintroduit dans le cœur métier avec ses tests, sans impact sur le reste de l'architecture.
