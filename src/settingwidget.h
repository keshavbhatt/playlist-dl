#ifndef SETTINGWIDGET_H
#define SETTINGWIDGET_H

#include <QWidget>
#include <QSettings>
#include <QStandardPaths>


#include "engine.h"
#include "utils.h"

namespace Ui {
class SettingWidget;
}

class SettingWidget : public QWidget
{
    Q_OBJECT

signals:
    void updateWindowTheme();
    void concurrentDownloadsValueChanged();
    void clearWebengineCache();
    void showBlocked();
    void blockerSettingChanged(const bool blockerDisabled);
    void updatePlayerWebsite();

public:
    explicit SettingWidget(QWidget *parent = nullptr);
    ~SettingWidget();

public slots:
    void refresh();
protected slots:
    void closeEvent(QCloseEvent *event);
private slots:
    void init_engine();

    void on_deleteCache_clicked();

    void on_changeLocation_clicked();

    void on_downloadLocation_textChanged(const QString &arg1);

    void on_concurrent_downloads_valueChanged(int arg1);

    void on_concurrent_downloads_info_clicked();

    void showInfo(QString title, QString message);
    void on_commentsCheckBox_toggled(bool checked);

    void on_adblockCheckBox_toggled(bool checked);

    void on_trackerCheckBox_toggled(bool checked);

    void on_whatAreTrackerBtn_clicked();

    void on_seeBlockedReq_clicked();

    void on_keepRunningHelp_clicked();

    void on_keepPlayer_toggled(bool checked);

    void on_lightRb_toggled(bool checked);

    void on_darkRb_toggled(bool checked);

    void on_mobileRb_toggled(bool checked);

    void on_desktopRb_toggled(bool checked);
private:
    Ui::SettingWidget *ui;
    Engine *engine = nullptr;
    QSettings settings;
};

#endif // SETTINGWIDGET_H
