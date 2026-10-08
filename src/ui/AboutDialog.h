#pragma once

#include <QDialog>

namespace grossbuch {

// Boîte de dialogue À propos : informations statiques sur l'application (nom,
// version, licence, auteur, dépôt et version de Qt). La version provient de
// l'en-tête généré Version.h (PROJECT_VERSION). Ouverte depuis le menu Aide.
// Voir docs/adr/0016.
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
};

} // namespace grossbuch
