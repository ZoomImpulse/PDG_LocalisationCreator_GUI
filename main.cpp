#include "PDG_LocalisationCreator_GUI.h"
#include <QtWidgets/QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    // Set global application/window icon from resources
    app.setWindowIcon(QIcon(":/PDG_LocalisationCreator_GUI/icons/app.png"));
    PDG_LocalisationCreator_GUI window;
    window.show();
    return app.exec();
}
