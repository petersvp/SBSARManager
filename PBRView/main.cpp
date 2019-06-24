#include "pbrview.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    PBRView w;
    w.show();

    return a.exec();
}
