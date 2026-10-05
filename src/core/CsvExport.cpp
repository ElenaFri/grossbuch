#include "core/CsvExport.h"

#include <algorithm>

namespace grossbuch {

namespace {

// Formate des centimes en « 1234,56 » sans dépendre de la locale (la décimale
// est toujours une virgule, pas de séparateur de milliers, pour un CSV stable).
QString formatCents(qint64 cents)
{
    const qint64 absolute = cents < 0 ? -cents : cents;
    const QString sign = cents < 0 ? QStringLiteral("-") : QString();
    return QStringLiteral("%1%2,%3")
        .arg(sign)
        .arg(absolute / 100)
        .arg(absolute % 100, 2, 10, QLatin1Char('0'));
}

// Échappe un champ CSV si nécessaire (présence de « ; », guillemet ou saut de
// ligne), selon la convention RFC 4180.
QString escapeField(const QString &value)
{
    if (!value.contains(QLatin1Char(';')) && !value.contains(QLatin1Char('"'))
        && !value.contains(QLatin1Char('\n')) && !value.contains(QLatin1Char('\r')))
        return value;
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

void appendRow(QString &out, const QString &category, const QString &subcategory, qint64 cents)
{
    out += escapeField(category);
    out += QLatin1Char(';');
    out += escapeField(subcategory);
    out += QLatin1Char(';');
    out += formatCents(cents);
    out += QLatin1Char('\n');
}

} // namespace

QString summaryToCsv(const QVector<Category> &categories,
                     const QHash<int, qint64> &amountsByCategory)
{
    // Reconstruit la hiérarchie à deux niveaux.
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

    QString out = QStringLiteral("Catégorie;Sous-catégorie;Montant (€)\n");
    qint64 grandTotal = 0;

    for (const Category &root : roots) {
        QVector<Category> children = childrenByParent.value(root.id);
        if (children.isEmpty()) {
            const qint64 amount = amountsByCategory.value(root.id);
            if (amount == 0)
                continue;
            appendRow(out, root.name, QString(), amount);
            grandTotal += amount;
            continue;
        }

        std::sort(children.begin(), children.end(), byName);
        for (const Category &child : children) {
            const qint64 amount = amountsByCategory.value(child.id);
            if (amount == 0)
                continue;
            appendRow(out, root.name, child.name, amount);
            grandTotal += amount;
        }
    }

    appendRow(out, QStringLiteral("Total"), QString(), grandTotal);
    return out;
}

} // namespace grossbuch
