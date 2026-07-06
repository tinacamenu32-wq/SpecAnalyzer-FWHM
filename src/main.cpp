#include <QApplication>

#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("SpecAnalyzer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("GeologicalSurvey");

    app.setStyle("Fusion");

    MainWindow window;
    window.show();
    return app.exec();
}
