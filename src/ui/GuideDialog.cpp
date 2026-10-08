#include "ui/GuideDialog.h"

#include <QDialogButtonBox>
#include <QTextBrowser>
#include <QVBoxLayout>

namespace grossbuch {

// Contenu d'aide statique uniquement : aucune logique à tester. Exclu de la mesure
// de couverture (un test se réduirait à vérifier que du texte contient du texte).
// Voir docs/adr/0019.
// LCOV_EXCL_START
GuideDialog::GuideDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Guide d'utilisation"));
    resize(560, 440);

    auto *browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(true);
    browser->setHtml(tr(
        "<h3>Bienvenue dans grossbuch</h3>"
        "<p>L'application s'utilise via la barre de menus. Une seule vue est "
        "affichée à la fois ; les raccourcis Ctrl+1 à Ctrl+4 permettent de passer "
        "de l'une à l'autre.</p>"
        "<h4>Saisie</h4>"
        "<p>Menu Édition, « Saisie des dépenses » (Ctrl+1). Enregistrez une dépense "
        "en indiquant son montant, sa date, un libellé facultatif et sa "
        "(sous-)catégorie. La liste des dépenses du mois en cours permet de modifier "
        "ou de supprimer une entrée.</p>"
        "<h4>Dépenses récurrentes</h4>"
        "<p>Menu Édition, « Dépenses récurrentes » (Ctrl+2). Définissez des paiements "
        "qui reviennent chaque mois ; ils sont reportés automatiquement sur le mois "
        "en cours. Les modifications ne concernent que l'avenir, jamais les mois "
        "déjà passés. Une dépense récurrente peut être désactivée puis réactivée "
        "sans perdre son historique.</p>"
        "<h4>Consultation</h4>"
        "<p>Menu Affichage : « Récapitulatif » (Ctrl+3) agrège les dépenses par "
        "catégorie et sous-catégorie, pour un mois ou une année au choix, avec le "
        "total ; « Graphiques » (Ctrl+4) superpose les courbes mensuelles des "
        "dernières années.</p>"
        "<h4>Échange et sauvegardes</h4>"
        "<p>Menu Fichier : « Exporter des données » crée un fichier à partager ; "
        "« Importer des données » fusionne un fichier reçu sans jamais créer de "
        "doublon, après une sauvegarde automatique. « Restaurer une sauvegarde » "
        "revient à un état antérieur. Une sauvegarde est créée chaque jour et avant "
        "chaque import ; les plus anciennes sont effacées automatiquement.</p>"));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(browser);
    layout->addWidget(buttons);
}
// LCOV_EXCL_STOP

} // namespace grossbuch
