#ifndef PLAYLISTVIEW_H
#define PLAYLISTVIEW_H

#include <QClipboard>
#include <QListWidget>
#include <QWidget>

#include "engine.h"
#include "utils.h"
#include "videoitem.h"
#include "waitingspinnerwidget.h"

namespace Ui {
class PlaylistView;
}

class PlaylistView : public QWidget {
  Q_OBJECT

public:
  explicit PlaylistView(QWidget *parent = nullptr,
                        QVariant playlist_id = QVariant(""));
  ~PlaylistView();

public slots:
  void loadPlaylist(const QString playlistId);

signals:
  void downloadSelected(const QString playlistId,
                        const QStringList itemsIdList);
  void playPlaylist(QString playlisId);
  void playAuthorUploads(QString authorId);

private slots:
  void flatterFinished(int exitCode);
  void flat_playlist(QVariant playlist_id);
  void flattererror();
  void on_selectAllCheckBox_toggled(bool checked);

  void selectAllItemsInView(QListWidget *listWidget, bool checked);
  int checkItemCount(QListWidget *listWidget);
  void updateStatusLabel();
  void hideAllListItems(QListWidget *listWidget);
  void fillFilteredItemsMetaList(QListWidget *listWidget);
  void filterList(const QString &arg1, QListWidget *listWidget);
  void on_filterLineEdit_textChanged(const QString &arg1);

  void on_resultsListWidget_itemDoubleClicked(QListWidgetItem *item);

  void loadToView(const bool fromCache, const QString data);
  void on_downloadPushButton_clicked();

  void resetUi();
  QStringList getSelectedItemId(QListWidget *listWidget);
  void on_playPlaylist_clicked();

  void on_playAuthotUploads_clicked();

  void on_copyId_clicked();

  void on_forceReload_clicked();

private:
  Ui::PlaylistView *ui;
  QString playlist_id, playlistCacheFileName, playlist_author;

  WaitingSpinnerWidget *_loader;
  QList<QString> filteredItemsMetaList;

  QNetworkAccessManager *networkManager_ = nullptr;
};

#endif // PLAYLISTVIEW_H
