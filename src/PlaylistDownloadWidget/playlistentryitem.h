#ifndef PLAYLISTENTRYITEM_H
#define PLAYLISTENTRYITEM_H

#include <QWidget>
#include <QtNetwork>

#include "utils.h"

#include "ui_playlistitemoverlay.h"

namespace Ui {
class PlaylistEntryItem;
}

class PlaylistEntryItem : public QWidget
{
    Q_OBJECT

public:
    explicit PlaylistEntryItem(QWidget *parent = nullptr, QNetworkAccessManager *manager = nullptr);
    ~PlaylistEntryItem();

    enum Status {
        Error,
        Running,
        Finished,
        NotRunning,
        Queued
    };
    Q_ENUM(Status)

signals:
    void viewPlaylist(QString playlistId);
    void viewPlaylistInfo();
    void selectItem(QPoint itemPos);
public slots:
    void setStatusText(QString str);
    void init(QString download_record_filename, QString id, QString title,
              QString playlistThumbnail, QString author, QString authorId,
              int videoCount, QString videoMetaStr, QList<int>quality,
              QString container, QString download_type,QString download_location);
    void animate();
    void setStatus(PlaylistEntryItem::Status status);
    QString getPlaylistRecordFileName();
    QString getPlaylistId();
    QString getPlaylistUUID();
    int getPlaylistItemsCount();
    void appendFailed();
    void appendFinished();
    void appendQueued();

    void resetFailed();
    void resetQueued();
    QString getPlaylistDownloadLocation();
protected slots:
    void resizeEvent(QResizeEvent *event);
    void moveEvent(QMoveEvent *event);
private slots:
    void on_menuButton_clicked();
    void init_progressFile();
    void updateStatusText();

private:
    Ui::PlaylistEntryItem *ui;
    Ui::PlaylistItemOverlay overlay_ui;
    QString playlistId, download_record_filename, downloadLocation;
    QMap<QString, QString> statusColor;
    QWidget *overlay_widget = nullptr;

    QNetworkAccessManager *networkManager_ = nullptr;
    QSettings *settings;
    int itemsCount;
};

#endif // PLAYLISTENTRYITEM_H
