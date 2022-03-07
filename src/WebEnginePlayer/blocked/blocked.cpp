#include "blocked.h"
#include "ui_blocked.h"
#include <QTimer>
#include <QScrollBar>

Blocked::Blocked(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Blocked)
{
    ui->setupUi(this);

    ui->indicator_ads->setStyleSheet("background:#ea0000");
    ui->indicator_tracker->setStyleSheet("background:#4682B4");

    int blocked =  settings.value("adblocker_blocker_count",0).toInt();
    ui->blockedCount->setText(QString::number(blocked));

    clear_timer = new QTimer(this);
    connect(clear_timer, &QTimer::timeout, this, QOverload<>::of(&Blocked::on_clearLog_clicked));
    clear_timer->start(1000*120);//every 2 min
}

void Blocked::closeEvent(QCloseEvent *event){

    emit closed();
    QWidget::closeEvent(event);
}

Blocked::~Blocked()
{
    clear_timer->blockSignals(true);
    clear_timer->stop();
    clear_timer->deleteLater();
    delete ui;
}

void Blocked::appendLog(QString logText)
{
    int blocked =  settings.value("adblocker_blocker_count",0).toInt();

    int newBlocked = blocked + 1;

    settings.setValue("adblocker_blocker_count",newBlocked);

    QString colorName = isTracker(logText) ? "#4682B4" : "#ea0000";

    ui->logTextBrowser->append("<p style='color:"+colorName+"'>" + logText + "</p>");

    ui->blockedCount->setText(QString::number(newBlocked));

    if(shouldScrollToBottom())
    {
        ui->logTextBrowser->verticalScrollBar()->setValue(ui->logTextBrowser->verticalScrollBar()->maximum());
    }

}

bool Blocked::shouldScrollToBottom()
{
    bool t = true;
    int currentValue = ui->logTextBrowser->verticalScrollBar()->value();
    if(currentValue < ui->logTextBrowser->verticalScrollBar()->maximum())
    {
        t = false;
    }
    return t;
}

bool Blocked::isTracker(const QString logText)
{
    bool tracker = false;

    if( logText.contains("/log_event?")
        || logText.contains("/ptracking?")
        || logText.contains("api/stats/"))
    tracker = true;

    return tracker;
}

void Blocked::on_clearLog_clicked()
{
    ui->logTextBrowser->clear();
}

void Blocked::on_close_clicked()
{
    this->close();
}
