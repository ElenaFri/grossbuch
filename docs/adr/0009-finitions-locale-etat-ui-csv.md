# 0009 - Finitions : locale forcée, état de l'UI et export CSV

Date : 2026-10-05

## Statut

Accepté

## Contexte

La phase 7 vise la robustesse et le confort d'usage. Trois besoins se posent : garantir un affichage francophone cohérent des dates et des montants quelle que soit la locale du système (et de la machine d'intégration continue) ; mémoriser l'état de l'interface d'une session à l'autre sans perturber l'emplacement des données ; permettre d'extraire le récapitulatif vers un tableur.

Un piège connu doit être évité : définir `organizationName` modifie l'emplacement renvoyé par `QStandardPaths::AppDataLocation`, donc le chemin de la base de données.

## Décision

- Locale : l'application force `QLocale::setDefault(QLocale(QLocale::French, QLocale::France))` au démarrage. Les tests d'interface font de même, ce qui rend déterministes les assertions sur les noms de mois et le symbole monétaire, y compris en intégration continue.
- Persistance de l'UI : la géométrie de la fenêtre et l'index du dernier onglet sont stockés via `QSettings` construit explicitement avec l'organisation et l'application (`QSettings("grossbuch", "grossbuch")`). La configuration va dans `~/.config`, séparément des données, sans jamais toucher à `AppDataLocation`. La restauration a lieu dans le constructeur de `MainWindow`, la sauvegarde dans `closeEvent`.
- Export CSV : la génération du contenu est une fonction pure du cœur métier (`summaryToCsv`), donc testable sans interface. Le format est déterministe et indépendant de la locale : séparateur « ; », virgule décimale, en-tête `Catégorie;Sous-catégorie;Montant (€)`, lignes de montant non nul uniquement, ligne finale `Total;;grand`. L'interface n'ajoute que le choix du fichier et l'écriture avec BOM UTF-8 pour la compatibilité avec Excel.

## Conséquences

- Les tests ne dépendent plus de la locale de la machine hôte ; un pipeline GitHub Actions compile et exécute `ctest` à chaque poussée.
- La configuration de l'interface et les données sont dans des arborescences distinctes, ce qui évite toute régression du chemin de la base.
- La logique d'export vit dans `grossbuch_core` et est couverte par des tests unitaires ; l'ajout d'autres formats d'export se fera au même endroit.
