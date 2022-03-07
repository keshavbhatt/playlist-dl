#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QAction>
#include <QSettings>

#include "about.h"
#include "playlistsearch.h"
#include "settingwidget.h"
#include "playlistview.h"
#include "playlistdownloadoptions.h"
#include "downloadwidget.h"
#include "webengineplayer.h"
#include "spinner.h"
#include "account.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    bool quiting = false;

public slots:
    void switchStackWidget(QWidget *widget, bool addToStackVector = true);
    void playVideo(QString videoId);
    void playPlaylist(QString plsylistId);
    void playAuthorUploads(QString authorId);
    void startSpinner();
    void stopSpinner();
protected slots:
    void closeEvent(QCloseEvent *event);
    bool eventFilter(QObject *obj, QEvent *event);
private slots:
    void createActions();

    void showAbout();
    void showSettings();
    void init_settings();

    void updateWindowTheme();
    void cancelAllRequests();
    void init_account();
    void enablePro();
    void disablePro();
private:
    Ui::MainWindow *ui;
    PlaylistSearch *playlistSearchWidget;
    PlaylistView *playlistViewWidget;
    PlaylistDownloadOptions *playlistDownloadOptionsWidget;
    DownloadWidget *downloadWidget;
    WebEnginePlayer *webenginePlayerWidget;

    QPalette lightPalette;

    QAction *homeAction,
            *settingAction,
            *searchAction,
            *downloadsAction,
            *aboutAction,
            *playerAction,
            *accountAction;

    QSettings settings;
    SettingWidget *settingWidget;

    QVector<QWidget*>stackVector;

    QNetworkAccessManager *n_manager = nullptr;

    Spinner *spinner = nullptr;

    account *accountWidget = nullptr;


};

#endif // MAINWINDOW_H
