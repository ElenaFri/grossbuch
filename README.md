# grossbuch

Application de comptabilité privée pour Linux, écrite en C++ / Qt.

L'objectif est de saisir rapidement ses dépenses, en les classant par catégories, de consulter un récapitulatif par catégorie au mois ou à l'année et de visualiser l'évolution des dépenses totales d'une année sur l'autre.

On est dans une approche minimaliste et optimisée.

## Sommaire

- [Fonctionnalités cibles](#fonctionnalités-cibles)
- [Catégories](#catégories)
- [Plan de développement](#plan-de-développement)
- [Construire le projet](#construire-le-projet)
- [Générer le paquet `.deb`](#générer-le-paquet-deb)

Les choix techniques, l'architecture et le modèle de données sont documentés sous forme d'ADR (Architecture Decision Records) dans [docs/adr/](docs/adr/).

## Fonctionnalités cibles

L'application s'organise autour de quatre onglets :

1. Accueil / Saisie : un formulaire pour enregistrer la prochaine dépense (montant, date, libellé optionnel) avec sélection de la (sous-)catégorie dans une liste déroulante.
2. Paiements récurrents : les dépenses qui reviennent chaque mois (loyer, abonnements, assurances), définies une fois comme modèles et reportées automatiquement sur le mois en cours, modifiables pour l'avenir et désactivables à tout moment sans perte de l'historique.
3. Récapitulatif : un tableau agrégé par catégorie (avec le détail par sous-catégorie) et un total général, pour un mois ou une année entière au choix (par défaut le mois en cours).
4. Graphiques annuels : courbes des dépenses totales (toutes catégories confondues) de toutes les années enregistrées, superposées, pour comparer les mois d'une année à l'autre.

---

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
- [x] Champ montant (validation numérique, format monétaire)
- [x] Sélecteur de date (par défaut : aujourd'hui)
- [x] Champ libellé optionnel
- [x] Liste déroulante des catégories sélectionnables (sous-catégories, ou catégorie racine si elle n'a pas d'enfant), groupées par catégorie racine
- [x] Bouton « Enregistrer » + feedback de confirmation
- [x] Liste des dépenses du mois en cours sous le formulaire
- [x] Modifier une dépense sélectionnée dans la liste
- [x] Supprimer une dépense sélectionnée (avec confirmation)
- [x] Rafraîchir les autres onglets après ajout, modification ou suppression

### Phase 5 — Onglet Récapitulatif
- [x] Sélecteur de période : un mois ou une année entière, au choix (par défaut : mois en cours)
- [x] Tableau par catégorie racine avec sous-totaux par sous-catégorie
- [x] Ligne de total général de la période
- [x] Mise en forme monétaire (€, séparateurs de milliers)

### Phase 6 — Onglet Graphiques annuels
- [x] Intégrer Qt Charts
- [x] Une courbe par année (12 points = 12 mois), superposées
- [x] Afficher les trois dernières années par défaut
- [x] Sélecteur pour ajuster les années visibles
- [x] Légende, axes (mois en abscisse, montant en ordonnée)
- [x] Afficher uniquement le total mensuel (toutes catégories)
- [x] Rafraîchissement automatique à l'ajout d'une dépense

### Phase 7 — Finitions & robustesse
- [x] Gestion des erreurs (base inaccessible, saisie invalide)
- [x] Localisation FR (format de dates/montants via `QLocale`)
- [x] Persistance de l'état de l'UI (dernier onglet, taille fenêtre)
- [x] (Optionnel) Export CSV du récapitulatif

### Phase 8 — Onglet Paiements récurrents
- [x] Migration du schéma en version 2 (table `recurring_expenses`, colonne `recurring_id` sur `expenses`)
- [x] Modéliser `RecurringExpense` (montant par défaut, catégorie, libellé, jour du mois, mois de début, actif)
- [x] `RecurringRepository` : créer, modifier (montant/catégorie/libellé/jour, pour l'avenir uniquement), lister
- [x] `RecurringRepository` : désactiver / réactiver un paiement (jamais de suppression, l'historique est conservé)
- [x] Matérialisation automatique au lancement : occurrences manquantes du mois de début (rattrapage à la création) jusqu'au mois en cours, repère idempotent, jamais de recréation d'une occurrence supprimée, aucun mois futur
- [x] Les occurrences générées sont de vraies dépenses (comptées dans Saisie, Récapitulatif et Graphiques) et ajustables à la main dans l'onglet Saisie
- [x] Tests unitaires du core (matérialisation, idempotence, rattrapage initial, non-régénération du passé, (dés)activation)
- [x] Onglet « Récurrents » inséré après « Saisie » : liste des paiements récurrents + formulaire d'ajout
- [x] Modifier un paiement récurrent (effet sur les occurrences futures uniquement, jamais rétroactif)
- [x] Désactiver / réactiver un paiement récurrent depuis l'onglet
- [x] Rafraîchir les autres onglets après création, modification ou (dés)activation
- [x] Tests de l'onglet (Qt Test, offscreen)

### Phase 9 — Packaging & distribution
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
