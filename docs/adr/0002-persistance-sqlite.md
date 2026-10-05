# 0002 - Persistance via SQLite et Qt SQL

Date : 2026-10-05

## Statut

Accepté

## Contexte

L'application doit stocker durablement des dépenses et des catégories, puis les interroger avec des agrégations (par catégorie, par mois, total par année). Les données sont locales à un seul utilisateur et ne nécessitent ni serveur ni accès concurrent distant.

## Décision

Nous utilisons SQLite comme moteur de stockage, accédé via le module Qt SQL. Le fichier de base est placé dans le répertoire standard de l'utilisateur, obtenu via `QStandardPaths::AppDataLocation` (par exemple `~/.local/share/grossbuch/`).

## Conséquences

- Aucune configuration ni service à installer : la base est un simple fichier embarqué.
- Les agrégations sont déléguées au moteur SQL (requêtes `GROUP BY`, `SUM`), plutôt que calculées en mémoire, ce qui reste performant même avec beaucoup d'années de données.
- Le format fichier facilite la sauvegarde et la portabilité des données.
- Le schéma devra gérer sa propre version pour permettre des migrations futures (voir ADR 0003 et 0006 pour les détails du schéma).
