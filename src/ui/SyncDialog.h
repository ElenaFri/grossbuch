#pragma once

#include <QDialog>

class QCheckBox;
class QLineEdit;

namespace grossbuch {

class SyncController;

// Boîte de dialogue de configuration de la synchronisation distante : choix du
// dossier partagé (géré par un outil tiers comme Syncthing ou Nextcloud) et
// activation de la synchronisation automatique à l'ouverture et à la fermeture.
// À l'acceptation, la configuration est écrite dans le SyncController. Voir
// docs/adr/0020.
class SyncDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SyncDialog(SyncController &controller, QWidget *parent = nullptr);

private slots:
    void onBrowse();
    void onAccept();

private:
    SyncController &m_controller;
    QLineEdit *m_folderEdit = nullptr;
    QCheckBox *m_autoSyncCheck = nullptr;
};

} // namespace grossbuch
