# 0016 - Navigation par barre de menus et vues empilées

Date : 2026-10-07

## Statut

Accepté

## Contexte

L'interface reposait sur un QTabWidget : chaque fonction (saisie, paiements récurrents, récapitulatif, graphiques, données, à propos) était un onglet affiché en permanence. Deux limites sont apparues. D'une part, la barre d'onglets mélangeait des lieux de travail quotidiens et des éléments méta ou ponctuels (données, à propos), ce qui occupait inutilement la navigation principale. D'autre part, l'application ne proposait aucune barre de menus, alors que les actions sur les données (import, export, restauration) et les éléments d'aide y trouvent naturellement leur place.

## Décision

L'application adopte une barre de menus classique et n'affiche qu'une vue à la fois, via un QStackedWidget. La barre d'onglets est supprimée.

- Menu Fichier : importer des données (Ctrl+O), exporter des données (Ctrl+E), restaurer une sauvegarde, quitter (Ctrl+Q).
- Menu Édition (vues de saisie) : saisie des dépenses (Ctrl+1), dépenses récurrentes (Ctrl+2).
- Menu Affichage (vues de consultation) : récapitulatif (Ctrl+3), graphiques (Ctrl+4).
- Menu Aide : guide d'utilisation (F1), à propos.

Les quatre actions de navigation forment un groupe exclusif et cochable : la vue active est toujours signalée par une coche dans les menus. La navigation passe exclusivement par les menus et leurs raccourcis Ctrl+1 à Ctrl+4 : aucune barre d'outils n'est affichée. Au lancement, l'application ouvre systématiquement la vue Graphiques (qui présente notamment l'année en cours) ; la vue courante n'est volontairement pas mémorisée d'une session à l'autre. Seule la géométrie de la fenêtre est persistée dans QSettings.

L'ancien onglet Données est dissous : import et export deviennent des actions du menu Fichier, et la liste des sauvegardes migre dans une boîte de dialogue de restauration. L'ancien onglet À propos devient une boîte de dialogue ouverte depuis le menu Aide, à laquelle s'ajoute un guide d'utilisation concis.

## Conséquences

- La navigation principale est concentrée sur les quatre vues de travail ; les actions et le méta sont rangés dans les menus, conformément aux conventions de bureau.
- Les raccourcis clavier Ctrl+1 à Ctrl+4 compensent le clic supplémentaire qu'impose une navigation par menu.
- La séparation cœur/interface de l'ADR 0004 reste valable ; seul le mode de navigation de la couche interface change. Les widgets de vue (EntryTab, RecurringTab, SummaryTab, ChartsTab) sont inchangés et restent testables indépendamment.
- Les tests d'interface portent désormais sur la pile de vues, la vue affichée au lancement, l'absence de barre d'outils et la présence des actions de menu, plutôt que sur des onglets.
