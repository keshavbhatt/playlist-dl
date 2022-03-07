#ifndef PLAYLISTITEM_H
#define PLAYLISTITEM_H

#include <QWidget>
#include "utils.h"
#include <QtNetwork>

namespace Ui {
class PlayListItem;
}

class PlayListItem : public QWidget
{
    Q_OBJECT

public:
    explicit PlayListItem(QWidget *parent = nullptr, QNetworkAccessManager *manager = nullptr);
    ~PlayListItem();

signals:
    void viewPlaylist(QString playlistId);
    void selectItem(QPoint itemPos);

public slots:
    QString getPlaylistId();
    void init(QString id, QString title, QString playlistThumbnail, QString author, QString authorId, int videoCount, QList<QStringList> videoMeta);
private slots:
    void animate();
    void on_menuButton_clicked();

private:
    Ui::PlayListItem *ui;
    QString playlistId;

    QNetworkAccessManager *networkManager_ = nullptr;
};

#endif // PLAYLISTITEM_H
