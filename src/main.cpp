#include "mainwindow.h"
#include <QApplication>
#include <QMessageBox>
#include <QDebug>

#include "rungaurd.h"
#include "version.h"

int main(int argc, char *argv[])
{
    /*
     * Add extra arguments on runtime
     */
    char ARG_DISABLE_WEB_SECURITY[] = "--disable-web-security";
    int newArgc = argc+1+1;
    char** newArgv = new char*[newArgc];
    for(int i=0; i<argc; i++) {
        newArgv[i] = argv[i];
    }
    newArgv[argc] = ARG_DISABLE_WEB_SECURITY;
    newArgv[argc+1] = nullptr;

    #ifdef QT_DEBUG
        qputenv("QTWEBENGINE_REMOTE_DEBUGGING", "23654");
    #endif

    QApplication a(newArgc, newArgv);

    QApplication::setApplicationName("Playlist DL");
    QApplication::setOrganizationName("org.keshavnrj.ubuntu");
    QApplication::setApplicationVersion(PLDL_VERSION);


    //allow multiple instances in debug builds
    #ifndef QT_DEBUG
    QString appname = QApplication::applicationName();
        RunGuard guard("org.keshavnrj.ubuntu."+appname);
        if ( !guard.tryToRun() ){
            QMessageBox::critical(0, appname,"An instance of "+appname+" is already running.");
            return 0;
        }
    #endif

    MainWindow w;
    w.show();

    return a.exec();
}
