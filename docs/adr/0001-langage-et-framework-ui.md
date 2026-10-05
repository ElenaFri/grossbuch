# 0001 - Langage C++17 et framework Qt 6 Widgets

Date : 2026-10-05

## Statut

Accepté

## Contexte

grossbuch est une application de bureau destinée à Linux (notamment Debian 13 / LMDE 7), avec une interface à onglets et des graphiques. Il faut un langage et un framework d'interface adaptés au desktop natif, performants et bien supportés sur la distribution cible.

Sur le système cible, `qt6-base-dev` est disponible en version 6.8.2 (une version LTS), ce qui garantit un support durable.

## Décision

Nous utilisons C++17 comme standard de langage (migration possible vers C++20) et Qt 6 Widgets comme framework d'interface.

## Conséquences

- Interface native, réactive et intégrée au bureau Linux.
- Accès aux modules Qt complémentaires (Qt SQL, Qt Charts, Qt Test) sous une même pile cohérente.
- Dépendance à Qt 6 : les machines de build et d'exécution doivent fournir les bibliothèques Qt 6 correspondantes.
- C++17 apporte RAII, `std::optional`, `std::filesystem` et une syntaxe moderne, sans exiger de compilateur trop récent.
