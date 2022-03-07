#ifndef PLAYLISTDOWNLOADOPTIONS_H
#define PLAYLISTDOWNLOADOPTIONS_H

#include <QWidget>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "utils.h"
#include "videoitem.h"
#include "account.h"

namespace Ui {
class PlaylistDownloadOptions;
}

class PlaylistDownloadOptions : public QWidget
{
    Q_OBJECT

public:
    explicit PlaylistDownloadOptions(QWidget *parent = nullptr,
                                     QNetworkAccessManager *manager = nullptr,
                                     account *accountManager = nullptr);
    ~PlaylistDownloadOptions();

signals:
    void addToDownload(QString download_record_filename);

public slots:
    void loadSelected(const QString playlistId, const QStringList itemIdList);
    void setAccountManager(account *accountManager = nullptr);
private slots:
    void resetUi();
    void on_videoRadioButton_toggled(bool checked);

    void on_audioRadioButton_toggled(bool checked);

    void loadToView(const QString data, const QStringList itemIdList);
    void on_addToDownload_clicked();

    QString generateDownloadRecordFile(QString playlistId);
    int getAudioQuality();
    int getVideoQuality();
    int getVideoAudioQuality();
    QString getVideoContainer();
    QString getAudioContainer();
    void updateStatus();
    void on_changeLocation_clicked();

    bool is_pro_feature();
private:
    Ui::PlaylistDownloadOptions *ui;
    QString playlistCacheFileName;
    QString playlist_id, uploader_name, playlist_name;
    QNetworkAccessManager *networkManager_ = nullptr;

    QJsonArray addedItemArray;
    account *m_account = nullptr;

};

#endif // PLAYLISTDOWNLOADOPTIONS_H
