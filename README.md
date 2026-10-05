# grossbuch

Application de comptabilité privée pour Linux, écrite en C++ / Qt.

L'objectif est de saisir rapidement ses dépenses, en les classant par catégories, de consulter un récapitulatif mensuel et de visualiser l'évolution des dépenses totales d'une année sur l'autre.

On est dans une approche minimaliste et optimisée.

## Sommaire

- [Fonctionnalités cibles](#fonctionnalités-cibles)
- [Catégories](#catégories)
- [Plan de développement](#plan-de-développement)
- [Construire le projet](#construire-le-projet)
- [Générer le paquet `.deb`](#générer-le-paquet-deb)

Les choix techniques, l'architecture et le modèle de données sont documentés sous forme d'ADR (Architecture Decision Records) dans [docs/adr/](docs/adr/).

## Fonctionnalités cibles

L'application s'organise autour de trois onglets :

1. Accueil / Saisie : un formulaire pour enregistrer la prochaine dépense (montant, date, libellé optionnel) avec sélection de la (sous-)catégorie dans une liste déroulante.
2. Récapitulatif mensuel : un tableau du mois en cours, agrégé par catégorie (avec le détail par sous-catégorie), et un total général.
3. Graphiques annuels : courbes des dépenses totales (toutes catégories confondues) de toutes les années enregistrées, superposées, pour comparer les mois d'une année à l'autre.

---

## Catégories

Hiérarchie initiale (pré-remplie au premier lancement) :

- Alimentation → Courses · Restaurants
- Vêtements → Adultes · Enfants
- Déplacements → Transports en commun · Vélo · Voiture · Train
- Éducation → École · Conservatoire · Danse · Centres aérés · Informatique · Arts
- Livres et jeux → Adultes · Enfants
- Santé → Adultes · Enfants
- Maison → Strasbourg · Niederhaslach
- Sorties → Musées · Sport · Spectacles · Babysitter
- Cadeaux et dons
- Voyages

> Le récapitulatif mensuel agrège par catégorie racine (avec détail par sous-catégorie). Les graphiques annuels n'utilisent que le total général.

## Plan de développement

### Phase 0 — Mise en place du projet
- [x] Initialiser l'arborescence (`src/`, `tests/`, `cmake/`, `packaging/`)
- [x] Écrire le `CMakeLists.txt` racine (C++17, détection de Qt6)
- [x] Configurer `clang-format` et `clang-tidy`
- [x] Vérifier un build « Hello Qt » minimal (fenêtre vide)
- [x] Documenter les dépendances de build (Qt6 Widgets, Sql, Charts, Test)

### Phase 1 — Cœur métier (core)
- [x] Modéliser `Category` (id, nom, parent)
- [x] Modéliser `Expense` (montant en centimes, date, libellé, catégorie)
- [x] Implémenter `Database` (ouverture SQLite, création du schéma)
- [x] Gérer la versioning / migration du schéma
- [x] Implémenter le seed des catégories au premier lancement
- [x] Implémenter `ExpenseRepository` : ajouter une dépense
- [x] `ExpenseRepository` : modifier une dépense existante
- [x] `ExpenseRepository` : supprimer une dépense
- [x] `ExpenseRepository` : lister les dépenses d'un mois donné
- [x] `ExpenseRepository` : agrégation par catégorie sur un mois donné
- [x] `ExpenseRepository` : total par mois pour une année donnée
- [x] `ExpenseRepository` : liste des années disponibles

### Phase 2 — Tests unitaires du core
- [x] Mettre en place Qt Test dans CMake (`ctest`)
- [x] Tester l'insertion et la relecture d'une dépense
- [x] Tester la modification et la suppression d'une dépense
- [x] Tester les agrégations mensuelles par catégorie
- [x] Tester le total mensuel / annuel
- [x] Tester les montants en centimes (pas d'erreur d'arrondi)

### Phase 3 — Interface : fenêtre principale
- [x] Créer `MainWindow` avec un `QTabWidget` à 3 onglets
- [x] Mettre en place l'icône, le titre et la taille par défaut
- [x] Injecter le `core` (repository) dans l'UI

### Phase 4 — Onglet Saisie
- [ ] Champ montant (validation numérique, format monétaire)
- [ ] Sélecteur de date (par défaut : aujourd'hui)
- [ ] Champ libellé optionnel
- [ ] Liste déroulante des catégories sélectionnables (sous-catégories, ou catégorie racine si elle n'a pas d'enfant), groupées par catégorie racine
- [ ] Bouton « Enregistrer » + feedback de confirmation
- [ ] Liste des dépenses du mois en cours sous le formulaire
- [ ] Modifier une dépense sélectionnée dans la liste
- [ ] Supprimer une dépense sélectionnée (avec confirmation)
- [ ] Rafraîchir les autres onglets après ajout, modification ou suppression

### Phase 5 — Onglet Récapitulatif mensuel
- [ ] Sélecteur de mois (par défaut : mois en cours)
- [ ] Tableau par catégorie racine avec sous-totaux par sous-catégorie
- [ ] Ligne de total général du mois
- [ ] Mise en forme monétaire (€, séparateurs de milliers)

### Phase 6 — Onglet Graphiques annuels
- [ ] Intégrer Qt Charts
- [ ] Une courbe par année (12 points = 12 mois), superposées
- [ ] Afficher les trois dernières années par défaut
- [ ] Sélecteur pour ajuster les années visibles
- [ ] Légende, axes (mois en abscisse, montant en ordonnée)
- [ ] Afficher uniquement le total mensuel (toutes catégories)
- [ ] Rafraîchissement automatique à l'ajout d'une dépense

### Phase 7 — Finitions & robustesse
- [ ] Gestion des erreurs (base inaccessible, saisie invalide)
- [ ] Localisation FR (format de dates/montants via `QLocale`)
- [ ] Persistance de l'état de l'UI (dernier onglet, taille fenêtre)
- [ ] (Optionnel) Export CSV du récapitulatif

### Phase 8 — Packaging & distribution
- [ ] Fichier `.desktop` + icône pour l'intégration au bureau
- [ ] Règles d'installation CMake (`install(TARGETS ...)`)
- [ ] Configurer CPack avec le générateur `DEB`
- [ ] Déclarer les dépendances runtime du `.deb` (libQt6...)
- [ ] Générer et tester l'installation du `.deb` sur le système
- [ ] (Optionnel) Workflow GitHub Actions : build + `.deb` sur les tags

## Construire le projet

> Prérequis (Debian/Ubuntu) :
> ```sh
> sudo apt install build-essential cmake \
>     qt6-base-dev qt6-charts-dev
> ```

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build        # lancer les tests
./build/grossbuch             # lancer l'application
```

## Générer le paquet `.deb`

```sh
cmake --build build --target package
# ou :
cd build && cpack -G DEB
```

Le fichier `grossbuch_<version>_amd64.deb` est produit dans `build/`.
Installation :

```sh
sudo apt install ./grossbuch_<version>_amd64.deb
```

## Licence

MIT © 2026 Elena FRISON — voir [LICENSE](LICENSE).
