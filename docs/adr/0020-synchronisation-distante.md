# 0020 - Synchronisation distante semi-automatique par dossier partagé

Date : 2026-10-08

## Statut

Accepté

## Contexte

Le fichier d'échange JSON et le moteur de fusion existent (voir docs/adr/0012), ainsi qu'un export et un import manuels depuis le menu Fichier. Il reste à rendre l'échange entre machines d'un même foyer commode, pour que deux personnes puissent travailler sur les mêmes données sur deux ordinateurs sans manipulation fastidieuse. L'architecture retenue reste hors ligne d'abord : aucun serveur, aucun compte, et surtout on ne synchronise jamais la base vivante (fichier SQLite avec son journal), uniquement le fichier d'échange. Le transport est délégué à un outil tiers que l'utilisateur possède déjà (Syncthing, Nextcloud, Dropbox…) qui maintient un dossier partagé entre les machines.

Plusieurs écueils guident la décision. Deux machines écrivant dans un même fichier commun du dossier partagé produiraient des conflits de fichiers au niveau de l'outil de synchronisation (copies « en conflit »). Le dossier partagé peut être momentanément indisponible (cloud non monté, chemin non configuré) : l'ouverture et la fermeture de l'application ne doivent jamais en être bloquées. Enfin, la fusion prenant une sauvegarde préalable de la base (voir docs/adr/0013), il ne faut pas en déclencher une par fichier importé sous peine d'engorger la rotation.

## Décision

Un instantané par machine. Chaque installation possède un identifiant stable (`deviceId`, un UUID généré une seule fois et conservé dans la configuration). Elle écrit son propre instantané `grossbuch-<deviceId>.json` dans le dossier partagé. À l'import, on fusionne tous les instantanés `grossbuch-*.json` présents sauf le sien. Comme chaque machine écrit un fichier distinct, l'outil de synchronisation n'a jamais à arbitrer d'écriture concurrente sur un même fichier. La fusion existante (par `uuid`, « la plus récente l'emporte », idempotente) fait converger toutes les machines quel que soit l'ordre des échanges.

Déclenchement. La synchronisation automatique s'effectue à l'ouverture (import puis rafraîchissement des vues si la base a changé) et à la fermeture (export de l'instantané local), lorsqu'elle est activée et configurée. Une action manuelle « Synchroniser maintenant » du menu Fichier reste disponible à tout moment ; elle importe puis exporte. La synchronisation automatique est désactivée par défaut : elle ne s'active qu'une fois un dossier choisi et la case cochée dans la boîte « Configurer la synchronisation… ».

Sauvegarde unique par salve. Avant d'appliquer la salve d'imports, une seule sauvegarde complète de la base est prise, puis la rotation est appliquée. Les imports fusionnent ensuite sans reprendre de sauvegarde par fichier. S'il n'y a aucun instantané pair à fusionner, aucune sauvegarde n'est prise.

Robustesse. Si le dossier n'est pas configuré ou n'existe pas (cloud non monté), la synchronisation est ignorée silencieusement, sans bloquer l'ouverture ni la fermeture, avec un message discret en barre d'état. L'application ne crée jamais le dossier partagé : il appartient à l'outil de synchronisation. Un instantané pair corrompu ou illisible est ignoré, et la fusion des autres se poursuit.

Séparation des responsabilités. Le cœur reçoit un `SyncService` (sans dépendance à l'interface) qui localise les instantanés, orchestre la sauvegarde unique et délègue la fusion à `ExchangeService`. Il est testable de bout en bout avec deux bases sur fichier et un dossier temporaire. Côté interface, un `SyncController` persiste la configuration (dossier partagé, synchronisation automatique, `deviceId`) et expose les actions ; une boîte `SyncDialog` édite cette configuration.

Chiffrement du fichier d'échange. Reporté à une version ultérieure. Le dossier partagé transite par un outil tiers dont le chiffrement de transport et de stockage relève de la configuration de l'utilisateur ; un chiffrement applicatif de l'instantané sera réévalué plus tard.

## Conséquences

- Deux machines convergent automatiquement sans serveur ni compte, le transport étant délégué à un outil de synchronisation de fichiers existant.
- L'absence de fichier commun évite les conflits de l'outil tiers : chaque machine n'écrit que son propre instantané.
- L'ouverture et la fermeture restent robustes même sans dossier disponible ; rien n'est bloquant, et le dossier n'est jamais créé par l'application.
- Une seule sauvegarde est prise par salve d'imports, préservant la lisibilité de l'historique de sauvegardes.
- La base vivante ne quitte jamais la machine : seul l'instantané JSON transite, conformément à l'architecture hors ligne d'abord.
- Cette décision prolonge les ADR 0012 (échange et fusion) et 0013 (sauvegardes). Elle ne remplace aucune décision antérieure. Le chiffrement applicatif de l'instantané reste à décider.
