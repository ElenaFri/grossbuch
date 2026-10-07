#pragma once

#include <QWidget>

namespace grossbuch {

// Onglet À propos : informations statiques sur l'application (nom, version,
// licence, auteur, dépôt et version de Qt à l'exécution). La version provient de
// l'en-tête généré Version.h (PROJECT_VERSION). Voir docs/adr/0005.
class AboutTab : public QWidget
{
    Q_OBJECT

public:
    explicit AboutTab(QWidget *parent = nullptr);
};

} // namespace grossbuch
