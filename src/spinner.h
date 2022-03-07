#ifndef SPINNER_H
#define SPINNER_H

#include <QWidget>
#include "waitingspinnerwidget.h"

namespace Ui {
class Spinner;
}

class Spinner : public QWidget
{
    Q_OBJECT

public:
    explicit Spinner(QWidget *parent = nullptr);
    ~Spinner();

public slots:
    void stop();
    void start();
    void setSpinnerSize(QSize size);

private:
    Ui::Spinner *ui;
    WaitingSpinnerWidget *_loader = nullptr;
};

#endif // SPINNER_H
