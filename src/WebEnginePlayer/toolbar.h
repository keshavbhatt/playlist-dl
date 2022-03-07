#ifndef TOOLBAR_H
#define TOOLBAR_H

#include <QWidget>

namespace Ui {
class ToolBar;
}

class ToolBar : public QWidget
{
    Q_OBJECT

signals:
    void navigateWebviewFoward();
    void navigateWebviewBackward();
    void goHome();
    void reload();
    void stop();
    void history();
    void loadBlankPage();

public:
    explicit ToolBar(QWidget *parent = nullptr);
    ~ToolBar();

public slots:
    void updateNavigation(const bool enableForward, const bool enableBackward, const bool enableReload);
    void setEnableLoadPreviousSessionButton(bool enabled);
    void setEnableCloseButton(bool enabled);
private slots:
    void on_back_clicked();
    void on_forward_clicked();
    void on_home_clicked();
    void on_reload_clicked();
    void on_stop_clicked();

    void on_history_clicked();

    void on_close_clicked();

private:
    Ui::ToolBar *ui;
};

#endif // TOOLBAR_H
