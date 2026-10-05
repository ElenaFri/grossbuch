#include <QApplication>
#include <QLabel>
#include <QMainWindow>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("grossbuch"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setOrganizationName(QStringLiteral("grossbuch"));

    QMainWindow window;
    window.setWindowTitle(QStringLiteral("grossbuch"));
    window.resize(900, 600);

    auto *placeholder = new QLabel(QStringLiteral("grossbuch — comptabilité privée"));
    placeholder->setAlignment(Qt::AlignCenter);
    window.setCentralWidget(placeholder);

    window.show();
    return QApplication::exec();
}
