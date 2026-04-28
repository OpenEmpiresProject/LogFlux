#include "LogFlux.h"
#include <QtWidgets/QApplication>
#include "DataSource.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
	app.setWindowIcon(QIcon(":/images/app.png"));

    LogFlux window;
    window.show();
    return app.exec();
}
