#include "playlistsearch.h"
#include "mainwindow.h"
#include "ui_playlistsearch.h"

#include <QMessageBox>
#include <QRegularExpression>
#include <QScrollBar>

PlaylistSearch::PlaylistSearch(QWidget *parent, QNetworkAccessManager *manager)
    : QWidget(parent), ui(new Ui::PlaylistSearch) {
  ui->setupUi(this);

  ui->searchButton->setEnabled(false);
  ui->forceReload->setEnabled(false);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
  ui->searchLineEdit->setClearButtonEnabled(true);
#endif

  ui->resultsListWidget->setSpacing(4);
  ui->resultsListWidget->setUniformItemSizes(true);
  // ui->resultsListWidget->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

  ui->searchLineEdit->addAction(QIcon(":/icons/youtube-line.png"),
                                QLineEdit::LeadingPosition);
  ui->searchLineEdit->setPlaceholderText(
      tr("Search YouTube playlist or paste Playlist Url"));

  onlineSearchSuggestion *ss = new onlineSearchSuggestion(ui->searchLineEdit);
  ui->searchLineEdit->installEventFilter(ss);

  // loader is the child of results
  _loader = new WaitingSpinnerWidget(ui->resultsListWidget, true, true);
  _loader->setRoundness(70.0);
  _loader->setMinimumTrailOpacity(15.0);
  _loader->setTrailFadePercentage(70.0);
  _loader->setNumberOfLines(9);
  _loader->setLineLength(12);
  _loader->setLineWidth(2);
  _loader->setInnerRadius(2);
  _loader->setRevolutionsPerSecond(3);
  _loader->setColor(QColor("#1e90ff"));

  n_manager = manager;

  // TEST
  //    ui->searchLineEdit->setText("pink");
  //    QTimer::singleShot(1000,this,SLOT(on_searchButton_clicked()));
}

void PlaylistSearch::cancelAllRequests() {
  foreach (auto &reply, n_manager->findChildren<QNetworkReply *>()) {
    if (!reply->isReadable()) {
      return;
    }
    reply->abort();
    reply->deleteLater();
  }
}

PlaylistSearch::~PlaylistSearch() { delete ui; }

void PlaylistSearch::on_searchLineEdit_returnPressed() {
  if (!ui->searchLineEdit->text().trimmed().isEmpty()) {
    doSearch();
  }
}

void PlaylistSearch::doSearch() {
  if (ui->searchButton->text().contains("playlist", Qt::CaseInsensitive)) {
    QString extractedId = getPlaylistId(ui->searchLineEdit->text());
    if (extractedId.trimmed().isEmpty()) {
      QMessageBox::information(
          this, QApplication::applicationName() + " | Error",
          "Unable to extract the Playlist ID for processing,\n"
          "try to search the playlist manually.");
    } else {
      emit loadPlaylist(extractedId);
    }
  } else {

    _loader->start();

    ui->searchButton->setEnabled(false);
    ui->forceReload->setEnabled(false);

    QString term = ui->searchLineEdit->text();

    QUrl url("https://ktechpit.com/USS/Olivia/youtube/api.php");
    QUrlQuery query;
    query.addQueryItem("query", term);
    url.setQuery(query);

    QNetworkRequest req(url);
    reply = n_manager->get(req);
    connect(reply, SIGNAL(finished()), this, SLOT(processResult()));
  }
}

void PlaylistSearch::processResult() {
  ui->resultsListWidget->clear();
  ui->resultsListWidget->verticalScrollBar()->setValue(
      ui->resultsListWidget->verticalScrollBar()->minimum());

  _loader->stop();

  ui->searchButton->setEnabled(true);
  ui->forceReload->setEnabled(true);

  m_currentLoadedUrl = reply->request().url();
  if (reply->error() == QNetworkReply::NoError) {
    QString replyStr = reply->readAll();
    QJsonDocument jsonResponse = QJsonDocument::fromJson(replyStr.toUtf8());
    if (jsonResponse.array().isEmpty()) {
      // QMessageBox::critical(this,QApplication::applicationName()+" |
      // Error","API:Empty response returned from API call. Please report to
      // developer.");
      this->noResults();
      n_manager->cache()->remove(reply->request().url());
      return;
    } else {
      parseResult(jsonResponse);
    }
  } else {
    // no error message if user cancels request
    if (reply->error() == QNetworkReply::OperationCanceledError)
      return;
    QMessageBox::critical(this, QApplication::applicationName() + " | Error",
                          "An error occured while search " +
                              cleanUrl(reply->errorString()));
  }
}

void PlaylistSearch::noResults() {
  QLabel *listTitle = new QLabel(ui->resultsListWidget);
  listTitle->setText(QString("<b>No result found for your query %1</b>")
                         .arg(ui->searchLineEdit->text()));
  listTitle->setAlignment(Qt::AlignCenter);
  listTitle->setFixedHeight(32);
  QListWidgetItem *item;
  item = new QListWidgetItem(ui->resultsListWidget);
  item->setFlags(Qt::NoItemFlags);
  item->setSizeHint(listTitle->minimumSizeHint());
  ui->resultsListWidget->setItemWidget(item, listTitle);
  ui->resultsListWidget->itemWidget(item)->setEnabled(false);
  ui->resultsListWidget->addItem(item);
}

void PlaylistSearch::parseResult(const QJsonDocument jsonResponse) {
  QObject *mainWindowObject = utils::getMainWindow(this);
  MainWindow *mainWindow = dynamic_cast<MainWindow *>(mainWindowObject);

  QJsonArray jsonArray = jsonResponse.array();
  ui->resultsListWidget->setUpdatesEnabled(false);
  foreach (const QJsonValue &val, jsonArray) {

    QJsonObject object = val.toObject();
    QString title = object.value("title").toString();
    QString id = object.value("playlistId").toString();
    QString author = object.value("author").toString();
    QString authorId = object.value("authorId").toString();
    int videoCount = object.value("videoCount").toInt();
    QString playlistThumbnail = object.value("playlistThumbnail").toString();
    playlistThumbnail.replace("hqdefault", "mqdefault");

    QJsonArray videoArray = object.value("videos").toArray();

    QList<QStringList> videoMeta;
    foreach (const QJsonValue &videoVal, videoArray) {
      QJsonObject videoObject = videoVal.toObject();
      QString videoTitle = videoObject.value("title").toString();
      int lengthSeconds = videoObject.value("lengthSeconds").toInt();
      QString videoId = videoObject.value("videoId").toString();

      QStringList videoItemMeta;
      videoItemMeta.append(videoId);
      videoItemMeta.append(videoTitle);
      videoItemMeta.append(QString::number(lengthSeconds));

      videoMeta.append(videoItemMeta);
    }

    // init itemwidget & add to resultList
    PlayListItem *playlistItem =
        new PlayListItem(ui->resultsListWidget, n_manager);
    connect(playlistItem, &PlayListItem::selectItem, [=, this](QPoint itemPos) {
      ui->resultsListWidget->setCurrentItem(
          ui->resultsListWidget->itemAt(itemPos));
    });

    connect(playlistItem, &PlayListItem::viewPlaylist,
            [=, this](QString playlistId) { emit loadPlaylist(playlistId); });

    playlistItem->setObjectName("item_" + id);
    playlistItem->init(id, title, playlistThumbnail, author, authorId,
                       videoCount, videoMeta);
    playlistItem->adjustSize();
    QListWidgetItem *item = new QListWidgetItem(ui->resultsListWidget);

    ui->resultsListWidget->setItemWidget(item, playlistItem);
    item->setSizeHint(playlistItem->sizeHint());
    ui->resultsListWidget->addItem(item);

    if (mainWindow->quiting == true) {
      break;
    }
    QApplication::processEvents();
  }
  ui->resultsListWidget->setUpdatesEnabled(true);
}

QString PlaylistSearch::cleanUrl(QString string) {
  QString finalString;
  finalString = QString(string.replace("ktechpit.com/USS/", ""));
  finalString = finalString.replace(".php", "");
  return finalString;
}

void PlaylistSearch::on_searchButton_clicked() {
  if (!ui->searchLineEdit->text().trimmed().isEmpty()) {
    doSearch();
  }
}

void PlaylistSearch::on_searchLineEdit_textChanged(const QString &arg1) {
  ui->searchButton->setEnabled(!arg1.trimmed().isEmpty());
  if (isPlaylistUrl(arg1)) {
    ui->searchButton->setText("Process Playlist");
  } else {
    ui->searchButton->setText("Search");
  }
}

// const userRegex =
// /(?:http|https:\/\/|)www.youtube\.com\/user\/([a-zA-Z0-9_-]{1,})/i; const
// channelRegex =
// /(?:http|https:\/\/|)www.youtube\.com\/channel\/([a-zA-Z0-9_-]{1,})/i; const
// playlistRegex =
// /(?:http|https:\/\/|)www\.youtube\.com\/playlist\?list=([a-zA-Z0-9_-]{1,})/i;

QString PlaylistSearch::getPlaylistId(QString arg1) {
  QString id;
  // QRegExp
  // reg("(?:http|https:\\/\\/|)www\\.youtube\\.com\\/playlist\\?list=([a-zA-Z0-9_-]{1,})");
  // QRegExp reg("(?:http|https:\\/\\/|)list=([a-zA-Z0-9_-]{1,})");
  static const QRegularExpression reg(".*(youtu.be\\/|list=)([^#\\&\\?]*)");
  const QRegularExpressionMatch match = reg.match(arg1);
  if (match.hasMatch()) {
    qWarning() << match.capturedTexts();
    id = match.captured(2);
  }
  return id;
}

bool PlaylistSearch::isPlaylistUrl(QString arg1) {
  bool positive = false;
  // QRegExp
  // reg("(?:http|https:\\/\\/|)www\\.youtube\\.com\\/playlist\\?list=([a-zA-Z0-9_-]{1,})");
  static const QRegularExpression reg(".*(youtu.be\\/|list=)([^#\\&\\?]*)");
  const QRegularExpressionMatch match = reg.match(arg1);
  if (match.hasMatch() && match.capturedStart(0) == 0) {
    positive = true;
  }
  return positive;
}

void PlaylistSearch::on_resultsListWidget_itemDoubleClicked(
    QListWidgetItem *item) {
  QWidget *itemWidget = ui->resultsListWidget->itemWidget(item);
  PlayListItem *playlistItem = qobject_cast<PlayListItem *>(itemWidget);
  if (playlistItem != nullptr) {
    emit loadPlaylist(playlistItem->getPlaylistId());
  }
}

void PlaylistSearch::on_forceReload_clicked() {
  if (n_manager && m_currentLoadedUrl.isValid()) {
    n_manager->cache()->remove(m_currentLoadedUrl);
  }

  on_searchLineEdit_returnPressed();
}
