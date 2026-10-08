# 0021 - Distribution complémentaire au format AppImage

Date : 2026-10-08

## Statut

Accepté

## Contexte

L'application est distribuée en paquet `.deb` (voir docs/adr/0005 et docs/adr/0015), format principal, bien intégré au bureau et installable sur Debian, Ubuntu, LMDE et dérivés. Ce format suppose toutefois un gestionnaire de paquets APT et des paquets Qt disponibles dans la distribution. Pour partager l'application hors de l'écosystème Debian, ou pour disposer d'un format de secours quand la résolution des dépendances du `.deb` échoue, un format portable, autonome et sans installation est souhaitable.

Plusieurs formats portables existent. Flatpak offre une bonne intégration multi-distributions mais son bac à sable complique l'accès au dossier partagé de synchronisation (voir docs/adr/0020), qui est au cœur du partage des données ; il faudrait accorder des permissions `--filesystem` et composer avec des chemins remappés. Snap est trop centré sur Ubuntu. AppImage produit un fichier unique exécutable sans installation ni privilèges, avec un accès complet au système de fichiers de l'utilisateur, ce qui n'entrave pas le modèle de dossier partagé.

## Décision

Ajouter l'AppImage comme format de distribution complémentaire, sans remplacer le `.deb` qui reste le format principal et recommandé.

Construction. Un script `packaging/build-appimage.sh` compile en Release, installe dans un `AppDir` via `cmake --install` (le même jeu de règles `install()` que le `.deb` : binaire, fichier `.desktop`, icône scalable), puis assemble l'AppImage avec `linuxdeploy` et son greffon Qt. Le pilote SQL SQLite de Qt, greffon chargé à l'exécution et indispensable pour ouvrir la base, est inclus de force via `EXTRA_QT_PLUGINS="sqldrivers"` ; sans cela l'application se lancerait mais ne pourrait pas ouvrir sa base. Qt Charts, dépendance directe du binaire, est embarqué automatiquement.

Automatisation. Le workflow de release construit l'AppImage dans un job dédié tournant sur Ubuntu 22.04, pour viser une version de glibc suffisamment ancienne et donc une portabilité large, puis la publie comme second asset de la release aux côtés du `.deb`. Les outils `linuxdeploy` sont téléchargés depuis leurs dépôts officiels ; ils tournent avec `APPIMAGE_EXTRACT_AND_RUN=1` pour ne pas dépendre de FUSE dans l'intégration continue.

Chiffrement et signature. Non retenus à ce stade, comme pour les autres formats. L'intégrité est assurée par la publication sur la page des releases du dépôt.

## Conséquences

- L'application peut être distribuée hors écosystème Debian, sous forme d'un fichier unique sans installation.
- Le `.deb` demeure le format principal, mieux intégré au bureau et doté de mises à jour par APT ; l'AppImage est un secours et un complément.
- L'AppImage est plus lourde car elle embarque Qt et ses greffons, et ne bénéficie pas de mise à jour automatique.
- L'inclusion explicite du pilote SQLite de Qt est un point de vigilance : tout oubli rendrait l'ouverture de la base impossible, d'où un contrôle manuel recommandé (lancer l'AppImage sur une machine sans Qt installé et vérifier l'ouverture de la base).
- Les utilisateurs d'AppImage peuvent avoir besoin de FUSE pour l'exécuter, ou de l'option `--appimage-extract-and-run`, conformément au fonctionnement habituel du format.
- Cette décision prolonge les ADR 0005 et 0015 (distribution `.deb`) et ne remplace aucune décision antérieure.
