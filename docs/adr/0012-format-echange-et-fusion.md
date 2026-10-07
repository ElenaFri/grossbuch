# 0012 - Format de fichier d'échange et moteur de fusion

Date : 2026-10-07

## Statut

Accepté

## Contexte

Le schéma est désormais synchronisable (voir docs/adr/0011) : chaque ligne porte un `uuid`, des horodatages et un marqueur de suppression logique, et les catégories ont une clé stable. Il reste à définir comment deux machines d'un même foyer échangent leurs données. L'architecture est hors ligne d'abord : on n'échange jamais la base vivante, mais un instantané que l'on fusionne dans la base locale de chaque machine. Il faut un format de fichier portable et versionné, un export complet, un import qui fusionne sans perte ni doublon, le tout idempotent et traçable.

Un point mérite une attention particulière : les occurrences de paiements récurrents. Un modèle récurrent est partagé (même `uuid` sur les deux machines), et chaque machine matérialise ses propres occurrences au lancement. Si ces occurrences recevaient un `uuid` aléatoire, la même échéance mensuelle (par exemple le loyer de janvier) existerait en deux exemplaires après fusion, ce qui fausserait les totaux.

## Décision

Format d'échange. Un fichier JSON, portable, lisible et versionné. Objet racine : `format` (identifiant `grossbuch-exchange`), `formatVersion` (entier, 1 aujourd'hui), `exportedAt` (horodatage), puis deux tableaux `expenses` et `recurring`. Chaque dépense porte `uuid`, `amount` (centimes entiers), `date`, `label` (ou nul), `category` (clé stable de catégorie, jamais l'identifiant entier local), `recurring` (uuid du modèle associé, ou nul), `createdAt`, `updatedAt`, `deleted`. Chaque modèle récurrent porte `uuid`, `amount`, `label`, `category`, `dayOfMonth`, `startYear`, `startMonth`, `active`, `lastYear`, `lastMonth`, `createdAt`, `updatedAt`, `deleted`. Le choix du JSON évite toute dépendance nouvelle (QJsonDocument est fourni par Qt) et rend le fichier lisible à long terme ; il sert aussi de sauvegarde.

Référence des catégories par clé. Les catégories sont figées et semées identiquement partout, mais leur identifiant entier local peut différer d'une base à l'autre. Le fichier d'échange les référence donc par leur clé textuelle stable. À l'import, la clé est résolue vers l'identifiant local.

Occurrences récurrentes à identité déterministe. Une occurrence matérialisée reçoit un `uuid` déterministe, dérivé de l'uuid de son modèle et du couple année-mois (UUID version 5, espace de noms fixe). Deux machines partageant le même modèle produisent ainsi le même `uuid` pour la même échéance mensuelle : la fusion les reconnaît comme une seule et même ligne, sans doublon. La matérialisation insère avec `INSERT OR IGNORE` sur l'`uuid`, de sorte qu'une occurrence déjà présente (générée localement, reçue par fusion, ou supprimée et conservée en tombstone) n'est jamais recréée. Les dépenses saisies à la main gardent un `uuid` aléatoire : elles sont intrinsèquement distinctes.

Le marqueur de matérialisation (`lastYear`, `lastMonth`) voyage avec la ligne du modèle. À la première réception d'un modèle, la machine réceptrice hérite du marqueur de l'émetteur et ne régénère donc pas l'historique déjà reçu. L'avancée du marqueur ne rafraîchit pas `updatedAt` : elle ne doit jamais faire gagner le modèle lors d'une fusion face à une véritable modification. Lorsqu'un modèle est écrasé par fusion, son marqueur local est porté au plus avancé des deux (jamais ramené en arrière).

Moteur de fusion. L'import se fait dans une transaction unique. Les modèles récurrents sont fusionnés avant les dépenses, afin que la référence `recurring` d'une dépense se résolve vers un modèle local existant. La fusion se fait ligne par ligne par `uuid`, selon la règle « la modification la plus récente l'emporte » : si l'`uuid` est absent localement, la ligne est insérée ; s'il est présent et que l'horodatage entrant est strictement plus récent, la ligne locale est écrasée ; sinon rien n'est modifié. Les suppressions se propagent naturellement, le tombstone `deleted` étant un champ comme un autre soumis à la même règle.

Idempotence. Parce qu'on n'écrase que si l'horodatage entrant est strictement plus récent, réimporter le même fichier ne modifie rien : les horodatages sont égaux, tout est classé « inchangé ».

Journal des conflits. Faute d'historique de synchronisation (pas d'horloges vectorielles), on ne peut pas distinguer formellement une modification concurrente d'une mise à jour séquentielle. On adopte donc une règle pragmatique et honnête : chaque fois qu'un écrasement remplace un contenu local qui différait réellement du contenu entrant, l'ancienne valeur locale est consignée dans un rapport de fusion. L'utilisateur dispose ainsi de la trace de ce qui a été remplacé. Le rapport récapitule aussi les nombres d'ajouts, de mises à jour, de suppressions propagées et de lignes inchangées.

## Conséquences

- Le fichier d'échange est portable entre machines, lisible, versionné, et constitue une sauvegarde complète.
- Les occurrences récurrentes ne se dédoublent pas après fusion, grâce à leur identité déterministe, et la matérialisation reste idempotente même en présence d'occurrences reçues par fusion.
- Les occurrences récurrentes matérialisées avant cette décision portent un `uuid` aléatoire ; elles ne posent pas de problème tant que le marqueur voyage avec le modèle, car la machine réceptrice n'en régénère pas l'historique.
- La règle « la plus récente l'emporte » au grain de la ligne est simple et sans serveur, au prix de l'impossibilité de fusionner deux modifications d'une même ligne : l'une est conservée, l'autre consignée dans le rapport.
- Le cœur gagne un service d'échange (`ExchangeService`) testable indépendamment de l'interface, qui sérialise, désérialise et fusionne, et produit un rapport de fusion.
- Cette décision prolonge les ADR 0010 (paiements récurrents) et 0011 (schéma synchronisable). Elle ne remplace aucune décision antérieure.
