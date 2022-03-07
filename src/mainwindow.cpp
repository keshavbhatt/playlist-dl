#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "rateapp.h"


#include <QStyleFactory>


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    setWindowTitle(QApplication::applicationName());
    setWindowIcon(QIcon(":/icons/app/icon-128.png"));
    setMinimumWidth(600);
    setMinimumHeight(400);

    lightPalette = qApp->palette();

    spinner = new Spinner();

    ui->centralWidget->layout()->setContentsMargins(9,9,9,9);

    n_manager = new QNetworkAccessManager(this);

    QNetworkDiskCache *diskCache = new QNetworkDiskCache(this);
    diskCache->setCacheDirectory(QStandardPaths::writableLocation(
                                     QStandardPaths::CacheLocation));
    n_manager->setCache(diskCache);

    QMargins m = ui->mainToolBar->layout()->contentsMargins();
    ui->mainToolBar->setContentsMargins(m.left(),m.top(),m.right(),m.top());

    restoreGeometry(settings.value("windowGeometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());

    //init webenginePlayerWidget
    webenginePlayerWidget = new WebEnginePlayer(this);
    connect(webenginePlayerWidget,&WebEnginePlayer::playerWorking,[=](bool working){
        if(working){
            playerAction->setIcon(QIcon(":/icons/others/primo/red/button_blue_play.png"));
            playerAction->setToolTip(webenginePlayerWidget->getTitle());
        }
        else{
            playerAction->setIcon(QIcon(":/icons/others/primo/button_blue_play.png"));
            playerAction->setToolTip("Player");
        }
    });
    connect(webenginePlayerWidget,&WebEnginePlayer::blockedClosed,[=](){
        if(settingWidget){
            settingWidget->showNormal();
        }
    });

    //init playlistSearchWidget
    playlistSearchWidget = new PlaylistSearch(this,this->n_manager);
    playlistSearchWidget->layout()->setContentsMargins(0,0,0,0);
    connect(playlistSearchWidget,&PlaylistSearch::loadPlaylist,[=](QString playListId){
       startSpinner();
       playlistViewWidget->loadPlaylist(playListId);
       this->switchStackWidget(playlistViewWidget);
       stopSpinner();
    });

    //init playlisViewWidget
    playlistViewWidget = new PlaylistView(this);
    playlistViewWidget->layout()->setContentsMargins(0,0,0,0);
    connect(playlistViewWidget,&PlaylistView::downloadSelected,[=](const QString playlistId,const QStringList itemIdList){
        startSpinner();
        playlistDownloadOptionsWidget->loadSelected(playlistId,itemIdList);
        this->switchStackWidget(playlistDownloadOptionsWidget);
        stopSpinner();
    });

    connect(playlistViewWidget,&PlaylistView::playPlaylist,[=](const QString playlistId){
        playPlaylist(playlistId);
    });

    connect(playlistViewWidget,&PlaylistView::playAuthorUploads,[=](const QString authorId){
        playAuthorUploads(authorId);
    });

    //init playlisViewWidget
    playlistDownloadOptionsWidget = new PlaylistDownloadOptions(this,this->n_manager,nullptr);
    playlistDownloadOptionsWidget->layout()->setContentsMargins(0,0,0,0);
    connect(playlistDownloadOptionsWidget,&PlaylistDownloadOptions::addToDownload,[=](QString download_record_filename)
    {
       //add after a delay so the animation could be seen by user
       QTimer *timer = new QTimer(this);
       timer->setSingleShot(true);
       connect(timer,&QTimer::timeout,[=](){
           downloadWidget->addToDownload(download_record_filename,true);
           this->stopSpinner();
           timer->deleteLater();
       });
       this->switchStackWidget(downloadWidget);
       this->startSpinner();
       timer->start(500);
    });

    //init downloadWidget
    downloadWidget = new DownloadWidget(this,this->n_manager);
    downloadWidget->layout()->setContentsMargins(0,0,0,0);

    createActions();

    //init stackWidget
    ui->stackedWidget->setAnimation(QEasingCurve::Type::OutQuart);
    ui->stackedWidget->setSpeed(650);
    connect(ui->stackedWidget,&QStackedWidget::currentChanged,[=](int arg1)
    {
        Q_UNUSED(arg1);
        if(stackVector.isEmpty() && ui->stackedWidget->currentWidget() != playlistSearchWidget){
            qDebug()<<"added"<<playlistSearchWidget->objectName()<<"to stackVector since"
                                                                   " it was empty and home"
                                                                   " was not loaded";
            stackVector.append(playlistSearchWidget);
        }
        searchAction->setEnabled(stackVector.isEmpty() == false);

        if( webenginePlayerWidget && ui->stackedWidget->currentWidget() != webenginePlayerWidget){

            webenginePlayerWidget->reset();

        }
        if(ui->stackedWidget->currentWidget() == downloadWidget){
            downloadWidget->home();
        }
        homeAction->setEnabled(ui->stackedWidget->currentWidget()!=playlistSearchWidget);
        downloadsAction->setEnabled(ui->stackedWidget->currentWidget()!=downloadWidget);
        playerAction->setEnabled(ui->stackedWidget->currentWidget()!=webenginePlayerWidget);

    });

    //add to stackWidget
    ui->stackedWidget->addWidget(playlistSearchWidget);
    ui->stackedWidget->addWidget(playlistViewWidget);
    ui->stackedWidget->addWidget(playlistDownloadOptionsWidget);
    ui->stackedWidget->addWidget(downloadWidget);
    ui->stackedWidget->addWidget(webenginePlayerWidget);

    qApp->installEventFilter(this);

    init_settings();

    RateApp *rateApp = new RateApp(this, "snap://playlist-dl", 5, 5, 1000 * 30);
    rateApp->setWindowTitle(QApplication::applicationName()+" | "+tr("Rate Application"));
    rateApp->setVisible(false);
    rateApp->setWindowFlags(Qt::Dialog);
    rateApp->setAttribute(Qt::WA_DeleteOnClose,true);
    QPoint centerPos = this->geometry().center()-rateApp->geometry().center();
    connect(rateApp,&RateApp::showRateDialog,[=]()
    {
        if(this->windowState() != Qt::WindowMinimized && this->isVisible() && isActiveWindow()){
            rateApp->move(centerPos);
            rateApp->show();
        }else{
            rateApp->delayShowEvent();
        }
    });

    //TEST
    //switchStackWidget(webenginePlayerWidget);
    //playVideo("_Yhyp-_hX2s");


    init_account();

    updateWindowTheme();

}

void MainWindow::switchStackWidget(QWidget *widget,bool addToStackVector)
{
    if(addToStackVector)
    {
        //prevent adding duplicate widget in stackVector
        if(stackVector.isEmpty() == false && ui->stackedWidget->currentWidget() != widget)
            stackVector.append(ui->stackedWidget->currentWidget());
        else if(stackVector.isEmpty()){
            stackVector.append(ui->stackedWidget->currentWidget());
        }
    }
    ui->stackedWidget->slideInWgt(widget);
}

void MainWindow::playVideo(QString videoId)
{
    webenginePlayerWidget->play(videoId);
    switchStackWidget(webenginePlayerWidget);
}

void MainWindow::playPlaylist(QString plsylistId)
{
    webenginePlayerWidget->playPlaylist(plsylistId);
    switchStackWidget(webenginePlayerWidget);
}

void MainWindow::playAuthorUploads(QString authorId)
{
    webenginePlayerWidget->playAuthorUploads(authorId);
    switchStackWidget(webenginePlayerWidget);
}


bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if(event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Escape)
        {
         qDebug()<<"Cancel request";
         if(playlistSearchWidget && ui->stackedWidget->currentWidget() == playlistSearchWidget)
            playlistSearchWidget->cancelAllRequests();
        }
    }
    return QMainWindow::eventFilter(obj,event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    quiting = true;

    cancelAllRequests();

    settings.setValue("windowGeometry", saveGeometry());
    settings.setValue("windowState", saveState());

    QMainWindow::closeEvent(event);
}

void MainWindow::cancelAllRequests()
{
    foreach (auto &reply, n_manager->findChildren<QNetworkReply *>()) {
        if (! reply->isReadable()) {
                return;
        }
        reply->abort();
        reply->deleteLater();
    }
    n_manager->disconnect();
    n_manager->deleteLater();
}

void MainWindow::createActions()
{

    ui->mainToolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->mainToolBar->setMovable(false);
    ui->mainToolBar->setFloatable(false);

    //create
    homeAction      = new QAction(QIcon(":/icons/others/primo/home.png"), tr("&Home"), this);
    searchAction    = new QAction(QIcon(":/icons/others/primo/prev.png"),tr("&Back"), this);
    searchAction->setEnabled(false);
    downloadsAction = new QAction(QIcon(":/icons/others/primo/inbox.png"),tr("&Downloads"), this);
    settingAction   = new QAction(QIcon(":/icons/others/primo/gear.png"),tr("&Settings"), this);
    aboutAction     = new QAction(QIcon(":/icons/others/primo/info_blue.png"),tr("&About"), this);
    playerAction    = new QAction(QIcon(":/icons/others/primo/button_blue_play.png"),tr("&Player"), this);
    accountAction   = new QAction(QIcon(":/icons/others/primo/lock.png"),tr("&Account"), this);

    //connect
    connect(homeAction,&QAction::triggered,[=](){
       stackVector.clear();
       switchStackWidget(playlistSearchWidget,false);
    });

    connect(searchAction,&QAction::triggered,[=]()
    {
        if(stackVector.isEmpty())
            return;
        switchStackWidget(stackVector.takeLast(),false);
    });

    connect(downloadsAction,&QAction::triggered,[=]()
    {
        switchStackWidget(downloadWidget);
    });

    connect(playerAction,&QAction::triggered,[=]()
    {
        switchStackWidget(webenginePlayerWidget);
    });

    connect(accountAction,&QAction::triggered,[=]()
    {
         accountWidget->adjustSize();
         accountWidget->show();
    });

    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);
    connect(settingAction, &QAction::triggered, this, &MainWindow::showSettings);

    //add
    ui->mainToolBar->addAction(searchAction);
    ui->mainToolBar->addSeparator();
    ui->mainToolBar->addAction(homeAction);

    QWidget* hSpacer = new QWidget(this);
    hSpacer->setStyleSheet("background-color:transparent");
    hSpacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    ui->mainToolBar->addWidget(hSpacer);

    spinner->setSpinnerSize(QSize(ui->mainToolBar->sizeHint().height(),
                                  ui->mainToolBar->sizeHint().height()));
    spinner->setFixedSize(spinner->minimumSizeHint());
    ui->mainToolBar->addWidget(spinner);

    ui->mainToolBar->addAction(downloadsAction);
    ui->mainToolBar->addAction(playerAction);
    ui->mainToolBar->addSeparator();
    ui->mainToolBar->addAction(settingAction);
    ui->mainToolBar->addAction(accountAction);
    ui->mainToolBar->addAction(aboutAction);
}

void MainWindow::startSpinner()
{
    spinner->start();
}

void MainWindow::stopSpinner()
{
    spinner->stop();
}

void MainWindow::init_settings()
{
    settingWidget = new SettingWidget(this);
    settingWidget->setWindowTitle(QApplication::applicationName()+" | "+tr("Settings"));
    settingWidget->setWindowFlag(Qt::Dialog);
    settingWidget->adjustSize();
    settingWidget->restoreGeometry(settings.value("settingsGeo").toByteArray());

    connect(settingWidget,SIGNAL(updateWindowTheme()),this,SLOT(updateWindowTheme()));
    connect(settingWidget,&SettingWidget::clearWebengineCache,[=](){
        if(webenginePlayerWidget)
            webenginePlayerWidget->clearCache();
    });

    connect(settingWidget,&SettingWidget::updatePlayerWebsite,[=](){
        if(webenginePlayerWidget)
            webenginePlayerWidget->updatePlayerWebsite();
    });



    connect(settingWidget,&SettingWidget::blockerSettingChanged,[=](const bool blockerDisabled){
       webenginePlayerWidget->blockerSettingChanged (blockerDisabled);
    });

    connect(settingWidget,&SettingWidget::showBlocked,webenginePlayerWidget,&WebEnginePlayer::showBlocked);

}

void MainWindow::updateWindowTheme()
{
    //fix the toolbar borders according to the theme
    QString toolbarStyle = "QToolBar{border-top: 0px solid silver;"
                           "border-bottom: 1px solid rgb(%1)}";
    QString styleColor = settings.value("windowTheme","light").toString() == "dark"
                            ? "80, 80, 80":"195,195,195";
    ui->mainToolBar->setStyleSheet(toolbarStyle.arg(styleColor));

    foreach (QListWidget *listWidget, this->findChildren<QListWidget*>()) {
        disconnect(listWidget, &QListWidget::currentItemChanged, this, nullptr);
        connect(listWidget,&QListWidget::currentItemChanged,[=](QListWidgetItem *current, QListWidgetItem *previous)
        {
            if(current != nullptr){
                auto currentItemWidget = current->listWidget()->itemWidget(current);
                if(currentItemWidget != nullptr)
                    currentItemWidget->setBackgroundRole(QPalette::Highlight);
            }

            if(previous != nullptr){
                auto previousItemWidget = previous->listWidget()->itemWidget(previous);
                if(previousItemWidget != nullptr)
                    previousItemWidget->setBackgroundRole(QPalette::Window);
            }
        });
    }

    if(settings.value("windowTheme","light").toString() == "dark")
    {
        qApp->setStyle(QStyleFactory::create("fusion"));
        QPalette palette;
        palette.setColor(QPalette::Window, QColor("#262D31"));
        palette.setColor(QPalette::Text, Qt::white);
        palette.setColor(QPalette::WindowText, Qt::white);
        palette.setColor(QPalette::Base, QColor("#323739"));
        palette.setColor(QPalette::AlternateBase, QColor("#5f6c73"));
        palette.setColor(QPalette::ToolTipBase, QColor(66, 66, 66));
        palette.setColor(QPalette::Disabled, QPalette::Window,QColor("#3f4143"));
        palette.setColor(QPalette::ToolTipText, QColor("silver"));
        palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
        palette.setColor(QPalette::Dark, QColor(35, 35, 35));
        palette.setColor(QPalette::Shadow, QColor(20, 20, 20));
        palette.setColor(QPalette::Button, QColor("#262D31"));
        palette.setColor(QPalette::ButtonText, Qt::white);
        palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
        palette.setColor(QPalette::BrightText, Qt::red);
        palette.setColor(QPalette::Link, QColor(42, 130, 218));
        palette.setColor(QPalette::Highlight, QColor(38, 140, 196));
        palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::Disabled, QPalette::HighlightedText,QColor(127, 127, 127));
        qApp->setPalette(palette);

    }else{
        qApp->setPalette(lightPalette);
    }

    QList<QWidget*> widgets = this->findChildren<QWidget*>();

    foreach (QWidget* w, widgets)
    {
        w->setPalette(qApp->palette());
    }
    this->update();
}

void MainWindow::showSettings()
{
 if(!settingWidget->isVisible())
    {
        settingWidget->setMinimumWidth(470);
        settingWidget->refresh();
        settingWidget->showNormal();
    }
}

void MainWindow::showAbout()
{
    About *about = new About(this);
    about->setWindowFlags(about->windowFlags()| Qt::Dialog);
    about->setMinimumSize(about->sizeHint());
    about->adjustSize();
    about->setAttribute(Qt::WA_DeleteOnClose);
    about->show();
}


void MainWindow::init_account()
{
    //work around to fix recieve created_signal from account class
    QPushButton *pb = new QPushButton("pop",this);
    pb->setObjectName("push");
    pb->hide();
    connect(pb,&QPushButton::clicked,[=](){
       qDebug()<<"PUSH BUTTON clicked";
       QTimer::singleShot(2000,[=](){
           if(accountWidget!=nullptr){
               if(!accountWidget->isVisible()){
                   accountWidget->adjustSize();
                   accountWidget->show();
               }
           }
       });
    });
    if(accountWidget==nullptr)
    {
        accountWidget = new account(this);
        accountWidget->setObjectName("accountWidget");
        accountWidget->setWindowFlags(Qt::Dialog);
        accountWidget->adjustSize();
        connect(accountWidget,&account::showAccountWidget,[=](){
            accountWidget->adjustSize();
            accountWidget->show();
            accountWidget->flashPurchaseButton();
        });
        //signal based check to enable pro on live check
        connect(accountWidget,&account::disablePro,[=](){
           disablePro();
        });
        connect(accountWidget,&account::enablePro,[=](){
           enablePro();
        });
        //manual check to enable pro if not conencted
        accountWidget->check_pro();
        if(accountWidget->pro == true){
            enablePro();
        }else{
            disablePro();
        }
        accountWidget->setFixedSize(accountWidget->sizeHint());
    }

    playlistDownloadOptionsWidget->setAccountManager(this->accountWidget);
}

void MainWindow::enablePro()
{
    accountAction->setIcon(QIcon(":/icons/others/primo/lock.png"));
    accountAction->setText("Account");
}

void MainWindow::disablePro()
{
    accountAction->setIcon(QIcon(":/icons/others/primo/unlock.png"));
    accountAction->setText("Unclock");
}


MainWindow::~MainWindow()
{
    delete ui;
}

