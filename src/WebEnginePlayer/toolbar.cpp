#include "toolbar.h"
#include "ui_toolbar.h"

ToolBar::ToolBar(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ToolBar)
{
    ui->setupUi(this);
    setStyleSheet("QWidget#"+this->objectName()+
                  "{background-color: qlineargradient(spread:reflect,"
                  " x1:0, y1:1, x2:0, y2:0, stop:0.0447761 rgba(0, 0, 0, 0),"
                  " stop:0.199005 rgba(0, 0, 0, 14), stop:0.348348 rgba(0, 0, 0, 65),"
                  " stop:0.510511 rgba(0, 0, 0, 84), stop:0.651652 rgba(0, 0, 0, 66),"
                  " stop:0.800995 rgba(0, 0, 0, 13), stop:0.945274 rgba(0, 0, 0, 0));"
                  "}");
}

void ToolBar::setEnableLoadPreviousSessionButton(bool enabled)
{
    ui->history->setEnabled(enabled);
}


void ToolBar::setEnableCloseButton(bool enabled)
{
    ui->close->setEnabled(enabled);
}

ToolBar::~ToolBar()
{
    delete ui;
}

void ToolBar::on_home_clicked()
{
    emit goHome();
}

void ToolBar::on_reload_clicked()
{
    emit reload();
}

void ToolBar::on_forward_clicked()
{
    emit navigateWebviewFoward();
}

void ToolBar::on_back_clicked()
{
     emit navigateWebviewBackward();
}

void ToolBar::updateNavigation(const bool enableForward, const bool enableBackward, const bool enableReload)
{
    ui->back->setEnabled(enableBackward);
    ui->forward->setEnabled(enableForward);

    ui->stop->setEnabled(!enableReload);
    ui->reload->setEnabled(enableReload);
}

void ToolBar::on_stop_clicked()
{
    emit stop();
}

void ToolBar::on_history_clicked()
{
    emit history();
}

void ToolBar::on_close_clicked()
{
    emit loadBlankPage();
}
