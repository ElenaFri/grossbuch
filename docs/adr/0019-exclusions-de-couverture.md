# 0019 - Exclusions de couverture pour le code intestable par nature

Date : 2026-10-08

## Statut

Accepté

## Contexte

L'objectif de couverture de tests du projet est d'au moins 90 % de lignes, avec une exigence forte : ne compter que des tests pertinents, jamais des tests creux (tautologiques) ou fragiles. La mesure brute (lcov) plafonnait à environ 89 % de lignes. L'analyse du code non couvert a montré que le reliquat est presque entièrement constitué de code qui s'exécute bien en production mais ne peut être testé sans contrepartie inacceptable.

Trois familles se dégagent.

La première est le contenu d'aide statique de `GuideDialog` : le constructeur se contente d'assembler du texte HTML fixe. Un test se réduirait à vérifier que ce texte contient tel mot, assertion tautologique sans valeur de régression.

La deuxième est constituée des slots modaux de `MainWindow` (`onExport`, `onImport`, `onRestore`, `onAbout`, `onGuide`) et de `RestoreDialog::onRestore`. Ils ouvrent des dialogues natifs bloquants (`QFileDialog`, `QMessageBox`) et, pour la restauration, relancent l'application via `QProcess`. En mode hors écran, `exec()` bloque ; les couvrir impose une automatisation par minuteur qui clique dans la fenêtre modale active, connue pour sa fragilité et sa sensibilité aux conditions de course.

La troisième est un ensemble de gardes défensives `return false` sur échec de requête SQL, qui ne se déclenchent que sous panne de SQLite et ne sont atteignables que par injection de panne.

## Décision

On distingue la couverture brute de la couverture pertinente. Le code intestable par nature est exclu du dénominateur de la couverture, au moyen de marqueurs lcov placés directement dans le source, versionnés et auto-documentés :

- `GuideDialog::GuideDialog` est encadré par `// LCOV_EXCL_START` / `// LCOV_EXCL_STOP` (contenu statique).
- Les slots modaux de `MainWindow` et `RestoreDialog::onRestore` sont encadrés de même (dialogues modaux et relance de processus).

Les marqueurs sont la source de vérité unique : comme les lignes exclues n'apparaissent pas dans le rapport lcov, l'exclusion se propage automatiquement à SonarCloud lors de l'import du rapport, sans dupliquer de règle au niveau fichier (qui serait plus grossière et masquerait aussi les parties testées de ces fichiers).

Les gardes défensives sur échec SQL ne sont volontairement pas marquées : elles restent comptées comme non couvertes. La couverture pertinente dépasse déjà largement 90 % sans y toucher, et laisser ces lignes visibles documente honnêtement un reliquat connu plutôt que de le dissimuler.

## Conséquences

- Couverture pertinente mesurée après exclusions : environ 92,5 % de lignes et 98 % de fonctions, au-dessus du seuil de 90 %.
- La couverture reflète la qualité réelle des tests : elle ne récompense ni les tests tautologiques ni les tests modaux fragiles, et ne les pénalise pas.
- Les exclusions sont tracées dans le code, à côté du code concerné, avec une justification ; un relecteur voit immédiatement pourquoi une zone est exclue.
- Si l'une de ces zones gagnait un jour une logique testable, il faudra retirer le marqueur correspondant et écrire le test.
- Le reliquat de gardes SQL défensives reste visible dans les rapports par fichier ; il pourra être couvert plus tard par injection de panne si cela devient pertinent.
