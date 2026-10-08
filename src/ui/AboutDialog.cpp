#include "ui/AboutDialog.h"

#include "Version.h"

#include <QDialogButtonBox>
#include <QFont>
#include <QLabel>
#include <QVBoxLayout>

namespace grossbuch {

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("À propos de grossbuch"));

    auto *title = new QLabel(QStringLiteral("grossbuch"), this);
    title->setObjectName(QStringLiteral("aboutTitle"));
    QFont titleFont = title->font();
    titleFont.setPointSizeF(titleFont.pointSizeF() * 1.8);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);

    auto *version = new QLabel(tr("Version %1").arg(QString::fromLatin1(kAppVersion)), this);
    version->setObjectName(QStringLiteral("aboutVersion"));
    version->setAlignment(Qt::AlignCenter);

    auto *description = new QLabel(
        tr("Application de comptabilité familiale : saisie des dépenses par catégories, "
           "paiements récurrents, récapitulatifs, graphiques annuels et échange de données."),
        this);
    description->setAlignment(Qt::AlignCenter);
    description->setWordWrap(true);

    auto *author = new QLabel(tr("Auteur : Elena FRISON"), this);
    author->setAlignment(Qt::AlignCenter);

    auto *license = new QLabel(tr("Licence : MIT"), this);
    license->setAlignment(Qt::AlignCenter);

    auto *repository = new QLabel(
        QStringLiteral("<a href=\"https://github.com/ElenaFri/grossbuch\">"
                       "https://github.com/ElenaFri/grossbuch</a>"),
        this);
    repository->setAlignment(Qt::AlignCenter);
    repository->setTextFormat(Qt::RichText);
    repository->setTextInteractionFlags(Qt::TextBrowserInteraction);
    repository->setOpenExternalLinks(true);

    auto *qtVersion =
        new QLabel(tr("Développé avec Qt %1").arg(QString::fromLatin1(qVersion())), this);
    qtVersion->setAlignment(Qt::AlignCenter);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(title);
    layout->addWidget(version);
    layout->addSpacing(12);
    layout->addWidget(description);
    layout->addSpacing(12);
    layout->addWidget(author);
    layout->addWidget(license);
    layout->addWidget(repository);
    layout->addSpacing(12);
    layout->addWidget(qtVersion);
    layout->addSpacing(12);
    layout->addWidget(buttons);
}

} // namespace grossbuch
