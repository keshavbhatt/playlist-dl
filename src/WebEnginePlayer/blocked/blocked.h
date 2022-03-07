#ifndef BLOCKED_H
#define BLOCKED_H

#include <QWidget>
#include <QSettings>

namespace Ui {
class Blocked;
}

class Blocked : public QWidget
{
    Q_OBJECT

signals:
    void closed();

public:
    explicit Blocked(QWidget *parent = nullptr);
    ~Blocked();

public slots:
    void appendLog(QString logText);

protected slots:
    void closeEvent(QCloseEvent *event);
private slots:
    void on_clearLog_clicked();

    void on_close_clicked();

    bool isTracker(const QString logText);
    bool shouldScrollToBottom();
private:
    Ui::Blocked *ui;
    QTimer *clear_timer = nullptr;
    QSettings settings;
};

#endif // BLOCKED_H
