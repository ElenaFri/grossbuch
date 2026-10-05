# 0005 - Build CMake et paquet .deb via CPack

Date : 2026-10-05

## Statut

Accepté

## Contexte

Le projet doit être compilé de façon reproductible et distribué sous forme de paquet `.deb` installable sur le système cible (Debian 13 / LMDE 7). L'outillage choisi doit s'intégrer avec Qt 6 et permettre aussi d'exécuter les tests.

## Décision

Nous utilisons CMake (version 3.21 ou supérieure) comme système de build, et CPack avec le générateur `DEB` pour produire le paquet. CMake orchestre la détection de Qt 6, la compilation, l'exécution des tests via CTest, et la génération du paquet.

## Conséquences

- Une seule chaîne d'outils couvre build, tests et packaging.
- La cible `package` (ou `cpack -G DEB`) produit directement un `.deb`.
- Les dépendances d'exécution (bibliothèques Qt 6) devront être déclarées dans la configuration CPack pour que le paquet s'installe proprement.
- Le même `CMakeLists.txt` pourra être utilisé par une CI (GitHub Actions) pour automatiser la génération du paquet sur les tags.
