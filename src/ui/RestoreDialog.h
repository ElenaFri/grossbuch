#pragma once

#include <QDialog>

class QListWidget;
class QPushButton;

namespace grossbuch {

class DataController;

// Boîte de dialogue de restauration : liste les sauvegardes disponibles et
// restaure celle que l'on choisit, après confirmation. En cas de succès, la
// boîte est acceptée (QDialog::Accepted) et l'appelant redémarre l'application.
// Reprend les fonctions de l'ancien onglet Données. Voir docs/adr/0016 et 0013.
class RestoreDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RestoreDialog(DataController &controller, QWidget *parent = nullptr);

private slots:
    void onRestore();
    void updateRestoreButton();

private:
    void reloadBackups();

    DataController &m_controller;
    QListWidget *m_backups = nullptr;
    QPushButton *m_restore = nullptr;
};

} // namespace grossbuch
