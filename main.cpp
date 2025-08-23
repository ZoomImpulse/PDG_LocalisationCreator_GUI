#include "PDG_LocalisationCreator_GUI.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    PDG_LocalisationCreator_GUI window;
    window.show();
    return app.exec();
}
