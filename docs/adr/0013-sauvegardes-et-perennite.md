# 0013 - Sauvegardes automatiques et pérennité des données

Date : 2026-10-07

## Statut

Accepté

## Contexte

Les données de comptabilité d'un foyer doivent survivre aux années, aux pannes et aux fausses manœuvres. L'architecture est hors ligne d'abord (voir docs/adr/0011 et docs/adr/0012) : la base vivante est locale à chaque machine et n'est jamais partagée telle quelle. Il faut donc, sur chaque machine, un filet de sécurité local : des copies cohérentes et horodatées de la base, créées automatiquement, dont on conserve un historique borné, et que l'on puisse restaurer. Un import de fusion modifiant la base en place, il doit être précédé d'une sauvegarde systématique.

Copier le fichier `.db` à la main n'est pas fiable : avec le journal WAL actif (voir docs/adr/0011), une simple copie du fichier principal peut être incohérente, car des transactions validées peuvent résider dans le fichier `-wal`. SQLite fournit pour cela `VACUUM INTO`, qui écrit une copie compacte et transactionnellement cohérente de la base dans un nouveau fichier, sans interrompre la connexion.

## Décision

Emplacement et nommage. Les sauvegardes sont écrites dans un sous-dossier `backups/` du répertoire de données de l'application (à côté de `grossbuch.db`), soit `~/.local/share/grossbuch/backups/`. Chaque sauvegarde est nommée `grossbuch-AAAAMMJJ-HHMMSS-mmm.db`, horodatage local, lexicographiquement triable et lisible. Le préfixe de date permet de savoir si une sauvegarde du jour existe déjà.

Copie cohérente. Une sauvegarde est produite par `VACUUM INTO '<chemin>'` exécuté sur la connexion vivante. On obtient un fichier SQLite autonome, compact et cohérent, même lorsque le journal WAL est actif. Le chemin est inséré dans l'instruction avec échappement des apostrophes, `VACUUM INTO` n'acceptant pas de paramètre lié.

Politique au démarrage : une sauvegarde par jour. À l'ouverture de l'application, une sauvegarde est créée uniquement si aucune sauvegarde ne porte déjà la date du jour. On obtient ainsi un instantané quotidien sans multiplier les fichiers quand l'application est lancée plusieurs fois dans la journée, et sans dépendre d'un événement de fermeture (peu fiable en cas d'arrêt brutal).

Rotation. On ne conserve que les N sauvegardes les plus récentes (N = 30 par défaut, soit environ un mois d'historique quotidien) ; les plus anciennes sont supprimées après chaque création. La valeur est une constante du service, ajustable.

Sauvegarde systématique avant import. Un import de fusion modifie la base en place. Le moteur d'échange (voir docs/adr/0012) crée donc une sauvegarde complète avant de fusionner, lorsqu'un répertoire de sauvegarde lui est fourni. Si la sauvegarde échoue, l'import est abandonné : on ne modifie jamais la base sans filet. L'application fournit toujours ce répertoire ; les tests unitaires de la logique de fusion l'omettent pour s'isoler.

Restauration. Restaurer consiste à remplacer le fichier de base vivant par une sauvegarde choisie. Avant tout remplacement, l'intégrité de la sauvegarde est vérifiée (`PRAGMA integrity_check` sur une connexion temporaire) : on refuse de restaurer un fichier corrompu. Les fichiers annexes du journal (`-wal`, `-shm`) de la base vivante sont supprimés lors du remplacement, afin qu'un reliquat de journal ne masque pas le contenu restauré. La restauration exige que la connexion vivante soit fermée ; elle est donc exposée comme une opération de fichiers autonome, et l'application la déclenche en invitant à redémarrer.

Documentation de pérennité. Le schéma SQLite et le format d'échange JSON sont documentés dans `docs/formats.md`, afin que les données restent lisibles et exploitables indépendamment de l'application, à très long terme.

## Conséquences

- Chaque machine conserve un historique quotidien borné de copies cohérentes de sa base, créées sans intervention.
- Aucun import ne modifie la base sans sauvegarde préalable ; un échec de sauvegarde bloque l'import.
- La restauration est sûre (intégrité vérifiée, journal annexe nettoyé) mais impose un redémarrage, la connexion devant être fermée.
- Le cœur gagne un service de sauvegarde (`BackupService`) testable indépendamment de l'interface.
- Les sauvegardes partagent le format SQLite de la base ; combinées au fichier d'échange JSON (docs/adr/0012) et à la documentation des formats, elles assurent la pérennité et la lisibilité des données.
- Cette décision prolonge les ADR 0011 et 0012. Elle ne remplace aucune décision antérieure.
