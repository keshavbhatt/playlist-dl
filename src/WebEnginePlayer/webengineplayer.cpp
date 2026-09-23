#include "webengineplayer.h"

#include <QWebEngineView>
#include <QWebEngineSettings>
#include <QWebEngineFullScreenRequest>
#include <QVBoxLayout>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebChannel>
#include <QWebEngineHistory>

#include "mainwindow.h"

WebEnginePlayer::WebEnginePlayer(QWidget *parent)
    : QWidget(parent)
{
    init_blocked();

    history = new QSettings(utils::returnPath("playerHistory")+"history",QSettings::NativeFormat,this);

    homePaegUrl = "https://youtube.com";

    //main window object
    QObject *mainWindowObject = utils::getMainWindow(this);
    MainWindow *mainWindow = dynamic_cast<MainWindow *>(mainWindowObject);

    //init bridge obj
    //https://stackoverflow.com/questions/61764733/capture-a-javascript-event-in-qtwebengine

    bridge = new QJsBridge();

    QObject::connect(bridge, &QJsBridge::hovered, [=, this](QVariant var)
    {
        if(var.toDouble() == 1.0){
            showLoginPrompt();
        }
    });

    //init webview
    m_view = new QWebEngineView(this);
    connect(m_view,&QWebEngineView::loadFinished,[=, this](bool loaded)
    {
        if(loaded){
            emit playerWorking(m_view->title() != "about:blank");
            settings.setValue("lastVisited",m_view->url().toString().toUtf8().toBase64());
        }
        mainWindow->stopSpinner();
        this->updateNavigationButtons(true);
    });
    connect(m_view,&QWebEngineView::loadStarted,[=, this]()
    {
        mainWindow->startSpinner();
        m_view->page()->profile()->settings()->setAttribute(QWebEngineSettings::ShowScrollBars,false);
        m_view->page()->settings()->setAttribute(QWebEngineSettings::ShowScrollBars,false);
        this->updateNavigationButtons(false);
    });

    connect(m_view,&QWebEngineView::loadProgress,[=, this](int progress)
    {

        toolbarWidget->setEnableCloseButton(m_view->title() != "about:blank");

        if(progress>40){
            mainWindow->stopSpinner();
        }
        if(m_view->url().toString().contains("youtube")
                && (settings.value("player_firstrun",true).toBool() && progress>70) )
        {
            m_view->page()->runJavaScript("document.cookie='PREF=f6=400&f5=30000;path=/;domain=.youtube.com';",QWebEngineScript::MainWorld);
            m_view->reload();
            settings.setValue("player_firstrun",false);
        }
        if(progress > 90){
            this->updateNavigationButtons(false);
            if(settings.value("website_to_load","desktop") != "mobile")
            {
                m_view->page()->profile()->settings()->setAttribute(QWebEngineSettings::ShowScrollBars,true);
                m_view->page()->settings()->setAttribute(QWebEngineSettings::ShowScrollBars,true);
            }
        }
        if(progress == 100){
            this->updateNavigationButtons(true);
        }
    });

    //init webpage
    QWebEngineProfile *profile = QWebEngineProfile::defaultProfile();

    //profile->setHttpUserAgent("Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:72.0) Gecko/20100101 Firefox/72.0");

    profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);

    QWebEnginePage *myPage = new QWebEnginePage(profile, m_view);

    //update the useragent according to the settings in settings dialog
    updatePlayerWebsite(false);

    myPage->settings()->setAttribute(QWebEngineSettings::FullScreenSupportEnabled, true);

    m_view->setPage(myPage);

    //init webChannel, JS helper is installed via webpage
    QWebChannel *channel = new QWebChannel(m_view->page());
    m_view->page()->setWebChannel(channel);
    channel->registerObject(QStringLiteral("qt_helper"), bridge);

    //init requestInterceptor to page profile
    RequestInterceptor *interceptor = new RequestInterceptor(profile);
    profile->setUrlRequestInterceptor(interceptor);
    connect(interceptor,&RequestInterceptor::blocked,blockedWidget,&Blocked::appendLog);


    //page stuff
    connect(m_view->page(),
            &QWebEnginePage::fullScreenRequested,
            this,
            &WebEnginePlayer::fullScreenRequested);

    //init scroll stylesheet
    insertStyleSheet("scroll",getSourceCode(QStringLiteral(":/css/scroll.css")),false,false);

    //init core stylesheet
    insertStyleSheet("core",getSourceCode(QStringLiteral(":/css/core.css")),false);

    //init adblock
    insertJavascript("skipper",getSourceCode(QStringLiteral(":/js/skip.js")),false);

    //init theatre mode
    QString theatremode = "window.addEventListener('yt-navigate-finish', function(event) {"
            "var newPlayer = document.querySelector('button.ytp-size-button');"
            "if ( newPlayer && null === document.getElementById('player-theater-container').firstChild ) {"
                "newPlayer.click();"
        "}});";

    insertJavascript("theatre",theatremode,false,false);
    insertJavascript("theatreCookie","document.cookie='wide=1;path=/;domain=.youtube.com';",false,false);

    //init dark mode
    insertJavascript("darkMode","document.cookie='PREF=f6=400&f5=30000;path=/;domain=.youtube.com';",false,false);

    createToolBar();

    //add to layout
    QHBoxLayout *mainHLayout = new QHBoxLayout;
    mainHLayout->setContentsMargins(0,0,0,0);

    QVBoxLayout *toolbarVLayout = new QVBoxLayout;
    toolbarVLayout->addWidget(toolbarWidget);

    QVBoxLayout *webviewLayout = new QVBoxLayout;
    webviewLayout->addWidget(m_view);
    webviewLayout->setContentsMargins(0,0,0,0);

    mainHLayout->addLayout(toolbarVLayout);
    mainHLayout->addLayout(webviewLayout);

    this->setLayout(mainHLayout);

    this->loadBlankHome();
}


void WebEnginePlayer::blockerSettingChanged(bool blockerDisabled)
{
    QWebEngineScriptCollection &scripts = m_view->page()->scripts();
    const QList<QWebEngineScript> skipper = scripts.find(QStringLiteral("skipper"));
    const QList<QWebEngineScript> core    = scripts.find(QStringLiteral("core"));

    if(blockerDisabled)
    {
        if(!skipper.isEmpty() || !core.isEmpty())
        {
            for(const QWebEngineScript &script : skipper) scripts.remove(script);
            for(const QWebEngineScript &script : core) scripts.remove(script);
            this->reload(true);
        }
    }else{
        if(skipper.isEmpty() || core.isEmpty())
        {
            insertJavascript("skipper",getSourceCode(QStringLiteral(":/js/skip.js")),false);
            insertStyleSheet("core",getSourceCode(QStringLiteral(":/css/core.css")),false);
            this->reload(true);
        }
    }
}

void WebEnginePlayer::reload( const bool ask)
{
    if(ask){
        QMessageBox::StandardButton btn =  QMessageBox::information(this,"Reload Page","This action requires page reload. Reload Page?",QMessageBox::No,QMessageBox::Yes);
        if(btn == QMessageBox::Yes)
             m_view->reload();
    }else{
         m_view->reload();
    }
}

WebEnginePlayer::~WebEnginePlayer()
{
    QByteArray ba;
    QDataStream ds(&ba, QIODevice::WriteOnly);
    ds << *(m_view->page()->history());
    history->setValue("history", ba);
}

void WebEnginePlayer::init_blocked()
{
    //init error
    blockedWidget = new Blocked(this);
    blockedWidget->setWindowTitle(QApplication::applicationName()+" | Blocked requests");
    blockedWidget->setWindowFlag(Qt::Dialog);
    blockedWidget->setWindowModality(Qt::NonModal);
    connect(blockedWidget,&Blocked::closed,[=, this](){
       emit blockedClosed();
    });
}

void WebEnginePlayer::showBlocked()
{
    if(blockedWidget->isVisible()==false)
    {
        blockedWidget->showNormal();
    }else{
        blockedWidget->raise();
    }
}

void WebEnginePlayer::updateNavigationButtons(const bool enableReload)
{
   auto history = m_view->history();

   bool enableBack    = history->canGoBack();
   bool enableForward = history->canGoForward();

   toolbarWidget->updateNavigation(enableForward,enableBack,enableReload);
}

void WebEnginePlayer::createToolBar()
{
    toolbarWidget = new ToolBar(this);

    //we have history settings file ?
    QFileInfo historyFileInfo(history->fileName());
    toolbarWidget->setEnableLoadPreviousSessionButton(
                (historyFileInfo.exists() && historyFileInfo.size() > 1000));

    connect(toolbarWidget,&ToolBar::loadBlankPage,this,&WebEnginePlayer::loadBlankHome);
    connect(toolbarWidget,&ToolBar::navigateWebviewBackward,m_view,&QWebEngineView::back);
    connect(toolbarWidget,&ToolBar::navigateWebviewFoward,m_view,&QWebEngineView::forward);
    connect(toolbarWidget,&ToolBar::reload,m_view,&QWebEngineView::reload);
    connect(toolbarWidget,&ToolBar::stop,m_view,&QWebEngineView::stop);
    connect(toolbarWidget,&ToolBar::goHome,[=, this](){
        m_view->page()->load(QUrl(homePaegUrl));
    });

    connect(toolbarWidget,&ToolBar::history,[=, this](){

        //load history of page
        QByteArray ba = history->value("history").toByteArray();
        QDataStream ds(&ba, QIODevice::ReadOnly);
        ds >> *(m_view->page()->history());
        updateNavigationButtons(true);
    });

    toolbarWidget->setMaximumWidth(toolbarWidget->minimumSizeHint().width());
    toolbarWidget->setAttribute(Qt::WA_StyledBackground, true);
}

void WebEnginePlayer::insertJavascript(const QString &name, const QString &source, bool immediately, bool runinframes)
{
    QWebEngineScript script;
    if (immediately)
        m_view->page()->runJavaScript(source, QWebEngineScript::ApplicationWorld);
    script.setName(name);
    script.setSourceCode(source);
    script.setInjectionPoint(QWebEngineScript::Deferred);
    script.setRunsOnSubFrames(runinframes);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    m_view->page()->scripts().insert(script);
}


void WebEnginePlayer::insertStyleSheet(const QString &name, const QString &source, bool immediately, bool runinframes)
{
    QWebEngineScript script;
    QString s = QString::fromLatin1("(function() {"\
                                    "    var css = document.createElement('style');"\
                                    "    css.type = 'text/css';"\
                                    "    css.id = '%1';"\
                                    "    document.head.appendChild(css);"\
                                    "    css.innerText = '%2';"\
                                    "})();").arg(name).arg(source.simplified());
    if (immediately)
        m_view->page()->runJavaScript(s, QWebEngineScript::ApplicationWorld);

    script.setName(name);
    script.setSourceCode(s);
    script.setInjectionPoint(QWebEngineScript::Deferred);
    script.setRunsOnSubFrames(runinframes);
    script.setWorldId(QWebEngineScript::ApplicationWorld);
    m_view->page()->scripts().insert(script);
}

void WebEnginePlayer::showLoginPrompt()
{
    const QString restrictedUrlStr = m_view->url().toString();
    QString prefix;
    if(restrictedUrlStr.contains("?v="))
        prefix = "watch?v=" + restrictedUrlStr.split("?v=").last();
    else if(restrictedUrlStr.contains("?p="))
        prefix = "playlist?list=" + restrictedUrlStr.split("?p=").last();
    else if (restrictedUrlStr.contains("?u="))
        prefix = "c/" + restrictedUrlStr.split("?u=").last();
    qDebug()<<prefix;

    QMessageBox msgBox;
    msgBox.setWindowTitle(QApplication::applicationName()+" | "+tr("Age restricted prompt"));
    msgBox.setInformativeText("Age verification is required to play Age restrited content,\n"
                              "Do you want to play video on YouTube?");
    QPushButton *connectButton = msgBox.addButton(tr("Play on YouTube"), QMessageBox::ActionRole);
    QPushButton *abortButton = msgBox.addButton(QMessageBox::Cancel);
    msgBox.setDefaultButton(abortButton);
    msgBox.exec();

    if (msgBox.clickedButton() == connectButton) {
        m_view->page()->load(QUrl(QString("http://www.youtube.com/%1").arg(prefix)));
        //m_view->page()->load(QUrl("https://youtube.com/account"));
    } else if (msgBox.clickedButton() == abortButton) {
        msgBox.close();
    }
}


void WebEnginePlayer::clearCache()
{
    m_view->page()->profile()->clearHttpCache();
}


void WebEnginePlayer::play(QString vId)
{
    if(vId.trimmed().isEmpty() == false){
        QString urlStr = "https://keshavbhatt.github.io/YtTest/?v=%1";
        m_view->page()->load(QUrl(urlStr.arg(vId)));
    }
}

void WebEnginePlayer::playPlaylist(QString pId)
{
    if(pId.trimmed().isEmpty() == false){
        QString urlStr = "https://keshavbhatt.github.io/YtTest/?p=%1";
        m_view->page()->load(QUrl(urlStr.arg(pId)));
    }
}

void WebEnginePlayer::playAuthorUploads(QString aId)
{
    if(aId.trimmed().isEmpty() == false){
        QString urlStr = "https://keshavbhatt.github.io/YtTest/?u=%1";
       m_view->page()->load(QUrl(urlStr.arg(aId)));
    }
}

void WebEnginePlayer::loadBlankHome()
{
    m_view->page()->setHtml("<html><head><title>about:blank</title><style type='text/css'>html{overflow: hidden;}"
                    "body{color: silver;font-family: -apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,Oxygen-Sans,Ubuntu,Cantarell,'Helvetica Neue',sans-serif; height: 100%;width: 100%; background-color: black;}"
                    "</style></head><body>"
                    "<div style='height: 100%;display: flex;align-items: center;justify-content: center'>"
                    "<h3 style='margin:0;'>Press Home button to go to YouTube.</h3>"
                    "</div></body></html>");
    toolbarWidget->setEnableCloseButton(false);
    emit playerWorking(false);
}

QString WebEnginePlayer::getTitle()
{
    return m_view->title();
}

void WebEnginePlayer::updatePlayerWebsite(bool askToReload)
{
    if(settings.value("website_to_load","desktop").toString() == "mobile"){
        m_view->page()->profile()->setHttpUserAgent("Mozilla/5.0 (Linux; Android 6.0.1; Moto G (4)) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/90.0.4430.85 Mobile Safari/537.36");
        reload(askToReload);
    }else{
        m_view->page()->profile()->setHttpUserAgent("Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:72.0) Gecko/20100101 Firefox/72.0");
        reload(askToReload);
    }
}

void WebEnginePlayer::reset()
{
    if(settings.value("keepPlayer",true).toBool() == true){
        emit playerWorking(m_view->title() != "about:blank");
        return;
    }
    if(m_view->title() != "about:blank")
    {
        loadBlankHome();
    }
}

void WebEnginePlayer::fullScreenRequested(QWebEngineFullScreenRequest request)
{
    if (request.toggleOn()) {
        if (m_fullScreenWindow)
            return;
        request.accept();
        m_fullScreenWindow.reset(new FullScreenWindow(m_view));
    } else {
        if (!m_fullScreenWindow)
            return;
        request.accept();
        m_fullScreenWindow.reset();
    }
}
