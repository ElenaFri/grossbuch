#pragma once

#include <QWidget>

class QLabel;
class QListWidget;
class QPushButton;

namespace grossbuch {

class DataController;

// Onglet Données : export vers un fichier d'échange, import avec fusion (rapport
// affiché), et restauration d'une sauvegarde choisie dans la liste. Les actions
// sont déléguées au contrôleur de données. Voir docs/adr/0012 et docs/adr/0013.
class DataTab : public QWidget
{
    Q_OBJECT

public:
    explicit DataTab(DataController &controller, QWidget *parent = nullptr);

    // Recharge la liste des sauvegardes disponibles.
    void refresh();

signals:
    // Émis après une restauration réussie : l'application doit redémarrer.
    void restoreCompleted();

private slots:
    void onExport();
    void onImport();
    void onRestore();
    void updateRestoreButton();

private:
    void reloadBackups();
    void showResult(const QString &message, bool error);

    DataController &m_controller;

    QPushButton *m_export = nullptr;
    QPushButton *m_import = nullptr;
    QListWidget *m_backups = nullptr;
    QPushButton *m_restore = nullptr;
    QLabel *m_result = nullptr;
};

} // namespace grossbuch
