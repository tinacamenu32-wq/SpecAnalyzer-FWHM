#include <QApplication>

#include "MainWindow.h"
#include "UiTheme.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
    QApplication app(argc, argv);
    app.setApplicationName("SpecAnalyzer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("GeologicalSurvey");

    UiTheme::apply(app);

    MainWindow window;
    window.show();
    return app.exec();
}
