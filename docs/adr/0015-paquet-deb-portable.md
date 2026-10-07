# 0015 - Dépendances .deb portables et indépendantes de la machine de build

Date : 2026-10-07

## Statut

Accepté. Complète l'ADR 0005 (build CMake et paquet .deb) sur la stratégie de dépendances d'exécution.

## Contexte

Le paquet `.deb` de la release v0.2, construit en intégration continue sur `ubuntu-latest`, était refusé à l'installation sur Debian 13 / LMDE 7 avec des dépendances non satisfiables (`libqt6gui6t64`, `libqt6sql6t64`, `libqt6widgets6t64`).

La cause est la transition « 64-bit time_t ». Les paquets dont l'ABI exposait `time_t` ont été renommés avec le suffixe `t64`, mais de façon divergente selon la distribution : Debian 13 n'a renommé que `libqt6core6` (en `libqt6core6t64`) et conserve `libqt6gui6`, `libqt6sql6`, `libqt6widgets6` sans suffixe, alors qu'Ubuntu a renommé en plus `gui`, `sql` et `widgets`. Or la configuration initiale s'appuyait sur `dpkg-shlibdeps` (`CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON`), qui déduit les dépendances d'après les paquets Qt installés sur la machine de build et fige donc des noms propres à cette distribution.

## Décision

- Désactiver la déduction automatique (`CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF`).
- Déclarer manuellement les dépendances d'exécution dans `CPACK_DEBIAN_PACKAGE_DEPENDS`, en utilisant des alternatives « variante t64 | variante sans t64 » pour chaque bibliothèque Qt concernée (`core`, `gui`, `widgets`, `sql`, `charts`), avec un plancher de version prudent (`>= 6.2.0`) couvrant les Qt 6 réellement visés.
- Conserver `libqt6sql6-sqlite` en dépendance ferme (plugin chargé à l'exécution, sans variante t64) ainsi que `libc6`, `libstdc++6` et `libgcc-s1` sans contrainte de version.

## Conséquences

- Le même `.deb` s'installe sur Debian 12 et 13, LMDE, Ubuntu 22.04 et 24.04 et leurs dérivés, quelle que soit la machine de compilation.
- Le paquet produit ne dépend plus de la distribution de la machine de build : la CI sur Ubuntu génère désormais un paquet installable sur Debian, sans conteneur dédié.
- Les dépendances étant figées à la main, une évolution majeure des paquets Qt (nouveau nommage, hausse du plancher de version requis par le code) devra être répercutée explicitement ici.
- Le pilote SQLite restant une dépendance ferme, l'absence du plugin est détectée dès l'installation plutôt qu'à l'ouverture de la base.
