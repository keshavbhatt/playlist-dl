#include "playlistitem.h"
#include "ui_playlistitem.h"

#include <QGraphicsOpacityEffect>
#include <QMenu>
#include <QPropertyAnimation>
#include <playlistsearch.h>

PlayListItem::PlayListItem(QWidget *parent, QNetworkAccessManager *manager) :
    QWidget(parent),
    ui(new Ui::PlayListItem)
{
    ui->setupUi(this);

    this->layout()->setContentsMargins(0,0,0,0);

    this->networkManager_ = manager;

    animate();

}


void PlayListItem::animate()
{
    QGraphicsOpacityEffect *eff = new QGraphicsOpacityEffect(this);
    this->setGraphicsEffect(eff);
    QPropertyAnimation *a = new QPropertyAnimation(eff,"opacity");
    a->setDuration(600);
    a->setStartValue(0.0);
    a->setEndValue(1.0);
    a->setEasingCurve(QEasingCurve::Linear);
    a->start(QPropertyAnimation::DeleteWhenStopped);
    connect(a,&QPropertyAnimation::finished,[this,eff](){
       eff->deleteLater();
    });
}

void PlayListItem::init(QString id,QString title,QString playlistThumbnail,QString author,QString authorId,
                        int videoCount,QList<QStringList>videoMeta)
{
    Q_UNUSED(authorId);
    playlistId = id;

    if(playlistThumbnail.isEmpty()){
        playlistThumbnail = "https://i.ytimg.com/vi/"+id+"/mqdefault.jpg";
    }
    ui->thumbnail->setPixmap(QPixmap(":/icons/others/wall_placeholder_180.jpg").scaled(ui->thumbnail->size(),Qt::KeepAspectRatio,Qt::SmoothTransformation));
    ui->thumbnail->init(this->networkManager_,playlistThumbnail,":/icons/others/wall_placeholder_180.jpg");
    ui->title->setText(title);
    ui->by->setText(author);
    ui->videoCount->setText(QString::number(videoCount)+QString( videoCount == 1 ? tr(" video"):tr(" videos")));
    QString videoMetaStr;
    foreach (QStringList videoItemMeta, videoMeta) {
        QString durationStr = utils::formatSeconds(videoItemMeta.at(2).toInt());
        videoMetaStr.append("- "+videoItemMeta.at(1)+" ("+durationStr+")\n");
    }
    ui->videos->setText(videoMetaStr);
}

PlayListItem::~PlayListItem()
{
    delete ui;
}

QString PlayListItem::getPlaylistId()
{
    return this->playlistId;
}

void PlayListItem::on_menuButton_clicked()
{
    emit selectItem(this->pos());

    QMenu menu;
    QAction *viewPlaylistAction = new QAction(QIcon(":/icons/folder-open-line.png"), tr("View Playlist"), &menu);
    QAction *bookmarkPlaylistAction = new QAction(QIcon(":/icons/bookmark-3-line.png"), tr("Bookmark Playlist"), &menu);

    connect(viewPlaylistAction,&QAction::triggered,[=](){
        emit viewPlaylist(this->playlistId);
    });

    menu.addAction(viewPlaylistAction);
    menu.addAction(bookmarkPlaylistAction);

    menu.exec(ui->menuButton->mapToGlobal(QPoint(0,ui->menuButton->width())));

}
