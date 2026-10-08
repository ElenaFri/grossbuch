#pragma once

#include <QDialog>

namespace grossbuch {

// Boîte de dialogue Guide d'utilisation : rappel concis du fonctionnement de
// l'application (saisie, récurrents, consultation, échange et sauvegardes).
// Ouverte depuis le menu Aide. Voir docs/adr/0016.
class GuideDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GuideDialog(QWidget *parent = nullptr);
};

} // namespace grossbuch
