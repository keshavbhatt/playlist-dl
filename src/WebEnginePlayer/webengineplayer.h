#ifndef WEBENGINEPLAYER_H
#define WEBENGINEPLAYER_H

#include "fullscreenwindow.h"

#include <QMainWindow>
#include <QWebEngineView>
#include <QWebEngineFullScreenRequest>
#include <QFile>
#include <QWebEngineUrlRequestInterceptor>
#include <QSettings>
#include "toolbar.h"
#include "blocked/blocked.h"

class QJsBridge: public QObject{
    Q_OBJECT
public:
    using QObject::QObject;
Q_INVOKABLE void log(const QString& str) const
   {
       qDebug() << "LOG from JS: " << str;
   }
Q_SIGNALS:
    void hovered(QVariant var);
public Q_SLOTS:
    void onHovered(QVariant var){
        Q_EMIT hovered(var);
    }
};


class WebEnginePlayer : public QWidget
{
    Q_OBJECT

public:
    explicit WebEnginePlayer(QWidget *parent = nullptr);
    ~WebEnginePlayer();

signals:
    void blockedClosed();
    void playerWorking(bool working);

public slots:
    void play(QString vId);
    void reset();
    void playPlaylist(QString pId);
    void playAuthorUploads(QString aId);
    void clearCache();
    void showBlocked();
    void blockerSettingChanged(bool blockerDisabled);
    void loadBlankHome();
    QString getTitle();
    void updatePlayerWebsite(bool askToReload = true);

private slots:
    void fullScreenRequested(QWebEngineFullScreenRequest request);
    void insertJavascript(const QString &name, const QString &source, bool immediately, bool runinframes = false);
    void insertStyleSheet(const QString &name, const QString &source, bool immediately, bool runinframes = true);
    void showLoginPrompt();
    void createToolBar();
    void updateNavigationButtons(const bool enableReload);
    void init_blocked();
    void reload(const bool ask = false);
private:
    QWebEngineView *m_view = nullptr;
    QScopedPointer<FullScreenWindow> m_fullScreenWindow;
    QSettings settings, *history;

    QJsBridge *bridge = nullptr;
    ToolBar *toolbarWidget = nullptr;
    QString homePaegUrl;
    Blocked* blockedWidget = nullptr;

    static QString getSourceCode(const QString & filename){
        QFile file(filename);
        if(!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    }

};






class RequestInterceptor : public QWebEngineUrlRequestInterceptor
{
    Q_OBJECT

signals:
    void blocked(QString adUrl);
    void adblockerDisabled(bool disabled);

public:
    QSettings settings;
    RequestInterceptor(QObject *parent = nullptr) : QWebEngineUrlRequestInterceptor(parent)
    {
        adFile.setFileName(":/js/ads.light");

        if (!adFile.open(QIODevice::ReadOnly | QIODevice::Text))
                  return;

          while (!adFile.atEnd()) {
              QByteArray line = adFile.readLine();
              adsUrl.append(line.trimmed());
              adsUrlStore.append(adsUrl);
          }
    }

    void interceptRequest(QWebEngineUrlRequestInfo &info)
    {

        QString reqUrlStr = info.requestUrl().toString();


        //enable adblocker if settings set true
        if(settings.value("adblocker",true).toBool() == false)
        {
            adsUrl.clear();
        }else{
            adsUrl.append(adsUrlStore);
        }

        //add log_event to blacklist urls
        if(settings.value("eventlogger",true).toBool())
        {
            if(!adsUrl.contains("/log_event",Qt::CaseInsensitive))
                adsUrl.append("/log_event");

            if(!adsUrl.contains("/ptracking",Qt::CaseInsensitive))
                adsUrl.append("/ptracking");

            if(!adsUrl.contains("/api/stats/",Qt::CaseInsensitive))
                adsUrl.append("/api/stats/");

            if(!adsUrl.contains("/stats",Qt::CaseInsensitive))
                adsUrl.append("/stats");

        }else{

            int i1 = adsUrl.lastIndexOf("/log_event");
            if(i1 != -1)
                adsUrl.removeAt(i1);

            int i2 = adsUrl.lastIndexOf("/ptracking");
            if(i2 != -1)
                adsUrl.removeAt(i2);

            int i3 = adsUrl.lastIndexOf("/api/stats/");
            if(i3 != -1)
                adsUrl.removeAt(i3);

            int i4 = adsUrl.lastIndexOf("/stats");
            if(i4 != -1)
                adsUrl.removeAt(i4);
        }


        //add comment_service_ajax? to blacklist urls
        if(settings.value("comments",true).toBool() == false)
        {
            if(!adsUrl.contains("comment_service_ajax?",Qt::CaseInsensitive))
                adsUrl.append("comment_service_ajax?");
        }else{

            int index = adsUrl.lastIndexOf("comment_service_ajax?");
            if(index != -1)
                adsUrl.removeAt(index);
        }


        bool shouldBlock = false;

        adsUrl.removeDuplicates();

        foreach (QString adUrl, adsUrl)
        {
            if(reqUrlStr.contains(adUrl,Qt::CaseInsensitive))
            {
                //qWarning()<<"BLOCKED DUE TO RULE:"<<adUrl;
                shouldBlock = true;
                break;
            }
        }

        //prevent blocking signIns.
        if(reqUrlStr.contains("/accounts")||reqUrlStr.contains("accounts.youtube")
                || reqUrlStr.contains("signin=")){
            shouldBlock = false;
        }

        if (shouldBlock)
        {
            //qWarning() << "blocking" << info.requestUrl();
            emit blocked(reqUrlStr);
            info.block(true);
        }else{
            //qDebug()<<"NOT BLOCKED"<<reqUrlStr;
        }

    }
private:
    QFile adFile;
    QStringList adsUrl, adsUrlStore;

};

#endif // WEBENGINEPLAYER_H
