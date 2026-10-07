# grossbuch

Application de comptabilité privée pour Linux, écrite en C++ / Qt.

L'objectif est de saisir rapidement ses dépenses, en les classant par catégories, de consulter un récapitulatif par catégorie au mois ou à l'année et de visualiser l'évolution des dépenses totales d'une année sur l'autre.

On est dans une approche minimaliste et optimisée.

## Sommaire

- [Fonctionnalités existantes](#fonctionnalités-existantes)
- [Catégories](#catégories)
- [Feuille de route](#feuille-de-route)
- [Construire le projet](#construire-le-projet)
- [Générer le paquet `.deb`](#générer-le-paquet-deb)

Les choix techniques, l'architecture et le modèle de données sont documentés sous forme d'ADR (Architecture Decision Records) dans [docs/adr/](docs/adr/).

## Fonctionnalités existantes

L'application s'organise autour de quatre onglets.

- Saisie : enregistrement d'une dépense (montant, date, libellé optionnel) avec sélection de la (sous-)catégorie dans une liste déroulante groupée, modification et suppression d'une dépense, et liste des dépenses du mois en cours.
- Paiements récurrents : modèles de dépenses qui reviennent chaque mois, matérialisés automatiquement sur le mois en cours, modifiables pour l'avenir seulement et désactivables ou réactivables sans perte de l'historique.
- Récapitulatif : tableau agrégé par catégorie racine avec le détail par sous-catégorie, pour un mois ou une année au choix, avec un total général et un export CSV.
- Graphiques annuels : courbes des dépenses totales mensuelles superposées d'une année sur l'autre, les trois dernières années par défaut, avec sélection des années visibles.

Socle technique : stockage local SQLite, montants stockés en centimes pour éviter les erreurs d'arrondi, interface francophone, persistance de l'état de la fenêtre, distribution en paquet `.deb` et release GitHub automatisée sur les tags.

## Catégories

Hiérarchie initiale (pré-remplie au premier lancement) :

- Alimentation → Courses · Restaurants
- Vêtements → Adultes · Enfants
- Déplacements → Transports en commun · Vélo · Voiture · Train
- Éducation → École · Loisirs · Centres aérés · Formation continue
- Livres et jeux → Adultes · Enfants
- Santé → Adultes · Enfants
- Maison → Résidence principale · Résidence secondaire
- Sorties → Musées · Sport · Spectacles · Babysitter
- Cadeaux et dons
- Voyages

> Le récapitulatif agrège par catégorie racine (avec détail par sous-catégorie), au mois ou à l'année. Les graphiques annuels n'utilisent que le total général.

## Feuille de route

Prochain objectif : rendre les données pérennes et transmissibles, de sorte que plusieurs personnes d'un même foyer puissent utiliser l'application avec les mêmes données, chacune sur son ordinateur.

L'architecture retenue est hors ligne d'abord (offline-first). Chaque machine conserve sa propre base locale ; on n'échange jamais la base vivante, mais des instantanés d'échange que l'on fusionne dans chaque base. La fusion se fait ligne par ligne par identifiant unique, selon la règle « la modification la plus récente l'emporte », et les suppressions se propagent par marqueurs de suppression logique. Cette approche fonctionne sans serveur, reste fiable même si les deux personnes saisissent chacune de leur côté, et le fichier d'échange sert aussi de sauvegarde.

### v0.2.0 — Schéma synchronisable

- [x] Ajouter au seed une clé textuelle stable par catégorie, indépendante de l'ordre d'insertion
- [x] Migration de schéma (v3) : ajouter `uuid` (unique), `created_at`, `updated_at` et `deleted` (tombstone) sur `expenses` et `recurring_expenses`
- [x] Remplir les `uuid` et les horodatages des lignes existantes lors de la migration
- [x] Activer le mode WAL et les clés étrangères, et exposer un contrôle d'intégrité
- [x] Remplacer la suppression définitive par une suppression logique (tombstone) dans les dépôts
- [x] Tests de migration sur un jeu de données réaliste (préservation des données, idempotence, non-régénération)

### v0.2.0 — Moteur d'échange et de fusion

- [ ] Définir un format de fichier d'échange portable et versionné, référençant les catégories par leur clé stable et non par identifiant local
- [ ] Export complet : dépenses et paiements récurrents, marqueurs de suppression compris, vers le fichier d'échange
- [ ] Import avec fusion ligne par ligne par `uuid`, règle « la plus récente l'emporte », propagation des suppressions
- [ ] Idempotence de l'import (réimporter le même fichier ne modifie rien)
- [ ] Journaliser les conflits (la modification écrasée est consignée)
- [ ] Tests du moteur de fusion : ajout des deux côtés, modification concurrente, suppression propagée, réimport idempotent

### v0.2.0 — Sauvegardes et pérennité

- [ ] Sauvegarde automatique horodatée par copie cohérente de la base (`VACUUM INTO`) à l'ouverture ou à la fermeture
- [ ] Rotation des sauvegardes (conserver les N plus récentes)
- [ ] Sauvegarde automatique systématique avant tout import
- [ ] Restauration d'une sauvegarde depuis l'application
- [ ] Documenter le schéma et le format d'échange pour la lisibilité à long terme

### v0.2.0 — Interface d'import / export

- [ ] Menu ou onglet Données : exporter vers un fichier, importer depuis un fichier
- [ ] Retour visuel du résultat de la fusion (ajouts, mises à jour, suppressions, conflits)
- [ ] Confirmation avant import, avec rappel que la base est sauvegardée au préalable
- [ ] Tests de l'interface (Qt Test, offscreen)

### v0.3.0 — Synchronisation distante semi-automatique

- [ ] Chemin d'un dossier partagé configurable (géré côté système par Syncthing, Nextcloud, Dropbox ou équivalent)
- [ ] Export du fichier d'échange à la fermeture et import avec fusion à l'ouverture
- [ ] Garantie de ne jamais synchroniser la base vivante, uniquement le fichier d'échange
- [ ] (Optionnel) Chiffrement du fichier d'échange pour un transit par un cloud tiers

## Construire le projet

> Prérequis (Debian/Ubuntu) : `sudo apt install build-essential cmake qt6-base-dev qt6-charts-dev libqt6sql6-sqlite`. Le paquet `libqt6sql6-sqlite` fournit le pilote SQLite de Qt, chargé à l'exécution ; sans lui l'application ne peut pas ouvrir sa base.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build        # lancer les tests
./build/grossbuch             # lancer l'application
```

## Générer le paquet `.deb`

> Prérequis supplémentaire : `dpkg-dev` (fournit `dpkg-shlibdeps`, utilisé pour déduire automatiquement les dépendances Qt du paquet).

```sh
cmake --build build --target package
# ou :
cd build && cpack -G DEB
```

Le fichier `grossbuch_<version>-1_amd64.deb` est produit dans `build/`. Installation (apt résout les dépendances runtime) :

```sh
sudo apt install ./grossbuch_<version>-1_amd64.deb
```

Une release GitHub sur un tag `vX.Y.Z` construit et publie automatiquement ce paquet (voir `.github/workflows/release.yml`).
