#ifndef DOWNLOADWIDGET_H
#define DOWNLOADWIDGET_H

#include <QWidget>
#include <QDebug>
#include <QSplitter>
#include <QtNetwork>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QListWidget>

#include "utils.h"
#include "playlistentryitem.h"
#include "downloaditem.h"
#include "downloadmanager.h"


namespace Ui {
class DownloadWidget;
}

class DownloadWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DownloadWidget(QWidget *parent = nullptr,QNetworkAccessManager *manager = nullptr);
    ~DownloadWidget();

public slots:
    void home();
    void addToDownload(QString download_record_filename, bool animate = true);
    QString get_download_record_dir_path();
protected slots:
    bool eventFilter(QObject *watched, QEvent *event);
private slots:
    void loadPlaylist(QString download_record_filename);
    void loadHistory();
    void on_backToPlaylistButton_clicked();

    void on_playlistWidget_itemDoubleClicked(QListWidgetItem *item);

    void init_downloadManager();
    void on_startStopButton_clicked();

    void on_removeButton_clicked();

    void on_playlistWidget_currentRowChanged(int currentRow);

    void showPlaylistInfo(QString download_record_filename);
    void updateButtons();
    void updatePlaylistItems();
    void on_playlistViewlistWidget_currentRowChanged(int currentRow);

    void on_playlistFilterLineEdit_textChanged(const QString &arg1);

    void filterList(const QString &arg1, QListWidget *listWidget);
    void fillFilteredItemsMetaList(QListWidget *listWidget);
    void hideAllListItems(QListWidget *listWidget);
    void updatePlaylisInfoButton(PlaylistEntryItem *playlistItem);
private:
    Ui::DownloadWidget *ui;
    QNetworkAccessManager *networkManager_;
    QString download_record_dir_path;

    DownloadManager *downloadManager;
    QList<QString> filteredItemsMetaList;

};


#endif // DOWNLOADWIDGET_H
