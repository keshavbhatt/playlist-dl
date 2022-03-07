#ifndef PLAYLISTSEARCH_H
#define PLAYLISTSEARCH_H

#include <QWidget>
#include <QtNetwork>

#include "onlinesearchsuggestion.h"
#include "playlistitem.h"
#include "utils.h"
#include "waitingspinnerwidget.h"
#include "playlistview.h"

namespace Ui {
class PlaylistSearch;
}

class PlaylistSearch : public QWidget
{
    Q_OBJECT

public:
    explicit PlaylistSearch(QWidget *parent = nullptr, QNetworkAccessManager *manager = nullptr);
    ~PlaylistSearch();

signals:
    void loadPlaylist(QString playlistId);

public slots:
    void cancelAllRequests();

private slots:
    void processResult();
    void on_searchLineEdit_returnPressed();

    void doSearch();
    QString cleanUrl(QString string);
    void parseResult(const QJsonDocument jsonResponse);
    void on_searchButton_clicked();

    void on_searchLineEdit_textChanged(const QString &arg1);

    void on_resultsListWidget_itemDoubleClicked(QListWidgetItem *item);

    void noResults();

    bool isPlaylistUrl(QString arg1);
    QString getPlaylistId(QString arg1);
private:
    Ui::PlaylistSearch *ui;

    QNetworkAccessManager *n_manager;
    QNetworkReply *reply;

    WaitingSpinnerWidget *_loader;
    QList<QNetworkReply *>replies;

};

#endif // PLAYLISTSEARCH_H
