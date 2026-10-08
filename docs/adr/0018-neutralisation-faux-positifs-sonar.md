# 0018 - Neutralisation documentée de faux positifs SonarCloud

Date : 2026-10-08

## Statut

Accepté

## Contexte

L'analyse C++ de SonarCloud remonte deux familles d'alertes qui sont, dans ce projet, des faux positifs et font échouer la grille de qualité.

La première, cpp:S5025 (« Replace the use of new / Rewrite the code so that you no longer need this delete »), vise chaque allocation par new d'un widget. Or, dans Qt, allouer un widget par new en lui donnant un parent transfère la propriété au parent, qui détruit l'enfant automatiquement : c'est l'idiome fondamental du framework. Convertir ces allocations en pointeurs intelligents casserait la gestion de durée de vie assurée par l'arbre d'objets Qt. Ces alertes sont concentrées dans la couche interface (src/ui).

La seconde, cpp:S2259 (« Called C++ object pointer is null »), apparaît dans les tests d'interface. Le moteur d'exécution symbolique de Sonar ne modélise pas l'arrêt de fonction provoqué par la macro QVERIFY : après une vérification de non-nullité, il continue d'explorer un chemin où le pointeur serait nul et signale une déréférence impossible.

## Décision

Les faux positifs sont neutralisés de façon versionnée, via un fichier `sonar-project.properties` à la racine, à l'aide de `sonar.issue.ignore.multicriteria` :

- cpp:S5025 est ignoré sous `src/ui/**/*`, là où l'idiome parent-enfant de Qt est utilisé. Le cœur métier (src/core) n'est pas exclu : il utilise RAII et la pile, et doit continuer d'être analysé par cette règle.
- cpp:S2259 est ignoré sous `tests/**/*`. En complément, le code de test ajoute une garde explicite `if (ptr == nullptr) return;` après chaque QVERIFY de non-nullité, forme que le moteur symbolique comprend nativement.

Le fichier ne déclare que ces exclusions ; la clé de projet, l'organisation et la configuration du build-wrapper C++ restent portées par l'invocation du scanner.

## Conséquences

- La grille de qualité cesse d'échouer sur ces faux positifs, sans masquer d'éventuels vrais défauts dans le cœur métier.
- La décision est tracée dans le dépôt et reproductible pour toute machine qui lance le scanner, sans dépendre d'un réglage manuel dans l'interface web SonarCloud.
- Une vraie mauvaise gestion mémoire introduite dans src/core resterait détectée ; seule la couche interface, où l'idiome Qt s'applique, est exemptée de cpp:S5025.
- Les tests gagnent une garde de non-nullité explicite, légèrement redondante avec QVERIFY mais lisible et utile au moteur d'analyse.
