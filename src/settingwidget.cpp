#include "settingwidget.h"
#include "ui_settingwidget.h"

#include <QFileDialog>
#include <QMovie>

SettingWidget::SettingWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::SettingWidget)
{
    ui->setupUi(this);

    this->setMinimumWidth(548);

    init_engine();

    connect(ui->download_engine,&QPushButton::clicked,[=]()
    {
        emit engine->openSettingsAndClickDownload();
    });
    connect(ui->clearn_engine_cache,&QPushButton::clicked,[=]()
    {
        engine->clearEngineCache();
        QMovie *movie=new QMovie(":/icons/others/load.gif");
        ui->loading_movie->setMovie(movie);
        ui->loading_movie->setVisible(true);
        movie->start();
    });

    ui->concurrent_downloads->blockSignals(true);
    ui->concurrent_downloads->setRange(1,10);
    ui->concurrent_downloads->blockSignals(false);

    //load concurrent_downloads settings
    ui->concurrent_downloads->setValue(settings.value("concurrent_downloads",2).toInt());

    //load download location
    QString downloadPath = settings.value("download_path",utils::returnExactPath(
                          QStandardPaths::writableLocation(
                          QStandardPaths::DownloadLocation)+
                          QDir::separator()+QApplication::applicationName())).toString();
    ui->downloadLocation->setText(downloadPath);


    ui->commentsCheckBox->setChecked(settings.value("comments",true).toBool());
    ui->adblockCheckBox->setChecked(settings.value("adblocker",true).toBool());
    ui->trackerCheckBox->setChecked(settings.value("eventlogger",true).toBool());
    ui->keepPlayer->setChecked(settings.value("keepPlayer",true).toBool());

    QString windowTheme = settings.value("windowTheme","light").toString();
    windowTheme == "light" ? ui->lightRb->setChecked(true) : ui->darkRb->setChecked(true);

    QString playerWebsiteVariant = settings.value("website_to_load","desktop").toString();
    playerWebsiteVariant == "desktop" ? ui->desktopRb->setChecked(true) : ui->mobileRb->setChecked(true);

    refresh();
}

void SettingWidget::refresh()
{
    ui->cacheSize->setText(utils::refreshCacheSize(QStandardPaths::writableLocation(
                                                       QStandardPaths::CacheLocation)));
}

void SettingWidget::closeEvent(QCloseEvent *event)
{
    settings.setValue("settingsGeo",this->geometry());
    QWidget::closeEvent(event);
}

void SettingWidget::init_engine()
{
    engine = new Engine(this);

    connect(engine,&Engine::errorMessage,[=](QString errorMessage)
    {
         QMessageBox::critical(this,"Engine error",errorMessage);
    });

    connect(engine,&Engine::engineCacheCleared,[=]()
    {
        if(ui->loading_movie->movie()!=nullptr){
            ui->loading_movie->movie()->stop();
        }
        ui->loading_movie->setVisible(false);
    });

    connect(engine,&Engine::engineDownloadFailed,[=](QString errorMessage)
    {
        QMessageBox::critical(this,"Engine error",errorMessage);
    });

    connect(engine,&Engine::engineDownloadSucceeded,[=]()
    {
        if(ui->loading_movie->movie()!=nullptr){
            ui->loading_movie->movie()->stop();
        }
        ui->loading_movie->setVisible(false);
        ui->download_engine->setEnabled(true);
    });

    connect(engine,&Engine::engineStatus,[=](QString status)
    {
        ui->engine_status->setText(status);
    });

    connect(engine,&Engine::openSettingsAndClickDownload,[=]()
    {
           this->show();
           engine->download_engine_clicked();
           ui->download_engine->setEnabled(false);
           QMovie *movie=new QMovie(":/icons/others/load.gif");
           ui->loading_movie->setMovie(movie);
           ui->loading_movie->setVisible(true);
           movie->start();
    });
}

SettingWidget::~SettingWidget()
{
    delete ui;
}

void SettingWidget::on_deleteCache_clicked()
{
    emit clearWebengineCache();
    this->refresh();
}


void SettingWidget::on_changeLocation_clicked()
{
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::ShowDirsOnly);

    QString downloadPath = utils::returnExactPath(settings.value("download_path",
                          QStandardPaths::writableLocation(
                          QStandardPaths::DownloadLocation)+
                          QDir::separator()+QApplication::applicationName()).toString());

    QString path = QFileDialog::getExistingDirectory(this, tr("Choose Directory"),
                                                     downloadPath,
                                                     QFileDialog::ShowDirsOnly
                                                     | QFileDialog::DontUseNativeDialog);
    QFileInfo dir(path);
    if (dir.isDir() && dir.isWritable()) {
        ui->downloadLocation->setText(utils::returnExactPath(path));
        settings.setValue("download_path",ui->downloadLocation->text());
    }else{
        QMessageBox::critical(this,tr("Error"),"The selected path cannot be used,\nPlease select a Directory that is writable.");
    }
}

void SettingWidget::on_downloadLocation_textChanged(const QString &arg1)
{
    settings.setValue("download_path",arg1);
}

void SettingWidget::on_concurrent_downloads_valueChanged(int arg1)
{
    settings.setValue("concurrent_downloads",arg1);
    emit concurrentDownloadsValueChanged();
}

void SettingWidget::showInfo(QString title, QString message)
{
    QWidget *sheet = new QWidget(this);
    sheet->setWindowTitle(QApplication::applicationName()+" | "+title);

    QVBoxLayout *layout = new QVBoxLayout(sheet);
    sheet->setLayout(layout);
    QLabel *warningLabel = new QLabel(sheet);
    warningLabel->setText("<b>" + title + "</b><br><br>" + message);
    warningLabel->setWordWrap(true);
    layout->addWidget(warningLabel);
    warningLabel->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Expanding);
    sheet->adjustSize();

    sheet->setWindowFlags(Qt::Popup |Qt::FramelessWindowHint);
    sheet->setAttribute(Qt::WA_DeleteOnClose,true);
    //sheet->setFixedSize(250,sheet->sizeHint().height());
    sheet->move(this->geometry().center()-sheet->geometry().center());
    sheet->show();
}

void SettingWidget::on_concurrent_downloads_info_clicked()
{
    this->showInfo("Concurrent download warning","YouTube use to throttle download speed if multiple connections"
                                                 " are downloading\nfrom same IP address, so don't crank this"
                                                 " all the way up.");
}


void SettingWidget::on_commentsCheckBox_toggled(bool checked)
{
    settings.setValue("comments",checked);
}

void SettingWidget::on_adblockCheckBox_toggled(bool checked)
{
    settings.setValue("adblocker",checked);
    emit blockerSettingChanged(!checked);
}

void SettingWidget::on_trackerCheckBox_toggled(bool checked)
{
    settings.setValue("eventlogger",checked);
}

void SettingWidget::on_whatAreTrackerBtn_clicked()
{
    this->showInfo("What are Event Loggers?","YouTube use to track all user activity when they browse or navigate in their website.<br>"
                                                "They even send data like how much time your mouse was on a video, this includes sending<br>"
                                                "informations like what you watching where you left watching video etc.");

}

void SettingWidget::on_seeBlockedReq_clicked()
{
    emit showBlocked();
    this->close();
}

void SettingWidget::on_keepRunningHelp_clicked()
{
    this->showInfo("Keep player running?",
                   "Enabling this will keep playing loaded video in Player widget,even if you navigate to other widgets in Application.");
}

void SettingWidget::on_keepPlayer_toggled(bool checked)
{
    settings.setValue("keepPlayer",checked);
}

void SettingWidget::on_lightRb_toggled(bool checked)
{
    if(checked){
        settings.setValue("windowTheme","light");
        emit updateWindowTheme();
    }

}

void SettingWidget::on_darkRb_toggled(bool checked)
{
    if(checked){
        settings.setValue("windowTheme","dark");
        emit updateWindowTheme();
    }
}

void SettingWidget::on_mobileRb_toggled(bool checked)
{
    if(checked){
        settings.setValue("website_to_load","mobile");
        emit updatePlayerWebsite();
    }
}

void SettingWidget::on_desktopRb_toggled(bool checked)
{
    if(checked){
        settings.setValue("website_to_load","desktop");
        emit updatePlayerWebsite();
    }
}
