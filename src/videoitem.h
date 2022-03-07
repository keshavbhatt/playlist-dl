#ifndef VIDEOITEM_H
#define VIDEOITEM_H

#include <QWidget>
#include <QtNetwork>
#include "utils.h"

namespace Ui {
class VideoItem;
}

class VideoItem : public QWidget
{
    Q_OBJECT

signals:
    void itemCheckedChanged();
    void playVideo(QString videoId);

public:
    explicit VideoItem(QWidget *parent = nullptr, QNetworkAccessManager *manager = nullptr);
    ~VideoItem();

public slots:
    QString getVideoId();
    void hideSelector();
    void animate();
    void init(int pos, QString videoId, QString title, QString thumb, QString duration);
private slots:
    void on_checkBox_toggled(bool checked);

    void on_play_clicked();

private:
    Ui::VideoItem *ui;
    QString videoId;
    QNetworkAccessManager *networkManager_ = nullptr;

};

#endif // VIDEOITEM_H
