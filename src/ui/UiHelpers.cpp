#include "ui/UiHelpers.h"

#include "core/Category.h"

#include <QComboBox>
#include <QLabel>
#include <QLocale>
#include <QStandardItemModel>
#include <QTimer>

#include <algorithm>

namespace grossbuch {

QString formatMoney(qint64 cents)
{
    return QLocale().toCurrencyString(static_cast<double>(cents) / 100.0);
}

QHash<int, Category> categoriesById(const QVector<Category> &categories)
{
    QHash<int, Category> byId;
    byId.reserve(categories.size());
    for (const Category &category : categories)
        byId.insert(category.id, category);
    return byId;
}

QString categoryDisplayName(const QHash<int, Category> &byId, int categoryId)
{
    const Category category = byId.value(categoryId);
    if (category.id == 0)
        return QString();
    if (category.isRoot())
        return category.name;
    return QStringLiteral("%1 / %2").arg(byId.value(category.parentId.value()).name, category.name);
}

void populateCategoryCombo(QComboBox *combo, const QVector<Category> &categories)
{
    const int previous = combo->currentData().isValid() ? combo->currentData().toInt() : 0;

    combo->clear();
    auto *model = qobject_cast<QStandardItemModel *>(combo->model());

    QVector<Category> roots;
    QHash<int, QVector<Category>> childrenByParent;
    for (const Category &category : categories) {
        if (category.isRoot())
            roots.append(category);
        else
            childrenByParent[category.parentId.value()].append(category);
    }

    auto byName = [](const Category &a, const Category &b) { return a.name < b.name; };
    std::sort(roots.begin(), roots.end(), byName);

    int firstSelectable = -1;
    for (const Category &root : roots) {
        QVector<Category> children = childrenByParent.value(root.id);
        if (children.isEmpty()) {
            // Racine sans enfant : sélectionnable directement.
            combo->addItem(root.name, root.id);
            if (firstSelectable < 0)
                firstSelectable = combo->count() - 1;
        } else {
            // En-tête non sélectionnable, puis les sous-catégories.
            combo->addItem(root.name);
            if (model != nullptr)
                model->item(combo->count() - 1)->setFlags(Qt::NoItemFlags);

            std::sort(children.begin(), children.end(), byName);
            for (const Category &child : children) {
                combo->addItem(QStringLiteral("    %1").arg(child.name), child.id);
                if (firstSelectable < 0)
                    firstSelectable = combo->count() - 1;
            }
        }
    }

    if (previous > 0)
        selectComboCategory(combo, previous);
    if (combo->currentData().toInt() <= 0 && firstSelectable >= 0)
        combo->setCurrentIndex(firstSelectable);
}

void selectComboCategory(QComboBox *combo, int categoryId)
{
    for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).toInt() == categoryId) {
            combo->setCurrentIndex(i);
            return;
        }
    }
}

void showFeedback(QLabel *label, const QString &text, bool error)
{
    label->setText(text);
    label->setStyleSheet(error ? QStringLiteral("color: #c0392b;")
                               : QStringLiteral("color: #27ae60;"));
    QTimer::singleShot(4000, label, [label] { label->clear(); });
}

} // namespace grossbuch
