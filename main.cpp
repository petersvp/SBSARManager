#include "DarkStyle.h"
#include "substancemanagerapp.h"
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include "pbrview.h"
#include "qwinwidget.h"



int main(int argc, char *argv[])
{
    QApplication::setApplicationDisplayName("SBSAR Library Manager");
    QApplication a(argc, argv);

    SubstanceManagerApp app;
    app.show();

    return a.exec();
}
